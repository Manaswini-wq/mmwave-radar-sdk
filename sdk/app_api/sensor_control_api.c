/**
 * @file sensor_control_api.c
 * @brief Sensor control implementation for mmWave Radar SDK
 * @details Power management (on/off/standby), dynamic reconfiguration,
 *          temperature readback, built-in self-test, and version query.
 */
#include "sensor_control_api.h"
#include "mmwave_error_codes.h"
#include "hal_spi.h"
#include "hal_gpio.h"
#include "error_handler.h"

#include <string.h>

/** SPI register addresses on the radar front-end ASIC */
#define REG_POWER_CTRL      0x0010U
#define REG_TEMP_SENSOR     0x0024U
#define REG_PLL_STATUS      0x0030U
#define REG_LO_STATUS       0x0034U
#define REG_TX_STATUS       0x0040U
#define REG_RX_STATUS       0x0044U
#define REG_ADC_STATUS      0x0050U
#define REG_FW_VERSION      0x0100U
#define REG_HW_REVISION     0x0104U
#define REG_ROM_VERSION     0x0108U
#define REG_CHIRP_START     0x0200U
#define REG_CHIRP_SLOPE     0x0204U
#define REG_ADC_CFG         0x0208U
#define REG_SELF_TEST_TRIG  0x0300U
#define REG_SELF_TEST_STAT  0x0304U

/** Power-mode hardware encodings */
#define POWER_VAL_OFF       0x0000U
#define POWER_VAL_STANDBY   0x0001U
#define POWER_VAL_ON        0x0003U

/** Temperature sensor scale: LSB = 1/16 °C, signed 12-bit */
#define TEMP_LSB_TO_CELSIUS (1.0f / 16.0f)

static sensor_power_mode_t g_current_power_mode = SENSOR_POWER_OFF;

/* ---- Internal helpers ---- */
static int32 spi_write_reg(uint16 addr, uint32 value)
{
    return hal_spi_write(HAL_SPI_INSTANCE_0, addr, &value, sizeof(value));
}

static int32 spi_read_reg(uint16 addr, uint32* value)
{
    return hal_spi_read(HAL_SPI_INSTANCE_0, addr, value, sizeof(*value));
}

int32 sensor_set_power_mode(mmwave_handle_t handle, sensor_power_mode_t mode)
{
    if (handle == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    uint32 power_val;
    switch (mode) {
        case SENSOR_POWER_OFF:     power_val = POWER_VAL_OFF;     break;
        case SENSOR_POWER_STANDBY: power_val = POWER_VAL_STANDBY; break;
        case SENSOR_POWER_ON:      power_val = POWER_VAL_ON;      break;
        default:
            return MMWAVE_ERR_INVALID_PARAM;
    }

    int32 ret = spi_write_reg(REG_POWER_CTRL, power_val);
    if (ret != MMWAVE_OK) {
        error_handler_report(ret, "Power mode SPI write failed");
        return ret;
    }

    /* Wait for power rail stabilisation on power-on */
    if (mode == SENSOR_POWER_ON) {
        hal_delay_us(5000U);

        /* Verify PLL is locked after power-on */
        uint32 pll_stat = 0U;
        spi_read_reg(REG_PLL_STATUS, &pll_stat);
        if ((pll_stat & 0x01U) == 0U) {
            error_handler_report(MMWAVE_ERR_PLL_LOCK_FAILED, "PLL failed to lock after power-on");
            return MMWAVE_ERR_PLL_LOCK_FAILED;
        }
    }

    g_current_power_mode = mode;
    return MMWAVE_OK;
}

int32 sensor_get_power_mode(mmwave_handle_t handle, sensor_power_mode_t* mode)
{
    if (handle == NULL || mode == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    *mode = g_current_power_mode;
    return MMWAVE_OK;
}

int32 sensor_dynamic_reconfig(mmwave_handle_t handle,
                              const mmwave_chirp_profile_t* profile)
{
    if (handle == NULL || profile == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (g_current_power_mode != SENSOR_POWER_ON) {
        return MMWAVE_ERR_NOT_RUNNING;
    }

    /* Convert start frequency to fixed-point register value (26.6 format) */
    uint32 start_freq_reg = (uint32)(profile->start_freq_ghz * 64.0f);
    int32 ret = spi_write_reg(REG_CHIRP_START, start_freq_reg);
    if (ret != MMWAVE_OK) return ret;

    /* Convert slope to register format (16.16 fixed-point MHz/us) */
    uint32 slope_reg = (uint32)(profile->freq_slope_mhz_us * 65536.0f);
    ret = spi_write_reg(REG_CHIRP_SLOPE, slope_reg);
    if (ret != MMWAVE_OK) return ret;

    /* ADC config: sample count | sample rate */
    uint32 adc_val = ((uint32)profile->adc_samples << 16) | profile->sample_rate_ksps;
    ret = spi_write_reg(REG_ADC_CFG, adc_val);
    return ret;
}

int32 sensor_read_temperature(mmwave_handle_t handle, float32* temp_celsius)
{
    if (handle == NULL || temp_celsius == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (g_current_power_mode == SENSOR_POWER_OFF) {
        return MMWAVE_ERR_NOT_RUNNING;
    }

    uint32 raw = 0U;
    int32 ret = spi_read_reg(REG_TEMP_SENSOR, &raw);
    if (ret != MMWAVE_OK) {
        error_handler_report(MMWAVE_ERR_TEMP_SENSOR_FAULT, "Temp sensor read failed");
        return MMWAVE_ERR_TEMP_SENSOR_FAULT;
    }

    /* Sign-extend 12-bit reading and convert */
    int16 signed_raw = (int16)(raw & 0x0FFFU);
    if (signed_raw & 0x0800) {
        signed_raw |= (int16)0xF000;
    }
    *temp_celsius = (float32)signed_raw * TEMP_LSB_TO_CELSIUS;
    return MMWAVE_OK;
}

int32 sensor_self_test(mmwave_handle_t handle, sensor_selftest_result_t* result)
{
    if (handle == NULL || result == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    memset(result, 0, sizeof(sensor_selftest_result_t));

    /* Trigger built-in self-test on the radar front-end */
    int32 ret = spi_write_reg(REG_SELF_TEST_TRIG, 0x01U);
    if (ret != MMWAVE_OK) return ret;

    /* Poll for completion (timeout ~50 ms) */
    uint32 status = 0U;
    for (uint32 i = 0; i < 500U; i++) {
        hal_delay_us(100U);
        spi_read_reg(REG_SELF_TEST_STAT, &status);
        if (status & 0x80000000U) break;   /* Done bit */
    }
    if ((status & 0x80000000U) == 0U) {
        return MMWAVE_ERR_TIMEOUT;
    }

    /* Decode individual sub-test results (bits 0-4) */
    result->pll_ok = (status & 0x01U) != 0U;
    result->lo_ok  = (status & 0x02U) != 0U;
    result->tx_ok  = (status & 0x04U) != 0U;
    result->rx_ok  = (status & 0x08U) != 0U;
    result->adc_ok = (status & 0x10U) != 0U;

    result->overall_result =
        (result->pll_ok && result->lo_ok && result->tx_ok &&
         result->rx_ok  && result->adc_ok) ? MMWAVE_OK : MMWAVE_ERR_HW_INIT_FAILED;

    return result->overall_result;
}

int32 sensor_get_version(mmwave_handle_t handle, sensor_version_t* version)
{
    if (handle == NULL || version == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    uint32 fw_raw = 0U, hw_raw = 0U, rom_raw = 0U;
    int32 ret;

    ret = spi_read_reg(REG_FW_VERSION, &fw_raw);
    if (ret != MMWAVE_OK) return ret;

    ret = spi_read_reg(REG_HW_REVISION, &hw_raw);
    if (ret != MMWAVE_OK) return ret;

    ret = spi_read_reg(REG_ROM_VERSION, &rom_raw);
    if (ret != MMWAVE_OK) return ret;

    version->fw_major    = (uint8)((fw_raw >> 16) & 0xFFU);
    version->fw_minor    = (uint8)((fw_raw >>  8) & 0xFFU);
    version->fw_patch    = (uint8)( fw_raw        & 0xFFU);
    version->hw_revision = (uint8)( hw_raw        & 0xFFU);
    version->rom_version = rom_raw;

    return MMWAVE_OK;
}
