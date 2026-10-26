/**
 * @file profile_manager.c
 * @brief Profile Manager implementation for mmWave Radar SDK
 * @details Stores multiple chirp profiles, provides runtime profile switching
 *          with validation against hardware limits. Supports up to
 *          MMWAVE_MAX_PROFILES simultaneous profiles.
 */
#include "profile_manager.h"
#include "mmwave_error_codes.h"
#include "mmwave_config.h"

#include <string.h>

/* ---- Frequency and timing limits ---- */
#define FREQ_MIN_GHZ                76.0f
#define FREQ_MAX_GHZ                81.0f
#define SLOPE_MIN_MHZ_US            0.1f
#define SLOPE_MAX_MHZ_US            100.0f
#define ADC_SAMPLES_MIN             16U
#define ADC_SAMPLES_MAX             1024U
#define RX_GAIN_MAX_DB              54U
#define TX_BACKOFF_MAX_DB           20U
#define IDLE_TIME_MIN_US            2.0f
#define RAMP_END_MIN_US             5.0f

/* ---- Per-profile slot ---- */
typedef struct {
    uint32                  state;
    mmwave_chirp_profile_t  profile;
} profile_slot_t;

/* ---- Internal context ---- */
typedef struct {
    bool            initialized;
    profile_slot_t  slots[MMWAVE_MAX_PROFILES];
    uint32          active_profile_id;
    uint32          num_configured;
} profile_mgr_ctx_t;

static profile_mgr_ctx_t g_pmgr;

/* ---- Public API ---- */

int32 profile_mgr_init(profile_mgr_handle_t* handle)
{
    if (handle == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    memset(&g_pmgr, 0, sizeof(profile_mgr_ctx_t));

    for (uint32 i = 0U; i < MMWAVE_MAX_PROFILES; i++) {
        g_pmgr.slots[i].state = PROFILE_STATE_EMPTY;
    }

    g_pmgr.active_profile_id = 0U;
    g_pmgr.num_configured = 0U;
    g_pmgr.initialized = true;

    *handle = (profile_mgr_handle_t)&g_pmgr;
    return MMWAVE_OK;
}

int32 profile_mgr_set(profile_mgr_handle_t handle,
                      const mmwave_chirp_profile_t* profile)
{
    profile_mgr_ctx_t* ctx = (profile_mgr_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (profile == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (profile->profile_id >= MMWAVE_MAX_PROFILES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    /* Validate before storing */
    int32 ret = profile_mgr_validate(profile);
    if (ret != MMWAVE_OK) {
        return ret;
    }

    uint32 id = profile->profile_id;
    bool was_empty = (ctx->slots[id].state == PROFILE_STATE_EMPTY);

    memcpy(&ctx->slots[id].profile, profile, sizeof(mmwave_chirp_profile_t));
    ctx->slots[id].state = PROFILE_STATE_CONFIGURED;

    if (was_empty) {
        ctx->num_configured++;
    }

    return MMWAVE_OK;
}

int32 profile_mgr_get(profile_mgr_handle_t handle,
                      uint32 profile_id,
                      mmwave_chirp_profile_t* profile)
{
    profile_mgr_ctx_t* ctx = (profile_mgr_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (profile == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (profile_id >= MMWAVE_MAX_PROFILES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (ctx->slots[profile_id].state == PROFILE_STATE_EMPTY) {
        return MMWAVE_ERR_NOT_OPEN;  /* Profile not configured */
    }

    memcpy(profile, &ctx->slots[profile_id].profile, sizeof(mmwave_chirp_profile_t));
    return MMWAVE_OK;
}

int32 profile_mgr_switch(profile_mgr_handle_t handle, uint32 profile_id)
{
    profile_mgr_ctx_t* ctx = (profile_mgr_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (profile_id >= MMWAVE_MAX_PROFILES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (ctx->slots[profile_id].state == PROFILE_STATE_EMPTY) {
        return MMWAVE_ERR_NOT_OPEN;  /* Cannot switch to unconfigured profile */
    }

    /* Deactivate previous active profile */
    if (ctx->slots[ctx->active_profile_id].state == PROFILE_STATE_ACTIVE) {
        ctx->slots[ctx->active_profile_id].state = PROFILE_STATE_CONFIGURED;
    }

    /* Activate new profile */
    ctx->slots[profile_id].state = PROFILE_STATE_ACTIVE;
    ctx->active_profile_id = profile_id;

    return MMWAVE_OK;
}

int32 profile_mgr_validate(const mmwave_chirp_profile_t* profile)
{
    if (profile == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    /* Validate frequency range */
    if (profile->start_freq_ghz < FREQ_MIN_GHZ ||
        profile->start_freq_ghz > FREQ_MAX_GHZ) {
        return MMWAVE_ERR_CFG_INVALID_FREQ;
    }

    /* Validate frequency slope */
    if (profile->freq_slope_mhz_us < SLOPE_MIN_MHZ_US ||
        profile->freq_slope_mhz_us > SLOPE_MAX_MHZ_US) {
        return MMWAVE_ERR_CFG_INVALID_SLOPE;
    }

    /* Validate timing parameters */
    if (profile->idle_time_us < IDLE_TIME_MIN_US) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (profile->ramp_end_time_us < RAMP_END_MIN_US) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (profile->adc_start_time_us >= profile->ramp_end_time_us) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    /* Validate ADC configuration */
    if (profile->adc_samples < ADC_SAMPLES_MIN ||
        profile->adc_samples > ADC_SAMPLES_MAX) {
        return MMWAVE_ERR_CFG_INVALID_SAMPLES;
    }
    if (profile->sample_rate_ksps == 0U ||
        profile->sample_rate_ksps > MMWAVE_ADC_MAX_RATE_KSPS) {
        return MMWAVE_ERR_CFG_INVALID_SAMPLES;
    }

    /* Validate RF parameters */
    if (profile->rx_gain_db > RX_GAIN_MAX_DB) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (profile->tx_power_backoff_db > TX_BACKOFF_MAX_DB) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    /* Validate bandwidth does not exceed platform limit */
    float32 bandwidth = profile->freq_slope_mhz_us *
                        profile->ramp_end_time_us / 1000.0f;
    if (bandwidth > MMWAVE_MAX_BANDWIDTH_GHZ) {
        return MMWAVE_ERR_CFG_BANDWIDTH_EXCEED;
    }

    /* Validate end frequency stays within band */
    float32 end_freq = profile->start_freq_ghz + bandwidth;
    if (end_freq > FREQ_MAX_GHZ) {
        return MMWAVE_ERR_CFG_INVALID_FREQ;
    }

    /* Validate ADC sampling fits within active sweep */
    float32 adc_duration_us = (float32)profile->adc_samples /
                              ((float32)profile->sample_rate_ksps * 1000.0f) * 1.0e6f;
    float32 available_time  = profile->ramp_end_time_us - profile->adc_start_time_us;
    if (adc_duration_us > available_time) {
        return MMWAVE_ERR_CFG_INVALID_SAMPLES;
    }

    return MMWAVE_OK;
}

int32 profile_mgr_get_info(profile_mgr_handle_t handle,
                           profile_info_t* info,
                           uint32* count)
{
    profile_mgr_ctx_t* ctx = (profile_mgr_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (info == NULL || count == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    uint32 configured = 0U;

    for (uint32 i = 0U; i < MMWAVE_MAX_PROFILES; i++) {
        info[i].profile_id = i;
        info[i].state = ctx->slots[i].state;

        if (ctx->slots[i].state != PROFILE_STATE_EMPTY) {
            const mmwave_chirp_profile_t* p = &ctx->slots[i].profile;
            info[i].start_freq_ghz = p->start_freq_ghz;
            info[i].bandwidth_ghz  = p->freq_slope_mhz_us *
                                     p->ramp_end_time_us / 1000.0f;
            configured++;
        } else {
            info[i].start_freq_ghz = 0.0f;
            info[i].bandwidth_ghz  = 0.0f;
        }
    }

    *count = configured;
    return MMWAVE_OK;
}

int32 profile_mgr_deinit(profile_mgr_handle_t handle)
{
    profile_mgr_ctx_t* ctx = (profile_mgr_ctx_t*)handle;

    if (ctx == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    memset(ctx, 0, sizeof(profile_mgr_ctx_t));
    return MMWAVE_OK;
}
