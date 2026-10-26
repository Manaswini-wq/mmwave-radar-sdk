/**
 * @file radar_config_mgr.c
 * @brief Radar Configuration Manager implementation for mmWave Radar SDK
 * @details Handles chirp profile creation and validation, chirp timing
 *          calculations (bandwidth, sweep time, IF bandwidth), register
 *          programming via SPI, multi-profile support with runtime switching,
 *          and configuration readback/verification.
 */
#include "radar_config_mgr.h"
#include "profile_manager.h"
#include "hal_spi.h"
#include "mmwave_error_codes.h"
#include "mmwave_config.h"

#include <string.h>

/* Speed of light constant */
#define SPEED_OF_LIGHT_M_S          3.0e8f

/* Radar front-end SPI register addresses (device-specific) */
#define RADAR_REG_PROFILE_CFG_BASE  0x0100U
#define RADAR_REG_CHIRP_START_FREQ  0x00U
#define RADAR_REG_CHIRP_SLOPE       0x04U
#define RADAR_REG_CHIRP_IDLE_TIME   0x08U
#define RADAR_REG_CHIRP_ADC_START   0x0CU
#define RADAR_REG_CHIRP_RAMP_END    0x10U
#define RADAR_REG_CHIRP_ADC_CFG     0x14U
#define RADAR_REG_CHIRP_RX_GAIN     0x18U
#define RADAR_REG_CHIRP_TX_POWER    0x1CU

#define RADAR_REG_FRAME_CFG         0x0200U
#define RADAR_REG_FRAME_CHIRPS      0x00U
#define RADAR_REG_FRAME_COUNT       0x04U
#define RADAR_REG_FRAME_PERIOD      0x08U
#define RADAR_REG_FRAME_CHAN_MASK   0x0CU
#define RADAR_REG_FRAME_PROFILE     0x10U

/* Profile register stride: each profile uses 0x20 bytes */
#define RADAR_PROFILE_REG_STRIDE    0x20U

/* ---- Internal context ---- */
typedef struct {
    bool                    initialized;
    profile_mgr_handle_t    profile_mgr;
    mmwave_frame_config_t   frame_config;
    bool                    frame_configured;
    radar_chirp_timing_t    timing_cache[MMWAVE_MAX_PROFILES];
} radar_config_ctx_t;

static radar_config_ctx_t g_rcfg;

/* ---- SPI register read/write helpers ---- */

static int32 radar_spi_write_reg(uint16 reg_addr, uint32 value)
{
    uint8 tx_buf[6];

    /* SPI write frame: [0x80 | ADDR_HI] [ADDR_LO] [D3] [D2] [D1] [D0] */
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

static int32 radar_spi_read_reg(uint16 reg_addr, uint32* value)
{
    uint8 tx_buf[6] = {0};
    uint8 rx_buf[6] = {0};

    /* SPI read frame: [0x00 | ADDR_HI] [ADDR_LO] [0x00 x4] */
    tx_buf[0] = (uint8)((reg_addr >> 8) & 0x7FU);
    tx_buf[1] = (uint8)(reg_addr & 0xFFU);

    hal_spi_chip_select(HAL_SPI_INSTANCE_0, HAL_SPI_CS_0, true);
    int32 ret = hal_spi_transfer(HAL_SPI_INSTANCE_0, tx_buf, rx_buf, 6U);
    hal_spi_chip_select(HAL_SPI_INSTANCE_0, HAL_SPI_CS_0, false);

    if (ret == MMWAVE_OK && value != NULL) {
        *value = ((uint32)rx_buf[2] << 24) |
                 ((uint32)rx_buf[3] << 16) |
                 ((uint32)rx_buf[4] << 8)  |
                 ((uint32)rx_buf[5]);
    }

    return ret;
}

/* ---- Timing calculation ---- */

static void compute_chirp_timing(const mmwave_chirp_profile_t* profile,
                                 radar_chirp_timing_t* timing)
{
    /* Bandwidth = slope * ramp_end_time / 1000 (MHz/us * us -> GHz) */
    timing->bandwidth_ghz = profile->freq_slope_mhz_us * profile->ramp_end_time_us / 1000.0f;

    /* Active sweep time = ramp_end_time - adc_start_time */
    timing->sweep_time_us = profile->ramp_end_time_us - profile->adc_start_time_us;

    /* IF bandwidth = slope * num_samples / sample_rate (MHz) */
    timing->if_bandwidth_mhz = profile->freq_slope_mhz_us *
                               (float32)profile->adc_samples /
                               (float32)profile->sample_rate_ksps * 1000.0f;

    /* Range resolution = c / (2 * BW) */
    timing->range_resolution_m = SPEED_OF_LIGHT_M_S /
                                 (2.0f * timing->bandwidth_ghz * 1.0e9f);

    /* Max range = sample_rate * c / (2 * slope * 1e6) */
    timing->max_range_m = (float32)profile->sample_rate_ksps * 1000.0f *
                          SPEED_OF_LIGHT_M_S /
                          (2.0f * profile->freq_slope_mhz_us * 1.0e12f);

    /* Total chirp duration = idle_time + ramp_end_time */
    timing->chirp_duration_us = profile->idle_time_us + profile->ramp_end_time_us;
}

/* ---- Program profile registers over SPI ---- */

static int32 program_chirp_profile(const mmwave_chirp_profile_t* profile)
{
    uint16 base = (uint16)(RADAR_REG_PROFILE_CFG_BASE +
                           profile->profile_id * RADAR_PROFILE_REG_STRIDE);
    int32 ret;

    /* Convert start_freq to fixed-point: GHz * 2^26 / 3.6 (synth resolution) */
    uint32 freq_val = (uint32)(profile->start_freq_ghz * 67108864.0f / 3.6f);
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_START_FREQ, freq_val);
    if (ret != MMWAVE_OK) return ret;

    /* Slope: MHz/us * 2^26 / (3.6e3 * 900) fixed-point */
    uint32 slope_val = (uint32)(profile->freq_slope_mhz_us * 20724.0f);
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_SLOPE, slope_val);
    if (ret != MMWAVE_OK) return ret;

    /* Idle time: us * 10 (100ns resolution) */
    uint32 idle_val = (uint32)(profile->idle_time_us * 10.0f);
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_IDLE_TIME, idle_val);
    if (ret != MMWAVE_OK) return ret;

    /* ADC start time: us * 10 */
    uint32 adc_start_val = (uint32)(profile->adc_start_time_us * 10.0f);
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_ADC_START, adc_start_val);
    if (ret != MMWAVE_OK) return ret;

    /* Ramp end time: us * 10 */
    uint32 ramp_val = (uint32)(profile->ramp_end_time_us * 10.0f);
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_RAMP_END, ramp_val);
    if (ret != MMWAVE_OK) return ret;

    /* ADC config: [samples(15:0)] [sample_rate(15:0)] */
    uint32 adc_cfg = ((uint32)profile->adc_samples << 16) |
                     ((uint32)profile->sample_rate_ksps);
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_ADC_CFG, adc_cfg);
    if (ret != MMWAVE_OK) return ret;

    /* RX gain */
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_RX_GAIN,
                              (uint32)profile->rx_gain_db);
    if (ret != MMWAVE_OK) return ret;

    /* TX power backoff */
    ret = radar_spi_write_reg(base + RADAR_REG_CHIRP_TX_POWER,
                              (uint32)profile->tx_power_backoff_db);

    return ret;
}

static int32 program_frame_config(const mmwave_frame_config_t* config)
{
    int32 ret;

    ret = radar_spi_write_reg(RADAR_REG_FRAME_CFG + RADAR_REG_FRAME_CHIRPS,
                              (uint32)config->chirps_per_frame);
    if (ret != MMWAVE_OK) return ret;

    ret = radar_spi_write_reg(RADAR_REG_FRAME_CFG + RADAR_REG_FRAME_COUNT,
                              (uint32)config->num_frames);
    if (ret != MMWAVE_OK) return ret;

    /* Frame period: ms * 1000 -> us, then * 5 for 200ns resolution */
    uint32 period_val = (uint32)(config->frame_periodicity_ms * 5000.0f);
    ret = radar_spi_write_reg(RADAR_REG_FRAME_CFG + RADAR_REG_FRAME_PERIOD,
                              period_val);
    if (ret != MMWAVE_OK) return ret;

    uint32 chan_mask = ((uint32)config->tx_channel_mask << 8) |
                      ((uint32)config->rx_channel_mask);
    ret = radar_spi_write_reg(RADAR_REG_FRAME_CFG + RADAR_REG_FRAME_CHAN_MASK,
                              chan_mask);
    if (ret != MMWAVE_OK) return ret;

    ret = radar_spi_write_reg(RADAR_REG_FRAME_CFG + RADAR_REG_FRAME_PROFILE,
                              (uint32)config->profile_id);

    return ret;
}

/* ---- Public API ---- */

int32 radar_config_init(radar_config_handle_t* handle)
{
    if (handle == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    memset(&g_rcfg, 0, sizeof(radar_config_ctx_t));

    /* Initialize the profile manager sub-module */
    int32 ret = profile_mgr_init(&g_rcfg.profile_mgr);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    g_rcfg.initialized = true;
    *handle = (radar_config_handle_t)&g_rcfg;

    return MMWAVE_OK;
}

int32 radar_config_set_chirp(radar_config_handle_t handle,
                             const mmwave_chirp_profile_t* profile)
{
    radar_config_ctx_t* ctx = (radar_config_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (profile == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    /* Validate profile against hardware limits */
    int32 ret = profile_mgr_validate(profile);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Store the profile */
    ret = profile_mgr_set(ctx->profile_mgr, profile);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Compute timing parameters */
    compute_chirp_timing(profile, &ctx->timing_cache[profile->profile_id]);

    /* Program hardware registers over SPI */
    ret = program_chirp_profile(profile);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    return MMWAVE_OK;
}

int32 radar_config_set_frame(radar_config_handle_t handle,
                             const mmwave_frame_config_t* config)
{
    radar_config_ctx_t* ctx = (radar_config_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (config == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    /* Validate chirps per frame */
    if (config->chirps_per_frame == 0U ||
        config->chirps_per_frame > MMWAVE_MAX_CHIRPS_PER_FRAME) {
        return MMWAVE_ERR_CFG_INVALID_SAMPLES;
    }

    /* Validate channel masks */
    if (config->tx_channel_mask == 0U || config->rx_channel_mask == 0U) {
        return MMWAVE_ERR_CFG_INVALID_CHANNELS;
    }

    memcpy(&ctx->frame_config, config, sizeof(mmwave_frame_config_t));
    ctx->frame_configured = true;

    /* Activate the selected profile */
    int32 ret = profile_mgr_switch(ctx->profile_mgr, config->profile_id);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Program frame registers over SPI */
    ret = program_frame_config(config);
    return ret;
}

int32 radar_config_get_timing(radar_config_handle_t handle,
                              uint32 profile_id,
                              radar_chirp_timing_t* timing)
{
    radar_config_ctx_t* ctx = (radar_config_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (timing == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (profile_id >= MMWAVE_MAX_PROFILES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    memcpy(timing, &ctx->timing_cache[profile_id], sizeof(radar_chirp_timing_t));
    return MMWAVE_OK;
}

int32 radar_config_verify(radar_config_handle_t handle)
{
    radar_config_ctx_t* ctx = (radar_config_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (!ctx->frame_configured) {
        return MMWAVE_ERR_NOT_OPEN;
    }

    /* Readback the frame configuration registers and compare */
    uint32 readback = 0U;
    int32 ret;

    ret = radar_spi_read_reg(RADAR_REG_FRAME_CFG + RADAR_REG_FRAME_CHIRPS, &readback);
    if (ret != MMWAVE_OK) {
        return ret;
    }
    if (readback != (uint32)ctx->frame_config.chirps_per_frame) {
        return MMWAVE_ERR_CAL_DATA_INVALID;
    }

    ret = radar_spi_read_reg(RADAR_REG_FRAME_CFG + RADAR_REG_FRAME_COUNT, &readback);
    if (ret != MMWAVE_OK) {
        return ret;
    }
    if (readback != (uint32)ctx->frame_config.num_frames) {
        return MMWAVE_ERR_CAL_DATA_INVALID;
    }

    /* Verify the active profile's start frequency */
    uint32 profile_id = ctx->frame_config.profile_id;
    uint16 prof_base = (uint16)(RADAR_REG_PROFILE_CFG_BASE +
                                profile_id * RADAR_PROFILE_REG_STRIDE);

    ret = radar_spi_read_reg(prof_base + RADAR_REG_CHIRP_START_FREQ, &readback);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Verify non-zero (basic sanity) */
    if (readback == 0U) {
        return MMWAVE_ERR_CAL_DATA_INVALID;
    }

    return MMWAVE_OK;
}

int32 radar_config_deinit(radar_config_handle_t handle)
{
    radar_config_ctx_t* ctx = (radar_config_ctx_t*)handle;

    if (ctx == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    if (ctx->initialized) {
        profile_mgr_deinit(ctx->profile_mgr);
        memset(ctx, 0, sizeof(radar_config_ctx_t));
    }

    return MMWAVE_OK;
}
