/**
 * @file test_multi_profile.c
 * @brief Integration test: runtime chirp-profile switching
 */
#include "unity.h"
#include "mmwave_api.h"
#include "mmwave_types.h"
#include "mmwave_error_codes.h"

/* --------------- Fixtures --------------- */

static mmwave_handle_t g_handle;

void setUp(void)
{
    g_handle = NULL;
    mmwave_init(&g_handle, NULL);
    mmwave_open(g_handle);
}

void tearDown(void)
{
    if (g_handle != NULL) {
        mmwave_deinit(g_handle);
        g_handle = NULL;
    }
}

/* --------------- Helpers --------------- */

static mmwave_chirp_profile_t make_profile(uint32 id, float32 start_ghz,
                                            float32 slope, uint16 samples)
{
    mmwave_chirp_profile_t p = {
        .profile_id          = id,
        .start_freq_ghz      = start_ghz,
        .freq_slope_mhz_us   = slope,
        .idle_time_us        = 7.0f,
        .adc_start_time_us   = 6.0f,
        .ramp_end_time_us    = 40.0f,
        .adc_samples         = samples,
        .sample_rate_ksps    = 10000,
        .rx_gain_db          = 30,
        .tx_power_backoff_db = 0
    };
    return p;
}

/* --------------- Tests --------------- */

/** Store multiple profiles and switch between them at runtime. */
void test_profile_switch_while_stopped(void)
{
    mmwave_chirp_profile_t p0 = make_profile(0, 77.0f, 70.0f, 256);
    mmwave_chirp_profile_t p1 = make_profile(1, 78.0f, 50.0f, 512);

    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, mmwave_config_chirp(g_handle, &p0));
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, mmwave_config_chirp(g_handle, &p1));

    /* Use profile 0 for frame */
    mmwave_frame_config_t frame = {
        .chirps_per_frame     = 64,
        .num_frames           = 0,
        .frame_periodicity_ms = 50.0f,
        .tx_channel_mask      = 0x01,
        .rx_channel_mask      = 0x0F,
        .profile_id           = 0
    };
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, mmwave_config_frame(g_handle, &frame));

    /* Switch to profile 1 */
    frame.profile_id = 1;
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, mmwave_config_frame(g_handle, &frame));
}

/** Switching profile while sensor is running must either succeed or
 *  return NOT_RUNNING if hot-switch is unsupported. */
void test_profile_switch_while_running(void)
{
    mmwave_chirp_profile_t p0 = make_profile(0, 77.0f, 70.0f, 256);
    mmwave_config_chirp(g_handle, &p0);

    mmwave_frame_config_t frame = {
        .chirps_per_frame     = 128,
        .num_frames           = 0,
        .frame_periodicity_ms = 33.33f,
        .tx_channel_mask      = 0x07,
        .rx_channel_mask      = 0x0F,
        .profile_id           = 0
    };
    mmwave_config_frame(g_handle, &frame);
    mmwave_start(g_handle);

    mmwave_chirp_profile_t p1 = make_profile(1, 79.0f, 60.0f, 256);
    int32 ret = mmwave_config_chirp(g_handle, &p1);
    /* Accept either success or a graceful rejection */
    TEST_ASSERT_TRUE(ret == MMWAVE_OK || ret == MMWAVE_ERR_ALREADY_RUNNING);

    mmwave_stop(g_handle);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_profile_switch_while_stopped);
    RUN_TEST(test_profile_switch_while_running);
    return UNITY_END();
}
