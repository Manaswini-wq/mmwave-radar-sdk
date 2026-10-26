/**
 * @file test_mmwave_api.c
 * @brief Unit tests for the top-level mmWave SDK API (mmwave_api.h)
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
}

void tearDown(void)
{
    /* Ensure clean state between tests — deinit if still alive */
    if (g_handle != NULL) {
        mmwave_deinit(g_handle);
        g_handle = NULL;
    }
}

/* --------------- Tests --------------- */

/** Passing NULL for the handle pointer must return NULL_POINTER error. */
void test_init_with_null_handle(void)
{
    int32 ret = mmwave_init(NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_NULL_POINTER, ret);
}

/** Init with NULL config must succeed and apply the default configuration. */
void test_init_with_default_config(void)
{
    int32 ret = mmwave_init(&g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_NOT_NULL(g_handle);

    mmwave_status_t status;
    ret = mmwave_get_status(g_handle, &status);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_TRUE(status.is_initialized);
    TEST_ASSERT_FALSE(status.is_running);
}

/** Calling init twice without deinit must return ALREADY_INITIALIZED. */
void test_init_double_init_fails(void)
{
    int32 ret = mmwave_init(&g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    mmwave_handle_t handle2 = NULL;
    ret = mmwave_init(&handle2, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_ALREADY_INITIALIZED, ret);
    TEST_ASSERT_NULL(handle2);
}

/** Calling open without a prior init must fail. */
void test_open_before_init_fails(void)
{
    int32 ret = mmwave_open(NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_NOT_INITIALIZED, ret);
}

/** Chirp config with bandwidth exceeding platform limit must be rejected. */
void test_config_chirp_validates_bandwidth(void)
{
    int32 ret = mmwave_init(&g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    ret = mmwave_open(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    mmwave_chirp_profile_t bad_chirp = {
        .profile_id        = 0,
        .start_freq_ghz    = 77.0f,
        .freq_slope_mhz_us = 200.0f,   /* very steep slope */
        .idle_time_us      = 7.0f,
        .adc_start_time_us = 6.0f,
        .ramp_end_time_us  = 60.0f,     /* bandwidth = 200*60/1000 = 12 GHz */
        .adc_samples       = 256,
        .sample_rate_ksps  = 10000,
        .rx_gain_db        = 30,
        .tx_power_backoff_db = 0
    };
    ret = mmwave_config_chirp(g_handle, &bad_chirp);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_CFG_BANDWIDTH_EXCEED, ret);
}

/** Frame config with zero chirps must be rejected. */
void test_config_frame_validates_chirps(void)
{
    int32 ret = mmwave_init(&g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    ret = mmwave_open(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    mmwave_frame_config_t bad_frame = {
        .chirps_per_frame     = 0,   /* invalid */
        .num_frames           = 10,
        .frame_periodicity_ms = 33.33f,
        .tx_channel_mask      = 0x07,
        .rx_channel_mask      = 0x0F,
        .profile_id           = 0
    };
    ret = mmwave_config_frame(g_handle, &bad_frame);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_CFG_INVALID_SAMPLES, ret);
}

/** Start -> Stop lifecycle must transition correctly. */
void test_start_stop_lifecycle(void)
{
    int32 ret = mmwave_init(&g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    ret = mmwave_open(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    ret = mmwave_start(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    mmwave_status_t status;
    mmwave_get_status(g_handle, &status);
    TEST_ASSERT_TRUE(status.is_running);

    ret = mmwave_stop(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    mmwave_get_status(g_handle, &status);
    TEST_ASSERT_FALSE(status.is_running);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_with_null_handle);
    RUN_TEST(test_init_with_default_config);
    RUN_TEST(test_init_double_init_fails);
    RUN_TEST(test_open_before_init_fails);
    RUN_TEST(test_config_chirp_validates_bandwidth);
    RUN_TEST(test_config_frame_validates_chirps);
    RUN_TEST(test_start_stop_lifecycle);
    return UNITY_END();
}
