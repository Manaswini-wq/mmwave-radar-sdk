/**
 * @file vital_signs_monitor.c
 * @brief Vital Signs Monitoring Algorithm Implementation
 * @details Pipeline:
 *          1. Extract phase from target range bin (atan2 of I/Q)
 *          2. Accumulate phase history
 *          3. Bandpass filter for breathing (0.1-0.5 Hz) and heart (0.8-2.0 Hz)
 *          4. Peak counting for rate estimation
 *          5. Moving average smoothing of rate outputs
 */

#include <string.h>
#include <math.h>

#include "vital_signs_monitor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* -------------------------------------------------------------------------- */
/*  Internal Helpers                                                          */
/* -------------------------------------------------------------------------- */

/** Extract phase angle from I/Q sample pair at a given range bin */
static float32 extract_phase(const mmwave_frame_t* frame, uint16 range_bin)
{
    /* ADC data is interleaved I/Q: [I0, Q0, I1, Q1, ...] */
    int16 i_val = frame->adc_data[2 * range_bin];
    int16 q_val = frame->adc_data[2 * range_bin + 1];
    return atan2f((float32)q_val, (float32)i_val);
}

/** Phase unwrapping: keep phase difference within [-pi, pi] */
static float32 unwrap_phase(float32 current, float32 previous)
{
    float32 diff = current - previous;
    while (diff >  M_PI) { diff -= 2.0f * M_PI; }
    while (diff < -M_PI) { diff += 2.0f * M_PI; }
    return previous + diff;
}

/**
 * Simple IIR bandpass filter (2nd-order)
 * Coefficients are computed for given frequency band and sample rate.
 * This is a simplified Butterworth approximation.
 */
static void bandpass_filter(const float32* input, float32* output, uint32 len,
                             float32 f_low_hz, float32 f_high_hz, float32 fs_hz)
{
    float32 w_low  = 2.0f * M_PI * f_low_hz / fs_hz;
    float32 w_high = 2.0f * M_PI * f_high_hz / fs_hz;
    float32 bw     = w_high - w_low;
    float32 wc     = (w_high + w_low) / 2.0f;

    /* Simplified 2nd-order bandpass coefficients */
    float32 alpha = sinf(bw) / 2.0f;
    float32 cos_wc = cosf(wc);

    float32 b0 =  alpha;
    float32 b1 =  0.0f;
    float32 b2 = -alpha;
    float32 a0 =  1.0f + alpha;
    float32 a1 = -2.0f * cos_wc;
    float32 a2 =  1.0f - alpha;

    /* Normalize */
    b0 /= a0; b1 /= a0; b2 /= a0;
    a1 /= a0; a2 /= a0;

    float32 x1 = 0.0f, x2 = 0.0f;
    float32 y1 = 0.0f, y2 = 0.0f;

    for (uint32 n = 0; n < len; n++) {
        float32 x0 = input[n];
        output[n] = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x0;
        y2 = y1; y1 = output[n];
    }
}

/**
 * Count zero-crossings (positive slope) to estimate frequency,
 * then convert to BPM.
 */
static float32 count_rate_bpm(const float32* signal, uint32 len, float32 fs_hz,
                               float32* confidence)
{
    uint32 crossings = 0;
    float32 peak = 0.0f;

    for (uint32 i = 1; i < len; i++) {
        if (signal[i - 1] < 0.0f && signal[i] >= 0.0f) {
            crossings++;
        }
        float32 abs_val = fabsf(signal[i]);
        if (abs_val > peak) { peak = abs_val; }
    }

    float32 duration_s = (float32)len / fs_hz;
    float32 freq_hz = (float32)crossings / duration_s;
    float32 bpm = freq_hz * 60.0f;

    /* Confidence: based on signal peak (higher = more confident) */
    *confidence = (peak > 1000.0f) ? 1.0f : peak / 1000.0f;
    if (*confidence > 1.0f) { *confidence = 1.0f; }

    return bpm;
}

/** Moving average for rate smoothing */
static float32 moving_average(const float32* buf, uint16 window)
{
    float32 sum = 0.0f;
    for (uint16 i = 0; i < window; i++) {
        sum += buf[i];
    }
    return sum / (float32)window;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

int32 vital_signs_init(vital_signs_monitor_t* ctx, const vital_signs_config_t* cfg)
{
    if (ctx == NULL || cfg == NULL) { return MMWAVE_ERR_NULL_POINTER; }
    if (cfg->smoothing_window > VS_SMOOTHING_MAX) { return MMWAVE_ERR_INVALID_PARAM; }

    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = *cfg;
    return MMWAVE_OK;
}

int32 vital_signs_process_frame(vital_signs_monitor_t* ctx, const mmwave_frame_t* frame)
{
    if (ctx == NULL || frame == NULL) { return MMWAVE_ERR_NULL_POINTER; }
    if (ctx->cfg.target_range_bin >= frame->num_adc_samples) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    /* 1. Extract and unwrap phase */
    float32 raw_phase = extract_phase(frame, ctx->cfg.target_range_bin);
    if (ctx->sample_count > 0) {
        uint32 prev_idx = (ctx->phase_idx + VS_PHASE_HISTORY_LEN - 1) % VS_PHASE_HISTORY_LEN;
        raw_phase = unwrap_phase(raw_phase, ctx->phase_history[prev_idx]);
    }

    ctx->phase_history[ctx->phase_idx] = raw_phase;
    ctx->phase_idx = (ctx->phase_idx + 1) % VS_PHASE_HISTORY_LEN;
    ctx->sample_count++;

    /* Need sufficient samples before estimating rates */
    uint32 avail = (ctx->sample_count < VS_PHASE_HISTORY_LEN)
                   ? ctx->sample_count : VS_PHASE_HISTORY_LEN;
    if (avail < 64) { return MMWAVE_OK; }

    /* Build contiguous phase buffer for filtering */
    float32 phase_buf[VS_PHASE_HISTORY_LEN];
    float32 breath_buf[VS_PHASE_HISTORY_LEN];
    float32 heart_buf[VS_PHASE_HISTORY_LEN];

    for (uint32 i = 0; i < avail; i++) {
        uint32 idx = (ctx->phase_idx + VS_PHASE_HISTORY_LEN - avail + i)
                     % VS_PHASE_HISTORY_LEN;
        phase_buf[i] = ctx->phase_history[idx];
    }

    /* 2. Bandpass filter for breathing and heart rate */
    float32 fs = ctx->cfg.sample_rate_hz;
    bandpass_filter(phase_buf, breath_buf, avail,
                    ctx->cfg.breath_bpm_min / 60.0f,
                    ctx->cfg.breath_bpm_max / 60.0f, fs);
    bandpass_filter(phase_buf, heart_buf, avail,
                    ctx->cfg.heart_bpm_min / 60.0f,
                    ctx->cfg.heart_bpm_max / 60.0f, fs);

    /* 3. Estimate rates via peak counting */
    float32 breath_conf = 0.0f, heart_conf = 0.0f;
    float32 breath_bpm = count_rate_bpm(breath_buf, avail, fs, &breath_conf);
    float32 heart_bpm  = count_rate_bpm(heart_buf,  avail, fs, &heart_conf);

    /* 4. Moving average smoothing */
    uint16 sw = ctx->cfg.smoothing_window;
    uint16 si = ctx->smooth_idx % sw;
    ctx->breath_rate_buf[si] = breath_bpm;
    ctx->heart_rate_buf[si]  = heart_bpm;
    ctx->smooth_idx++;

    uint16 used = (ctx->smooth_idx < sw) ? (uint16)ctx->smooth_idx : sw;
    ctx->latest.breathing_rate_bpm  = moving_average(ctx->breath_rate_buf, used);
    ctx->latest.heart_rate_bpm      = moving_average(ctx->heart_rate_buf, used);
    ctx->latest.breathing_confidence = breath_conf;
    ctx->latest.heart_confidence     = heart_conf;

    return MMWAVE_OK;
}

int32 vital_signs_get_result(const vital_signs_monitor_t* ctx, vital_signs_result_t* result)
{
    if (ctx == NULL || result == NULL) { return MMWAVE_ERR_NULL_POINTER; }
    *result = ctx->latest;
    return MMWAVE_OK;
}

void vital_signs_deinit(vital_signs_monitor_t* ctx)
{
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(*ctx));
    }
}
