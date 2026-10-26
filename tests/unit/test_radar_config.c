/**
 * @file test_radar_config.c
 * @brief Unit tests for the radar configuration manager
 */
#include "unity.h"
#include "mmwave_types.h"
#include "mmwave_error_codes.h"
#include "mmwave_config.h"
#include "radar_config_mgr.h"

/* --------------- Fixtures --------------- */

static radar_config_handle_t g_cfg_handle;

void setUp(void)
{
    g_cfg_handle = NULL;
    radar_config_init(&g_cfg_handle);
}

void tearDown(void)
{
    if (g_cfg_handle != NULL) {
        radar_config_deinit(g_cfg_handle);
        g_cfg_handle = NULL;
    }
}

/* --------------- Tests --------------- */

/** Verify chirp timing calculation: bandwidth = slope * ramp_time / 1000. */
void test_chirp_timing_calculation(void)
{
    mmwave_chirp_profile_t profile = {
        .profile_id        = 0,
        .start_freq_ghz    = 77.0f,
        .freq_slope_mhz_us = 70.0f,
        .idle_time_us      = 7.0f,
        .adc_start_time_us = 6.0f,
        .ramp_end_time_us  = 40.0f,
        .adc_samples       = 256,
        .sample_rate_ksps  = 10000,
        .rx_gain_db        = 30,
        .tx_power_backoff_db = 0
    };

    float32 expected_bw_ghz = 70.0f * 40.0f / 1000.0f;  /* 2.8 GHz */
    float32 computed_bw = radar_config_get_bandwidth_ghz(&profile);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected_bw_ghz, computed_bw);

    float32 range_res = radar_config_get_range_resolution_m(&profile);
    /* range_res = c / (2 * BW) */
    float32 expected_res = 3.0e8f / (2.0f * expected_bw_ghz * 1.0e9f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected_res, range_res);
}

/** Profile with frequency outside 76-81 GHz band must be rejected. */
void test_profile_validation_rejects_bad_freq(void)
{
    mmwave_chirp_profile_t bad_profile = {
        .profile_id     = 0,
        .start_freq_ghz = 60.0f,   /* out of band */
        .freq_slope_mhz_us = 70.0f,
        .ramp_end_time_us  = 40.0f,
        .adc_samples       = 256,
        .sample_rate_ksps  = 10000,
    };
    int32 ret = radar_config_set_chirp(g_cfg_handle, &bad_profile);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_CFG_INVALID_FREQ, ret);
}

/** SDK supports up to MMWAVE_MAX_PROFILES profiles stored simultaneously. */
void test_multi_profile_storage(void)
{
    for (uint32 id = 0; id < MMWAVE_MAX_PROFILES; id++) {
        mmwave_chirp_profile_t p = {
            .profile_id     = id,
            .start_freq_ghz = 77.0f + (float32)id * 0.5f,
            .freq_slope_mhz_us = 70.0f,
            .ramp_end_time_us  = 40.0f,
            .adc_samples       = 256,
            .sample_rate_ksps  = 10000,
        };
        int32 ret = radar_config_set_chirp(g_cfg_handle, &p);
        TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    }
    TEST_ASSERT_EQUAL_UINT32(MMWAVE_MAX_PROFILES,
                             radar_config_get_profile_count(g_cfg_handle));
}

/** chirps_per_frame must be within [1, MMWAVE_MAX_CHIRPS_PER_FRAME]. */
void test_frame_config_limits(void)
{
    mmwave_frame_config_t frame = {
        .chirps_per_frame     = MMWAVE_MAX_CHIRPS_PER_FRAME + 1,
        .num_frames           = 0,
        .frame_periodicity_ms = 33.33f,
        .tx_channel_mask      = 0x07,
        .rx_channel_mask      = 0x0F,
        .profile_id           = 0
    };
    int32 ret = radar_config_set_frame(g_cfg_handle, &frame);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_CFG_INVALID_SAMPLES, ret);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_chirp_timing_calculation);
    RUN_TEST(test_profile_validation_rejects_bad_freq);
    RUN_TEST(test_multi_profile_storage);
    RUN_TEST(test_frame_config_limits);
    return UNITY_END();
}
