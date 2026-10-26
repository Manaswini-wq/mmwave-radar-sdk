/**
 * @file calibration_mgr.c
 * @brief Calibration Manager implementation for mmWave Radar SDK
 * @details Orchestrates auto-calibration sequences for TX power, RX gain/phase,
 *          and LO leakage. Monitors temperature for recalibration triggers.
 *          Provides flash-based cal data store/restore for boot-time optimization.
 */
#include "calibration_mgr.h"
#include "hal_spi.h"
#include "hal_interrupt.h"
#include "mmwave_error_codes.h"
#include "mmwave_config.h"

#include <string.h>

/* ---- Radar calibration SPI register addresses ---- */
#define CAL_REG_TX_POWER_CTRL       0x0300U
#define CAL_REG_TX_POWER_STATUS     0x0304U
#define CAL_REG_RX_GAIN_CTRL        0x0310U
#define CAL_REG_RX_PHASE_CTRL       0x0314U
#define CAL_REG_RX_CAL_STATUS       0x0318U
#define CAL_REG_LO_LEAK_CTRL        0x0320U
#define CAL_REG_LO_LEAK_STATUS      0x0324U
#define CAL_REG_TEMP_SENSOR         0x0330U
#define CAL_REG_CAL_TRIGGER         0x0340U
#define CAL_REG_CAL_STATUS_REG      0x0344U

/* Cal trigger command bits */
#define CAL_TRIG_TX_POWER           (1U << 0)
#define CAL_TRIG_RX_GAIN_PHASE      (1U << 1)
#define CAL_TRIG_LO_LEAKAGE         (1U << 2)
#define CAL_TRIG_ALL                (CAL_TRIG_TX_POWER | CAL_TRIG_RX_GAIN_PHASE | CAL_TRIG_LO_LEAKAGE)

/* Cal status register bits */
#define CAL_STAT_TX_DONE            (1U << 0)
#define CAL_STAT_RX_DONE            (1U << 1)
#define CAL_STAT_LO_DONE            (1U << 2)
#define CAL_STAT_ERROR              (1U << 31)

/* Flash addresses for calibration data storage */
#define CAL_FLASH_BASE_ADDR         0x00080000U
#define CAL_FLASH_MAGIC             0xCA1DA7A0U
#define CAL_DATA_MAX_SIZE           256U

/* Polling timeout for cal completion */
#define CAL_POLL_TIMEOUT            500000U

/* ---- Calibration data stored in flash ---- */
typedef struct {
    uint32  magic;
    uint32  version;
    float32 tx_power_correction[3];         /* Per TX channel */
    float32 rx_gain_correction[4];          /* Per RX channel */
    float32 rx_phase_correction_deg[4];     /* Per RX channel */
    float32 lo_leakage_i;
    float32 lo_leakage_q;
    float32 cal_temperature_c;
    uint32  crc;
} cal_data_t;

/* ---- Internal context ---- */
typedef struct {
    bool            initialized;
    cal_status_t    status;
    cal_data_t      cal_data;
    float32         current_temp_c;
    float32         last_cal_temp_c;
    float32         recal_threshold_c;
} cal_context_t;

static cal_context_t g_cal;

/* ---- SPI helpers (reuse pattern from radar_config_mgr) ---- */

static int32 cal_spi_write(uint16 reg_addr, uint32 value)
{
    uint8 tx_buf[6];
    tx_buf[0] = 0x80U | (uint8)((reg_addr >> 8) & 0x7FU);
    tx_buf[1] = (uint8)(reg_addr & 0xFFU);
    tx_buf[2] = (uint8)((value >> 24) & 0xFFU);
    tx_buf[3] = (uint8)((value >> 16) & 0xFFU);
    tx_buf[4] = (uint8)((value >> 8) & 0xFFU);
    tx_buf[5] = (uint8)(value & 0xFFU);

    hal_spi_chip_select(HAL_SPI_INSTANCE_0, HAL_SPI_CS_0, true);
    int32 ret = hal_spi_transfer(HAL_SPI_INSTANCE_0, tx_buf, NULL, 6U);
    hal_spi_chip_select(HAL_SPI_INSTANCE_0, HAL_SPI_CS_0, false);
    return ret;
}

static int32 cal_spi_read(uint16 reg_addr, uint32* value)
{
    uint8 tx_buf[6] = {0};
    uint8 rx_buf[6] = {0};
    tx_buf[0] = (uint8)((reg_addr >> 8) & 0x7FU);
    tx_buf[1] = (uint8)(reg_addr & 0xFFU);

    hal_spi_chip_select(HAL_SPI_INSTANCE_0, HAL_SPI_CS_0, true);
    int32 ret = hal_spi_transfer(HAL_SPI_INSTANCE_0, tx_buf, rx_buf, 6U);
    hal_spi_chip_select(HAL_SPI_INSTANCE_0, HAL_SPI_CS_0, false);

    if (ret == MMWAVE_OK && value != NULL) {
        *value = ((uint32)rx_buf[2] << 24) | ((uint32)rx_buf[3] << 16) |
                 ((uint32)rx_buf[4] << 8)  | ((uint32)rx_buf[5]);
    }
    return ret;
}

/* ---- Temperature readout ---- */

static float32 read_temperature(void)
{
    uint32 raw = 0U;
    if (cal_spi_read(CAL_REG_TEMP_SENSOR, &raw) != MMWAVE_OK) {
        return -999.0f;  /* Sentinel for read failure */
    }
    /* Raw value is in 0.1 degree C increments, signed 16-bit in lower half */
    int16 raw_signed = (int16)(raw & 0xFFFFU);
    return (float32)raw_signed / 10.0f;
}

/* ---- Wait for calibration completion ---- */

static int32 wait_cal_done(uint32 expected_bits)
{
    uint32 timeout = CAL_POLL_TIMEOUT;
    uint32 status = 0U;

    while (timeout > 0U) {
        int32 ret = cal_spi_read(CAL_REG_CAL_STATUS_REG, &status);
        if (ret != MMWAVE_OK) {
            return ret;
        }

        if (status & CAL_STAT_ERROR) {
            return MMWAVE_ERR_CAL_TIMEOUT;
        }

        if ((status & expected_bits) == expected_bits) {
            return MMWAVE_OK;
        }

        timeout--;
    }

    return MMWAVE_ERR_CAL_TIMEOUT;
}

/* ---- Public API ---- */

int32 calibration_mgr_init(cal_handle_t* handle)
{
    if (handle == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    memset(&g_cal, 0, sizeof(cal_context_t));
    g_cal.status.status = CAL_STATUS_IDLE;
    g_cal.recal_threshold_c = 10.0f;  /* Default */

    g_cal.initialized = true;
    *handle = (cal_handle_t)&g_cal;

    return MMWAVE_OK;
}

int32 calibration_mgr_run(cal_handle_t handle, const mmwave_cal_config_t* config)
{
    cal_context_t* ctx = (cal_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (config == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    ctx->status.status = CAL_STATUS_IN_PROGRESS;
    ctx->recal_threshold_c = config->temp_recal_threshold_c;

    /* Read current temperature */
    ctx->current_temp_c = read_temperature();

    /* Check if recalibration is needed based on temperature delta */
    if (config->enable_temp_compensation && ctx->status.cal_count > 0U) {
        float32 temp_delta = ctx->current_temp_c - ctx->last_cal_temp_c;
        if (temp_delta < 0.0f) {
            temp_delta = -temp_delta;
        }
        if (temp_delta < ctx->recal_threshold_c) {
            /* Temperature hasn't changed enough, skip recal */
            ctx->status.status = CAL_STATUS_COMPLETE;
            return MMWAVE_OK;
        }
    }

    int32 ret;
    uint32 trig_bits = 0U;
    uint32 expected_done = 0U;

    /* ---- TX Power Calibration ---- */
    if (config->enable_tx_power_cal) {
        trig_bits |= CAL_TRIG_TX_POWER;
        expected_done |= CAL_STAT_TX_DONE;
    }

    /* ---- RX Gain/Phase Calibration ---- */
    if (config->enable_rx_gain_phase_cal) {
        trig_bits |= CAL_TRIG_RX_GAIN_PHASE;
        expected_done |= CAL_STAT_RX_DONE;
    }

    /* ---- LO Leakage Calibration ---- */
    if (config->enable_lo_leakage_cal) {
        trig_bits |= CAL_TRIG_LO_LEAKAGE;
        expected_done |= CAL_STAT_LO_DONE;
    }

    if (trig_bits == 0U) {
        /* Nothing to calibrate */
        ctx->status.status = CAL_STATUS_COMPLETE;
        return MMWAVE_OK;
    }

    /* Trigger the calibration sequence */
    ret = cal_spi_write(CAL_REG_CAL_TRIGGER, trig_bits);
    if (ret != MMWAVE_OK) {
        ctx->status.status = CAL_STATUS_FAILED;
        return ret;
    }

    /* Wait for completion */
    ret = wait_cal_done(expected_done);
    if (ret != MMWAVE_OK) {
        ctx->status.status = CAL_STATUS_FAILED;
        return ret;
    }

    /* Read back calibration results */
    if (config->enable_tx_power_cal) {
        uint32 tx_result = 0U;
        cal_spi_read(CAL_REG_TX_POWER_STATUS, &tx_result);
        /* Store per-channel TX power corrections */
        ctx->cal_data.tx_power_correction[0] = (float32)((tx_result >>  0) & 0xFFU) * 0.1f;
        ctx->cal_data.tx_power_correction[1] = (float32)((tx_result >>  8) & 0xFFU) * 0.1f;
        ctx->cal_data.tx_power_correction[2] = (float32)((tx_result >> 16) & 0xFFU) * 0.1f;
        ctx->status.tx_power_cal_done = true;
    }

    if (config->enable_rx_gain_phase_cal) {
        uint32 rx_result = 0U;
        cal_spi_read(CAL_REG_RX_CAL_STATUS, &rx_result);
        /* Parse gain and phase corrections from packed register */
        for (uint32 ch = 0U; ch < 4U; ch++) {
            uint8 gain_raw = (uint8)((rx_result >> (ch * 8U)) & 0xFFU);
            ctx->cal_data.rx_gain_correction[ch] = (float32)gain_raw * 0.5f;
        }
        /* Phase corrections from a separate read */
        uint32 phase_result = 0U;
        cal_spi_read(CAL_REG_RX_PHASE_CTRL, &phase_result);
        for (uint32 ch = 0U; ch < 4U; ch++) {
            uint8 phase_raw = (uint8)((phase_result >> (ch * 8U)) & 0xFFU);
            ctx->cal_data.rx_phase_correction_deg[ch] = (float32)phase_raw * 1.4f;
        }
        ctx->status.rx_gain_phase_cal_done = true;
    }

    if (config->enable_lo_leakage_cal) {
        uint32 lo_result = 0U;
        cal_spi_read(CAL_REG_LO_LEAK_STATUS, &lo_result);
        ctx->cal_data.lo_leakage_i = (float32)((int16)(lo_result >> 16)) / 32768.0f;
        ctx->cal_data.lo_leakage_q = (float32)((int16)(lo_result & 0xFFFFU)) / 32768.0f;
        ctx->status.lo_leakage_cal_done = true;
    }

    /* Record calibration metadata */
    ctx->last_cal_temp_c = ctx->current_temp_c;
    ctx->cal_data.cal_temperature_c = ctx->current_temp_c;
    ctx->status.last_cal_temperature_c = ctx->current_temp_c;
    ctx->status.cal_count++;
    ctx->status.status = CAL_STATUS_COMPLETE;

    return MMWAVE_OK;
}

int32 calibration_mgr_get_status(cal_handle_t handle, cal_status_t* status)
{
    cal_context_t* ctx = (cal_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (status == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    memcpy(status, &ctx->status, sizeof(cal_status_t));
    return MMWAVE_OK;
}

int32 calibration_mgr_store(cal_handle_t handle)
{
    cal_context_t* ctx = (cal_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }

    /* Prepare cal data header */
    ctx->cal_data.magic   = CAL_FLASH_MAGIC;
    ctx->cal_data.version = 1U;

    /* Compute simple checksum CRC */
    uint32 crc = 0U;
    const uint8* data_bytes = (const uint8*)&ctx->cal_data;
    uint32 crc_offset = (uint32)((uintptr_t)&ctx->cal_data.crc - (uintptr_t)&ctx->cal_data);
    for (uint32 i = 0U; i < crc_offset; i++) {
        crc += (uint32)data_bytes[i];
        crc = (crc << 1) | (crc >> 31);  /* Rotate left */
    }
    ctx->cal_data.crc = crc;

    /*
     * Write to flash: In a real implementation this would call the flash
     * driver. Here we simulate by writing to the memory-mapped flash address.
     */
    volatile uint8* flash_ptr = (volatile uint8*)CAL_FLASH_BASE_ADDR;
    const uint8* src = (const uint8*)&ctx->cal_data;
    for (uint32 i = 0U; i < sizeof(cal_data_t); i++) {
        flash_ptr[i] = src[i];
    }

    return MMWAVE_OK;
}

int32 calibration_mgr_restore(cal_handle_t handle)
{
    cal_context_t* ctx = (cal_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }

    /* Read from flash */
    const volatile uint8* flash_ptr = (const volatile uint8*)CAL_FLASH_BASE_ADDR;
    uint8* dst = (uint8*)&ctx->cal_data;
    for (uint32 i = 0U; i < sizeof(cal_data_t); i++) {
        dst[i] = flash_ptr[i];
    }

    /* Validate magic */
    if (ctx->cal_data.magic != CAL_FLASH_MAGIC) {
        memset(&ctx->cal_data, 0, sizeof(cal_data_t));
        return MMWAVE_ERR_CAL_DATA_INVALID;
    }

    /* Validate CRC */
    uint32 crc = 0U;
    const uint8* data_bytes = (const uint8*)&ctx->cal_data;
    uint32 crc_offset = (uint32)((uintptr_t)&ctx->cal_data.crc - (uintptr_t)&ctx->cal_data);
    for (uint32 i = 0U; i < crc_offset; i++) {
        crc += (uint32)data_bytes[i];
        crc = (crc << 1) | (crc >> 31);
    }

    if (crc != ctx->cal_data.crc) {
        memset(&ctx->cal_data, 0, sizeof(cal_data_t));
        return MMWAVE_ERR_CAL_DATA_INVALID;
    }

    /* Mark calibrations as restored */
    ctx->status.tx_power_cal_done      = true;
    ctx->status.rx_gain_phase_cal_done = true;
    ctx->status.lo_leakage_cal_done    = true;
    ctx->last_cal_temp_c = ctx->cal_data.cal_temperature_c;
    ctx->status.last_cal_temperature_c = ctx->cal_data.cal_temperature_c;
    ctx->status.status = CAL_STATUS_COMPLETE;

    return MMWAVE_OK;
}

int32 calibration_mgr_deinit(cal_handle_t handle)
{
    cal_context_t* ctx = (cal_context_t*)handle;

    if (ctx == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    memset(ctx, 0, sizeof(cal_context_t));
    return MMWAVE_OK;
}
