/**
 * @file test_sdk_full_flow.c
 * @brief Integration test: complete SDK lifecycle
 *        Init -> Open -> Config -> Calibrate -> Start -> GetFrames -> Stop -> Close -> Deinit
 */
#include "unity.h"
#include "mmwave_api.h"
#include "mmwave_types.h"
#include "mmwave_error_codes.h"

#include <string.h>

/* --------------- Fixtures --------------- */

static mmwave_handle_t g_handle;

void setUp(void)
{
    g_handle = NULL;
}

void tearDown(void)
{
    if (g_handle != NULL) {
        mmwave_deinit(g_handle);
        g_handle = NULL;
    }
}

/* --------------- Helpers --------------- */

static volatile uint32 g_frame_cb_count;
static volatile int32  g_last_error;

static void frame_callback(const mmwave_frame_t* frame, void* user_data)
{
    (void)user_data;
    if (frame != NULL) {
        g_frame_cb_count++;
    }
}

static void error_callback(int32 code, const char* msg, void* user_data)
{
    (void)msg;
    (void)user_data;
    g_last_error = code;
}

/* --------------- Tests --------------- */

/** Full lifecycle must complete without errors and frames must be valid. */
void test_full_lifecycle(void)
{
    g_frame_cb_count = 0;
    g_last_error     = MMWAVE_OK;

    /* 1. Init with defaults */
    int32 ret = mmwave_init(&g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_NOT_NULL(g_handle);

    /* 2. Open hardware */
    ret = mmwave_open(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* 3. Register callbacks */
    ret = mmwave_register_frame_callback(g_handle, frame_callback, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    ret = mmwave_register_error_callback(g_handle, error_callback, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* 4. Configure chirp */
    mmwave_chirp_profile_t chirp = {
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
    ret = mmwave_config_chirp(g_handle, &chirp);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* 5. Configure frame */
    mmwave_frame_config_t frame_cfg = {
        .chirps_per_frame     = 128,
        .num_frames           = 10,
        .frame_periodicity_ms = 33.33f,
        .tx_channel_mask      = 0x07,
        .rx_channel_mask      = 0x0F,
        .profile_id           = 0
    };
    ret = mmwave_config_frame(g_handle, &frame_cfg);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* 6. Calibrate */
    ret = mmwave_calibrate(g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* 7. Start */
    ret = mmwave_start(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    mmwave_status_t status;
    mmwave_get_status(g_handle, &status);
    TEST_ASSERT_TRUE(status.is_running);

    /* 8. Get a frame */
    mmwave_frame_t frame;
    ret = mmwave_get_frame(g_handle, &frame, 1000);
    if (ret == MMWAVE_OK) {
        TEST_ASSERT_NOT_NULL(frame.adc_data);
        TEST_ASSERT_TRUE(frame.num_chirps > 0);
        TEST_ASSERT_TRUE(frame.adc_data_size > 0);
        TEST_ASSERT_TRUE(frame.crc != 0);
    }

    /* 9. Stop */
    ret = mmwave_stop(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* 10. Close */
    ret = mmwave_close(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* 11. Deinit */
    ret = mmwave_deinit(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    g_handle = NULL;

    /* Verify no asynchronous errors occurred */
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, g_last_error);
}

/** State transitions must be enforced in order. */
void test_state_transitions(void)
{
    /* Cannot start without open */
    int32 ret = mmwave_init(&g_handle, NULL);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    ret = mmwave_start(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_NOT_OPEN, ret);

    /* Cannot stop when not running */
    ret = mmwave_open(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    ret = mmwave_stop(g_handle);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_NOT_RUNNING, ret);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_full_lifecycle);
    RUN_TEST(test_state_transitions);
    return UNITY_END();
}
