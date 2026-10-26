/**
 * @file data_acquisition_api.c
 * @brief High-speed DMA-based data acquisition for mmWave Radar SDK
 * @details Implements ping-pong buffer management with interrupt-driven
 *          ADC capture, frame assembly from multiple chirps, and CRC
 *          validation on captured radar data.
 */
#include "data_acquisition_api.h"
#include "mmwave_error_codes.h"
#include "hal_dma.h"
#include "hal_interrupt.h"
#include "error_handler.h"

#include <string.h>

/** Ping-pong buffer indices */
#define PING_BUF_IDX    0U
#define PONG_BUF_IDX    1U
#define NUM_PING_PONG   2U

/** Maximum ADC samples per chirp per channel */
#define MAX_ADC_SAMPLES_PER_CHIRP   1024U

/** CRC-32 polynomial (IEEE 802.3) */
#define CRC32_POLY  0xEDB88320U

/** Internal acquisition state */
typedef enum {
    DAQ_STATE_IDLE = 0,
    DAQ_STATE_ARMED,
    DAQ_STATE_CAPTURING,
    DAQ_STATE_FRAME_READY
} daq_state_t;

/** Data acquisition context */
typedef struct {
    bool            initialized;
    daq_state_t     state;

    /* Ping-pong DMA buffers */
    int16*          pp_buffers[NUM_PING_PONG];
    uint32          buf_size_bytes;
    uint8           active_buf;         /**< Index of buffer being filled by DMA */

    /* Frame assembly */
    int16*          frame_buffer;       /**< Assembled frame output buffer */
    uint32          frame_buf_size;
    uint16          chirps_collected;
    uint16          chirps_per_frame;
    uint16          num_rx_channels;
    uint16          adc_samples_per_chirp;

    /* Status counters */
    uint32          frame_count;
    uint32          overflow_count;
    volatile bool   frame_available;
    mmwave_frame_t  latest_frame;
} daq_context_t;

/* ---- Forward declarations ---- */
static void     daq_dma_complete_isr(void* arg);
static uint32   daq_compute_crc32(const void* data, uint32 length);
static int32    daq_assemble_frame(daq_context_t* ctx);

int32 data_acq_init(data_acq_handle_t* handle, uint32 buffer_size)
{
    if (handle == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (buffer_size == 0U || buffer_size > (MAX_ADC_SAMPLES_PER_CHIRP * MMWAVE_MAX_RX_CHANNELS * sizeof(int16))) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    static daq_context_t g_daq;
    memset(&g_daq, 0, sizeof(daq_context_t));

    /* Allocate ping-pong ADC DMA buffers – aligned for DMA burst transfers */
    for (uint8 i = 0; i < NUM_PING_PONG; i++) {
        g_daq.pp_buffers[i] = (int16*)hal_dma_alloc_aligned(buffer_size, 64U);
        if (g_daq.pp_buffers[i] == NULL) {
            return MMWAVE_ERR_OUT_OF_MEMORY;
        }
        memset(g_daq.pp_buffers[i], 0, buffer_size);
    }

    /* Allocate full-frame assembly buffer (worst case: 256 chirps * 4 RX * 1024 samples) */
    g_daq.frame_buf_size = MMWAVE_MAX_CHIRPS_PER_FRAME *
                           MMWAVE_MAX_RX_CHANNELS *
                           MAX_ADC_SAMPLES_PER_CHIRP * sizeof(int16);
    g_daq.frame_buffer = (int16*)hal_dma_alloc_aligned(g_daq.frame_buf_size, 64U);
    if (g_daq.frame_buffer == NULL) {
        return MMWAVE_ERR_OUT_OF_MEMORY;
    }

    g_daq.buf_size_bytes  = buffer_size;
    g_daq.active_buf      = PING_BUF_IDX;
    g_daq.initialized     = true;
    g_daq.state           = DAQ_STATE_IDLE;
    g_daq.frame_available = false;

    *handle = (data_acq_handle_t)&g_daq;
    return MMWAVE_OK;
}

int32 data_acq_start(data_acq_handle_t handle)
{
    daq_context_t* ctx = (daq_context_t*)handle;
    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (ctx->state == DAQ_STATE_CAPTURING) {
        return MMWAVE_ERR_ALREADY_RUNNING;
    }

    ctx->active_buf       = PING_BUF_IDX;
    ctx->chirps_collected  = 0U;
    ctx->frame_count       = 0U;
    ctx->overflow_count    = 0U;
    ctx->frame_available   = false;

    /* Register DMA completion ISR for ADC channel */
    hal_interrupt_register(IRQ_DMA_ADC_DONE, daq_dma_complete_isr, ctx);

    /* Program DMA for first ping buffer */
    hal_dma_config_t dma_cfg;
    dma_cfg.src_addr  = (uint32)HAL_ADC_DATA_REG;
    dma_cfg.dst_addr  = (uint32)ctx->pp_buffers[PING_BUF_IDX];
    dma_cfg.xfer_size = ctx->buf_size_bytes;
    dma_cfg.mode      = HAL_DMA_MODE_PING_PONG;
    hal_dma_configure(HAL_DMA_CH_ADC, &dma_cfg);
    hal_dma_enable(HAL_DMA_CH_ADC);

    ctx->state = DAQ_STATE_CAPTURING;
    return MMWAVE_OK;
}

int32 data_acq_stop(data_acq_handle_t handle)
{
    daq_context_t* ctx = (daq_context_t*)handle;
    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }

    hal_dma_disable(HAL_DMA_CH_ADC);
    hal_interrupt_unregister(IRQ_DMA_ADC_DONE);
    ctx->state = DAQ_STATE_IDLE;
    return MMWAVE_OK;
}

int32 data_acq_get_frame(data_acq_handle_t handle, mmwave_frame_t* frame, uint32 timeout_ms)
{
    daq_context_t* ctx = (daq_context_t*)handle;
    if (ctx == NULL || frame == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (ctx->state != DAQ_STATE_CAPTURING && ctx->state != DAQ_STATE_FRAME_READY) {
        return MMWAVE_ERR_NOT_RUNNING;
    }

    /* Blocking wait with timeout */
    uint32 elapsed = 0U;
    while (!ctx->frame_available) {
        if (timeout_ms != MMWAVE_WAIT_FOREVER && elapsed >= timeout_ms) {
            return MMWAVE_ERR_TIMEOUT;
        }
        hal_delay_us(100U);
        elapsed += 1U; /* approximate 0.1 ms increments */
    }

    memcpy(frame, &ctx->latest_frame, sizeof(mmwave_frame_t));
    ctx->frame_available = false;
    return MMWAVE_OK;
}

int32 data_acq_get_latest_frame(data_acq_handle_t handle, mmwave_frame_t* frame)
{
    daq_context_t* ctx = (daq_context_t*)handle;
    if (ctx == NULL || frame == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (!ctx->frame_available) {
        return MMWAVE_ERR_DATA_UNDERFLOW;
    }

    memcpy(frame, &ctx->latest_frame, sizeof(mmwave_frame_t));
    ctx->frame_available = false;
    return MMWAVE_OK;
}

int32 data_acq_deinit(data_acq_handle_t handle)
{
    daq_context_t* ctx = (daq_context_t*)handle;
    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (ctx->state == DAQ_STATE_CAPTURING) {
        data_acq_stop(handle);
    }

    for (uint8 i = 0; i < NUM_PING_PONG; i++) {
        hal_dma_free(ctx->pp_buffers[i]);
        ctx->pp_buffers[i] = NULL;
    }
    hal_dma_free(ctx->frame_buffer);
    ctx->frame_buffer  = NULL;
    ctx->initialized   = false;
    return MMWAVE_OK;
}

/* ---- ISR: DMA transfer complete (one chirp captured) ---- */
static void daq_dma_complete_isr(void* arg)
{
    daq_context_t* ctx = (daq_context_t*)arg;

    /* Copy finished buffer into frame assembly area */
    uint32 offset = (uint32)ctx->chirps_collected * ctx->buf_size_bytes;
    if (offset + ctx->buf_size_bytes > ctx->frame_buf_size) {
        ctx->overflow_count++;
        error_handler_report(MMWAVE_ERR_BUFFER_OVERFLOW, "DAQ frame buffer overflow");
        return;
    }
    memcpy((uint8*)ctx->frame_buffer + offset,
           ctx->pp_buffers[ctx->active_buf],
           ctx->buf_size_bytes);

    /* Swap ping-pong buffer for next DMA */
    ctx->active_buf ^= 1U;
    hal_dma_set_dst(HAL_DMA_CH_ADC, (uint32)ctx->pp_buffers[ctx->active_buf]);
    hal_dma_enable(HAL_DMA_CH_ADC);

    ctx->chirps_collected++;

    /* Check if full frame is assembled */
    if (ctx->chirps_collected >= ctx->chirps_per_frame) {
        daq_assemble_frame(ctx);
        ctx->chirps_collected = 0U;
    }
}

/* ---- Assemble a complete frame with CRC ---- */
static int32 daq_assemble_frame(daq_context_t* ctx)
{
    uint32 total_bytes = (uint32)ctx->chirps_per_frame * ctx->buf_size_bytes;
    uint32 crc = daq_compute_crc32(ctx->frame_buffer, total_bytes);

    ctx->latest_frame.frame_number     = ctx->frame_count++;
    ctx->latest_frame.timestamp_us     = hal_get_timestamp_us();
    ctx->latest_frame.num_chirps       = ctx->chirps_per_frame;
    ctx->latest_frame.num_rx_channels  = ctx->num_rx_channels;
    ctx->latest_frame.num_adc_samples  = ctx->adc_samples_per_chirp;
    ctx->latest_frame.adc_data         = ctx->frame_buffer;
    ctx->latest_frame.adc_data_size    = total_bytes;
    ctx->latest_frame.crc              = crc;

    ctx->frame_available = true;
    ctx->state = DAQ_STATE_FRAME_READY;
    return MMWAVE_OK;
}

/* ---- CRC-32 (Castagnoli / IEEE) ---- */
static uint32 daq_compute_crc32(const void* data, uint32 length)
{
    const uint8* ptr = (const uint8*)data;
    uint32 crc = 0xFFFFFFFFU;

    for (uint32 i = 0; i < length; i++) {
        crc ^= ptr[i];
        for (uint8 bit = 0; bit < 8; bit++) {
            if (crc & 1U) {
                crc = (crc >> 1) ^ CRC32_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFFU;
}
