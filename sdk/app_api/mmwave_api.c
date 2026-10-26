/**
 * @file mmwave_api.c
 * @brief Top-level mmWave Radar SDK API implementation
 * @details Provides the easy-to-use interface that abstracts and simplifies
 *          customer usage of TI's mmWave radar sensors. Orchestrates the
 *          real-time radar control firmware across HAL, middleware, and DSP layers.
 */
#include "mmwave_api.h"
#include "mmwave_error_codes.h"
#include "mmwave_config.h"
#include "radar_config_mgr.h"
#include "calibration_mgr.h"
#include "frame_scheduler.h"
#include "data_acquisition_api.h"
#include "error_handler.h"
#include "hal_spi.h"
#include "hal_uart.h"
#include "hal_gpio.h"
#include "hal_dma.h"
#include "hal_interrupt.h"

#include <string.h>

/** Internal SDK context structure */
typedef struct {
    bool                     initialized;
    bool                     opened;
    bool                     running;
    mmwave_config_t          config;
    mmwave_frame_callback_t  frame_cb;
    void*                    frame_cb_data;
    mmwave_error_callback_t  error_cb;
    void*                    error_cb_data;
    mmwave_status_t          status;
    radar_config_handle_t    radar_cfg;
    cal_handle_t             cal_mgr;
    frame_sched_handle_t     frame_sched;
    data_acq_handle_t        data_acq;
    uint32                   frame_counter;
} sdk_context_t;

static sdk_context_t g_sdk_ctx;

/* Forward declarations for internal helpers */
static int32 sdk_validate_config(const mmwave_config_t* config);
static int32 sdk_init_hardware(sdk_context_t* ctx);
static void  sdk_frame_done_isr(void* arg);
static void  sdk_error_isr(void* arg);

int32 mmwave_init(mmwave_handle_t* handle, const mmwave_config_t* config)
{
    if (handle == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    if (g_sdk_ctx.initialized) {
        return MMWAVE_ERR_ALREADY_INITIALIZED;
    }

    memset(&g_sdk_ctx, 0, sizeof(sdk_context_t));

    if (config != NULL) {
        int32 ret = sdk_validate_config(config);
        if (ret != MMWAVE_OK) {
            return ret;
        }
        memcpy(&g_sdk_ctx.config, config, sizeof(mmwave_config_t));
    } else {
        mmwave_config_t default_cfg = MMWAVE_DEFAULT_CONFIG;
        memcpy(&g_sdk_ctx.config, &default_cfg, sizeof(mmwave_config_t));
    }

    /* Initialize error handling subsystem */
    error_handler_init();

    g_sdk_ctx.initialized = true;
    g_sdk_ctx.frame_counter = 0U;
    *handle = (mmwave_handle_t)&g_sdk_ctx;

    return MMWAVE_OK;
}

int32 mmwave_open(mmwave_handle_t handle)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (ctx->opened) {
        return MMWAVE_ERR_ALREADY_OPEN;
    }

    /* Initialize all hardware abstraction layers */
    int32 ret = sdk_init_hardware(ctx);
    if (ret != MMWAVE_OK) {
        error_handler_report(ret, "Hardware initialization failed");
        return ret;
    }

    /* Initialize radar configuration manager */
    ret = radar_config_init(&ctx->radar_cfg);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Initialize calibration manager */
    ret = calibration_mgr_init(&ctx->cal_mgr);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Initialize frame scheduler */
    ret = frame_scheduler_init(&ctx->frame_sched);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Initialize data acquisition */
    ret = data_acq_init(&ctx->data_acq, MMWAVE_ADC_BUFFER_SIZE);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Register interrupts */
    hal_interrupt_register(IRQ_FRAME_DONE, sdk_frame_done_isr, ctx);
    hal_interrupt_register(IRQ_CHIRP_ERR, sdk_error_isr, ctx);

    ctx->opened = true;
    ctx->status.is_open = true;

    return MMWAVE_OK;
}

int32 mmwave_config_chirp(mmwave_handle_t handle, const mmwave_chirp_profile_t* profile)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (!ctx->opened) {
        return MMWAVE_ERR_NOT_OPEN;
    }
    if (profile == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    /* Validate frequency parameters against platform limits */
    float32 bandwidth = profile->freq_slope_mhz_us * profile->ramp_end_time_us / 1000.0f;
    if (bandwidth > MMWAVE_MAX_BANDWIDTH_GHZ) {
        return MMWAVE_ERR_CFG_BANDWIDTH_EXCEED;
    }
    if (profile->adc_samples > 1024U || profile->adc_samples == 0U) {
        return MMWAVE_ERR_CFG_INVALID_SAMPLES;
    }

    memcpy(&ctx->config.chirp_profile, profile, sizeof(mmwave_chirp_profile_t));

    return radar_config_set_chirp(ctx->radar_cfg, profile);
}

int32 mmwave_config_frame(mmwave_handle_t handle, const mmwave_frame_config_t* config)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->opened) {
        return MMWAVE_ERR_NOT_OPEN;
    }
    if (config == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (config->chirps_per_frame == 0U ||
        config->chirps_per_frame > MMWAVE_MAX_CHIRPS_PER_FRAME) {
        return MMWAVE_ERR_CFG_INVALID_SAMPLES;
    }

    memcpy(&ctx->config.frame_config, config, sizeof(mmwave_frame_config_t));

    return radar_config_set_frame(ctx->radar_cfg, config);
}

int32 mmwave_calibrate(mmwave_handle_t handle, const mmwave_cal_config_t* config)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->opened) {
        return MMWAVE_ERR_NOT_OPEN;
    }

    const mmwave_cal_config_t* cal_cfg = (config != NULL) ? config : &ctx->config.cal_config;

    return calibration_mgr_run(ctx->cal_mgr, cal_cfg);
}

int32 mmwave_start(mmwave_handle_t handle)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->opened) {
        return MMWAVE_ERR_NOT_OPEN;
    }
    if (ctx->running) {
        return MMWAVE_ERR_ALREADY_RUNNING;
    }

    /* Run calibration if enabled and not yet done */
    if (ctx->config.cal_config.enable_tx_power_cal ||
        ctx->config.cal_config.enable_rx_gain_phase_cal) {
        int32 ret = mmwave_calibrate(handle, NULL);
        if (ret != MMWAVE_OK) {
            if (ctx->error_cb) {
                ctx->error_cb(ret, "Pre-start calibration failed", ctx->error_cb_data);
            }
            return ret;
        }
    }

    /* Start data acquisition DMA */
    int32 ret = data_acq_start(ctx->data_acq);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    /* Start frame scheduler - this triggers the RF front-end */
    ret = frame_scheduler_start(ctx->frame_sched, &ctx->config.frame_config);
    if (ret != MMWAVE_OK) {
        data_acq_stop(ctx->data_acq);
        return ret;
    }

    ctx->running = true;
    ctx->status.is_running = true;

    return MMWAVE_OK;
}

int32 mmwave_stop(mmwave_handle_t handle)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->running) {
        return MMWAVE_ERR_NOT_RUNNING;
    }

    frame_scheduler_stop(ctx->frame_sched);
    data_acq_stop(ctx->data_acq);

    ctx->running = false;
    ctx->status.is_running = false;

    return MMWAVE_OK;
}

int32 mmwave_close(mmwave_handle_t handle)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->opened) {
        return MMWAVE_ERR_NOT_OPEN;
    }
    if (ctx->running) {
        mmwave_stop(handle);
    }

    data_acq_deinit(ctx->data_acq);
    frame_scheduler_deinit(ctx->frame_sched);
    calibration_mgr_deinit(ctx->cal_mgr);
    radar_config_deinit(ctx->radar_cfg);

    hal_interrupt_unregister(IRQ_FRAME_DONE);
    hal_interrupt_unregister(IRQ_CHIRP_ERR);

    ctx->opened = false;
    ctx->status.is_open = false;

    return MMWAVE_OK;
}

int32 mmwave_deinit(mmwave_handle_t handle)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (ctx->opened) {
        mmwave_close(handle);
    }

    error_handler_deinit();
    memset(ctx, 0, sizeof(sdk_context_t));

    return MMWAVE_OK;
}

int32 mmwave_register_frame_callback(mmwave_handle_t handle,
                                      mmwave_frame_callback_t callback,
                                      void* user_data)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }

    ctx->frame_cb = callback;
    ctx->frame_cb_data = user_data;

    return MMWAVE_OK;
}

int32 mmwave_register_error_callback(mmwave_handle_t handle,
                                      mmwave_error_callback_t callback,
                                      void* user_data)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }

    ctx->error_cb = callback;
    ctx->error_cb_data = user_data;

    return MMWAVE_OK;
}

int32 mmwave_get_status(mmwave_handle_t handle, mmwave_status_t* status)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || status == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    ctx->status.frames_processed = ctx->frame_counter;
    memcpy(status, &ctx->status, sizeof(mmwave_status_t));

    return MMWAVE_OK;
}

int32 mmwave_get_frame(mmwave_handle_t handle, mmwave_frame_t* frame, uint32 timeout_ms)
{
    sdk_context_t* ctx = (sdk_context_t*)handle;

    if (ctx == NULL || !ctx->running) {
        return MMWAVE_ERR_NOT_RUNNING;
    }
    if (frame == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    return data_acq_get_frame(ctx->data_acq, frame, timeout_ms);
}

/* ---- Internal helpers ---- */

static int32 sdk_validate_config(const mmwave_config_t* config)
{
    if (config->chirp_profile.start_freq_ghz < 76.0f ||
        config->chirp_profile.start_freq_ghz > 81.0f) {
        return MMWAVE_ERR_CFG_INVALID_FREQ;
    }
    if (config->chirp_profile.freq_slope_mhz_us <= 0.0f ||
        config->chirp_profile.freq_slope_mhz_us > 100.0f) {
        return MMWAVE_ERR_CFG_INVALID_SLOPE;
    }
    if (config->frame_config.rx_channel_mask == 0U ||
        config->frame_config.tx_channel_mask == 0U) {
        return MMWAVE_ERR_CFG_INVALID_CHANNELS;
    }
    return MMWAVE_OK;
}

static int32 sdk_init_hardware(sdk_context_t* ctx)
{
    int32 ret;

    ret = hal_gpio_init();
    if (ret != MMWAVE_OK) return ret;

    ret = hal_spi_init(HAL_SPI_INSTANCE_0, HAL_SPI_SPEED_10MHZ);
    if (ret != MMWAVE_OK) return ret;

    ret = hal_uart_init(MMWAVE_DEBUG_UART_INSTANCE, MMWAVE_DEBUG_UART_BAUD);
    if (ret != MMWAVE_OK) return ret;

    ret = hal_dma_init();
    if (ret != MMWAVE_OK) return ret;

    ret = hal_interrupt_init();
    if (ret != MMWAVE_OK) return ret;

    (void)ctx;
    return MMWAVE_OK;
}

static void sdk_frame_done_isr(void* arg)
{
    sdk_context_t* ctx = (sdk_context_t*)arg;
    ctx->frame_counter++;

    if (ctx->frame_cb != NULL) {
        mmwave_frame_t frame;
        if (data_acq_get_latest_frame(ctx->data_acq, &frame) == MMWAVE_OK) {
            ctx->frame_cb(&frame, ctx->frame_cb_data);
        }
    }
}

static void sdk_error_isr(void* arg)
{
    sdk_context_t* ctx = (sdk_context_t*)arg;
    ctx->status.error_count++;

    if (ctx->error_cb != NULL) {
        ctx->error_cb(MMWAVE_ERR_RF_FAULT, "Chirp error detected", ctx->error_cb_data);
    }
}

const char* mmwave_strerror(int32_t error_code)
{
    switch (error_code) {
        case MMWAVE_OK:                      return "Success";
        case MMWAVE_ERR_INVALID_PARAM:       return "Invalid parameter";
        case MMWAVE_ERR_NULL_POINTER:        return "Null pointer";
        case MMWAVE_ERR_NOT_INITIALIZED:     return "Not initialized";
        case MMWAVE_ERR_TIMEOUT:             return "Timeout";
        case MMWAVE_ERR_HW_INIT_FAILED:      return "Hardware init failed";
        case MMWAVE_ERR_PLL_LOCK_FAILED:     return "PLL lock failed";
        case MMWAVE_ERR_CAL_TX_POWER:        return "TX power calibration error";
        case MMWAVE_ERR_CAL_RX_GAIN:         return "RX gain calibration error";
        case MMWAVE_ERR_DATA_CRC_MISMATCH:   return "Data CRC mismatch";
        default:                             return "Unknown error";
    }
}
