/**
 * @file test_data_acquisition.c
 * @brief Unit tests for the data acquisition subsystem
 */
#include "unity.h"
#include "mmwave_types.h"
#include "mmwave_error_codes.h"
#include "mmwave_config.h"
#include "data_acquisition_api.h"

#include <string.h>

/* --------------- Fixtures --------------- */

static data_acq_handle_t g_acq;

void setUp(void)
{
    g_acq = NULL;
    data_acq_init(&g_acq, MMWAVE_ADC_BUFFER_SIZE);
}

void tearDown(void)
{
    if (g_acq != NULL) {
        data_acq_stop(g_acq);
        data_acq_deinit(g_acq);
        g_acq = NULL;
    }
}

/* --------------- Tests --------------- */

/** Ping-pong buffer index must toggle after each frame transfer. */
void test_ping_pong_buffer_switch(void)
{
    int32 ret = data_acq_start(g_acq);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    uint32 buf_idx_0 = data_acq_get_active_buffer_index(g_acq);
    TEST_ASSERT_TRUE(buf_idx_0 == 0 || buf_idx_0 == 1);

    /* Simulate one DMA transfer completion */
    data_acq_notify_dma_done(g_acq);

    uint32 buf_idx_1 = data_acq_get_active_buffer_index(g_acq);
    TEST_ASSERT_NOT_EQUAL(buf_idx_0, buf_idx_1);
}

/** Frame assembly must populate all required metadata. */
void test_frame_assembly(void)
{
    data_acq_start(g_acq);
    data_acq_notify_dma_done(g_acq);

    mmwave_frame_t frame;
    memset(&frame, 0, sizeof(frame));

    int32 ret = data_acq_get_latest_frame(g_acq, &frame);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_NOT_NULL(frame.adc_data);
    TEST_ASSERT_TRUE(frame.num_adc_samples > 0);
    TEST_ASSERT_TRUE(frame.num_chirps > 0);
    TEST_ASSERT_TRUE(frame.adc_data_size > 0);
}

/** CRC field must be non-zero and match recomputed value. */
void test_crc_validation(void)
{
    data_acq_start(g_acq);
    data_acq_notify_dma_done(g_acq);

    mmwave_frame_t frame;
    int32 ret = data_acq_get_latest_frame(g_acq, &frame);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_TRUE(frame.crc != 0);

    /* Recompute CRC over payload and compare */
    uint32 recomputed = data_acq_compute_crc(frame.adc_data, frame.adc_data_size);
    TEST_ASSERT_EQUAL_UINT32(frame.crc, recomputed);
}

/** Writing beyond the allocated buffer must return BUFFER_OVERFLOW. */
void test_buffer_overflow_detection(void)
{
    data_acq_start(g_acq);

    /* Feed more data than the buffer can hold */
    uint32 oversized = MMWAVE_ADC_BUFFER_SIZE + 1024;
    int32 ret = data_acq_inject_test_data(g_acq, NULL, oversized);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_BUFFER_OVERFLOW, ret);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ping_pong_buffer_switch);
    RUN_TEST(test_frame_assembly);
    RUN_TEST(test_crc_validation);
    RUN_TEST(test_buffer_overflow_detection);
    return UNITY_END();
}
