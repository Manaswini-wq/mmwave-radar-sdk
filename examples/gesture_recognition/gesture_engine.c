/**
 * @file gesture_engine.c
 * @brief Gesture Recognition Engine Implementation
 * @details Pipeline:
 *          1. Compute Doppler profile per frame (sum across target range bins)
 *          2. Accumulate into a rolling micro-Doppler spectrogram
 *          3. Extract features (centroid, spread, energy, asymmetry, etc.)
 *          4. Template-match against known gesture feature signatures
 *          5. Apply debouncing and fire gesture callback
 */

#include <string.h>
#include <math.h>

#include "gesture_engine.h"

/* -------------------------------------------------------------------------- */
/*  Gesture Templates (pre-defined feature vectors)                           */
/* -------------------------------------------------------------------------- */
typedef struct {
    gesture_type_t type;
    float32 features[GE_NUM_FEATURES];
} gesture_template_t;

/**
 * Template features:
 *   [0] energy, [1] centroid, [2] spread, [3] asymmetry,
 *   [4] temporal_slope, [5] peak_doppler, [6] duration, [7] range_delta
 */
static const gesture_template_t g_templates[] = {
    { GESTURE_SWIPE_LEFT,  { 1.0f,  0.4f, 0.3f,  0.6f,  0.2f, 0.5f, 0.6f, 0.0f } },
    { GESTURE_SWIPE_RIGHT, { 1.0f, -0.4f, 0.3f, -0.6f, -0.2f,-0.5f, 0.6f, 0.0f } },
    { GESTURE_PUSH,        { 0.8f,  0.0f, 0.2f,  0.0f,  0.5f, 0.3f, 0.4f, 0.7f } },
    { GESTURE_PULL,        { 0.8f,  0.0f, 0.2f,  0.0f, -0.5f,-0.3f, 0.4f,-0.7f } },
};
#define NUM_TEMPLATES  (sizeof(g_templates) / sizeof(g_templates[0]))

/* -------------------------------------------------------------------------- */
/*  Internal Helpers                                                          */
/* -------------------------------------------------------------------------- */

/** Compute Doppler power profile from ADC data, summed over target range bins */
static void compute_doppler_profile(const mmwave_frame_t* frame,
                                     uint16 min_rbin, uint16 max_rbin,
                                     float32* profile, uint16 num_doppler)
{
    memset(profile, 0, num_doppler * sizeof(float32));

    uint16 samples = frame->num_adc_samples;
    uint16 chirps  = frame->num_chirps;
    uint16 n_dop   = (chirps < num_doppler) ? chirps : num_doppler;
    uint16 rb_end  = (max_rbin < samples) ? max_rbin : samples;

    /* For each chirp (Doppler bin), accumulate energy in the range window */
    for (uint16 c = 0; c < n_dop; c++) {
        float32 sum = 0.0f;
        const int16* chirp_data = &frame->adc_data[c * samples * 2];
        for (uint16 r = min_rbin; r < rb_end; r++) {
            float32 i_val = (float32)chirp_data[2 * r];
            float32 q_val = (float32)chirp_data[2 * r + 1];
            sum += i_val * i_val + q_val * q_val;
        }
        profile[c] = sum;
    }
}

/** Extract feature vector from spectrogram */
static void extract_features(const gesture_engine_t* ctx, float32* features)
{
    memset(features, 0, GE_NUM_FEATURES * sizeof(float32));

    float32 total_energy = 0.0f;
    float32 weighted_freq = 0.0f;
    float32 weighted_time = 0.0f;
    float32 positive_energy = 0.0f;
    float32 negative_energy = 0.0f;
    float32 peak_val = 0.0f;
    float32 peak_dop = 0.0f;
    uint16 half = ctx->cfg.num_doppler_bins / 2;

    for (uint16 t = 0; t < GE_SPECTROGRAM_DEPTH; t++) {
        for (uint16 d = 0; d < ctx->cfg.num_doppler_bins && d < GE_MAX_DOPPLER_BINS; d++) {
            float32 val = ctx->spectrogram[t][d];
            total_energy += val;
            float32 signed_dop = (float32)d - (float32)half;
            weighted_freq += val * signed_dop;
            weighted_time += val * (float32)t;

            if (d < half) { negative_energy += val; }
            else          { positive_energy += val; }

            if (val > peak_val) { peak_val = val; peak_dop = signed_dop; }
        }
    }

    float32 inv_e = 1.0f / (total_energy + 1e-12f);

    features[0] = logf(total_energy + 1.0f) / 20.0f;        /* energy */
    features[1] = weighted_freq * inv_e / (float32)half;      /* centroid */
    features[2] = 0.3f;                                       /* spread (simplified) */
    features[3] = (positive_energy - negative_energy) * inv_e; /* asymmetry */
    features[4] = weighted_time * inv_e / GE_SPECTROGRAM_DEPTH; /* temporal slope */
    features[5] = peak_dop / (float32)half;                    /* peak Doppler */
    features[6] = 0.5f;                                       /* duration (placeholder) */
    features[7] = 0.0f;                                       /* range delta (placeholder) */
}

/** Template matching: cosine similarity between feature and template vectors */
static gesture_type_t classify(const float32* features, float32* out_confidence)
{
    gesture_type_t best = GESTURE_NONE;
    float32 best_sim = -1.0f;

    for (uint32 t = 0; t < NUM_TEMPLATES; t++) {
        float32 dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
        for (uint32 f = 0; f < GE_NUM_FEATURES; f++) {
            dot   += features[f] * g_templates[t].features[f];
            mag_a += features[f] * features[f];
            mag_b += g_templates[t].features[f] * g_templates[t].features[f];
        }
        float32 sim = dot / (sqrtf(mag_a) * sqrtf(mag_b) + 1e-12f);
        if (sim > best_sim) {
            best_sim = sim;
            best = g_templates[t].type;
        }
    }

    *out_confidence = (best_sim > 0.0f) ? best_sim : 0.0f;
    return best;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

int32 gesture_engine_init(gesture_engine_t* ctx, const gesture_engine_config_t* cfg)
{
    if (ctx == NULL || cfg == NULL) { return MMWAVE_ERR_NULL_POINTER; }

    memset(ctx, 0, sizeof(*ctx));
    ctx->cfg = *cfg;
    ctx->frames_since_gesture = cfg->debounce_frames; /* allow immediate first */
    return MMWAVE_OK;
}

int32 gesture_engine_process_frame(gesture_engine_t* ctx, const mmwave_frame_t* frame)
{
    if (ctx == NULL || frame == NULL) { return MMWAVE_ERR_NULL_POINTER; }

    /* 1. Compute Doppler profile and store in rolling spectrogram */
    uint16 idx = ctx->spec_write_idx % GE_SPECTROGRAM_DEPTH;
    compute_doppler_profile(frame, ctx->cfg.min_range_bin, ctx->cfg.max_range_bin,
                            ctx->spectrogram[idx], ctx->cfg.num_doppler_bins);
    ctx->spec_write_idx++;
    ctx->frames_since_gesture++;
    ctx->frame_count++;

    /* Wait until spectrogram buffer is full */
    if (ctx->frame_count < GE_SPECTROGRAM_DEPTH) { return MMWAVE_OK; }

    /* 2. Extract features */
    extract_features(ctx, ctx->features);

    /* 3. Classify */
    float32 confidence = 0.0f;
    gesture_type_t gesture = classify(ctx->features, &confidence);

    /* 4. Debounce and fire callback */
    if (gesture != GESTURE_NONE &&
        confidence >= ctx->cfg.min_confidence &&
        ctx->frames_since_gesture >= ctx->cfg.debounce_frames) {

        ctx->frames_since_gesture = 0;
        if (ctx->callback != NULL) {
            ctx->callback(gesture, confidence, ctx->callback_user_data);
        }
    }

    return MMWAVE_OK;
}

int32 gesture_engine_register_callback(gesture_engine_t* ctx,
                                        gesture_callback_t cb, void* user_data)
{
    if (ctx == NULL) { return MMWAVE_ERR_NULL_POINTER; }
    ctx->callback = cb;
    ctx->callback_user_data = user_data;
    return MMWAVE_OK;
}

void gesture_engine_deinit(gesture_engine_t* ctx)
{
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(*ctx));
    }
}
