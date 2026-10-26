/**
 * @file test_latency_benchmark.c
 * @brief System test: benchmark frame-to-frame latency and processing time
 */
#include "unity.h"
#include "mmwave_api.h"
#include "mmwave_types.h"
#include "mmwave_error_codes.h"
#include "benchmark_timer.h"

#include <string.h>

#define NUM_BENCH_FRAMES   50
#define MAX_LATENCY_US     40000   /* 40 ms budget for a 30 fps system */
#define MAX_PROC_TIME_US   5000    /* 5 ms processing budget */

/* --------------- Fixtures --------------- */

static mmwave_handle_t g_handle;

void setUp(void)
{
    g_handle = NULL;
    mmwave_init(&g_handle, NULL);
    mmwave_open(g_handle);
    mmwave_start(g_handle);
}

void tearDown(void)
{
    if (g_handle != NULL) {
        mmwave_deinit(g_handle);
        g_handle = NULL;
    }
}

/* --------------- Tests --------------- */

/** Frame-to-frame latency must stay within real-time budget. */
void test_frame_latency(void)
{
    uint32 latencies_us[NUM_BENCH_FRAMES];
    uint32 prev_ts = 0;

    for (uint32 i = 0; i < NUM_BENCH_FRAMES; i++) {
        mmwave_frame_t frame;
        int32 ret = mmwave_get_frame(g_handle, &frame, 1000);
        if (ret != MMWAVE_OK) {
            TEST_FAIL_MESSAGE("Failed to get frame during latency test");
        }
        if (i > 0) {
            latencies_us[i] = frame.timestamp_us - prev_ts;
        }
        prev_ts = frame.timestamp_us;
    }

    /* Compute average latency (skip first frame) */
    uint64 sum = 0;
    for (uint32 i = 1; i < NUM_BENCH_FRAMES; i++) {
        sum += latencies_us[i];
        TEST_ASSERT_TRUE_MESSAGE(latencies_us[i] < MAX_LATENCY_US,
                                 "Frame latency exceeded budget");
    }
    uint32 avg = (uint32)(sum / (NUM_BENCH_FRAMES - 1));
    TEST_ASSERT_TRUE(avg < MAX_LATENCY_US);
}

/** Per-frame processing time must fit in the inter-chirp idle period. */
void test_processing_time(void)
{
    for (uint32 i = 0; i < NUM_BENCH_FRAMES; i++) {
        mmwave_frame_t frame;
        int32 ret = mmwave_get_frame(g_handle, &frame, 1000);
        if (ret != MMWAVE_OK) continue;

        uint32 start_us = benchmark_timer_get_us();
        /* Simulate processing: range FFT + CFAR on the frame */
        benchmark_process_frame(&frame);
        uint32 elapsed = benchmark_timer_get_us() - start_us;

        TEST_ASSERT_TRUE_MESSAGE(elapsed < MAX_PROC_TIME_US,
                                 "Processing time exceeded budget");
    }
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_frame_latency);
    RUN_TEST(test_processing_time);
    return UNITY_END();
}
