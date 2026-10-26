/**
 * @file test_cfar.c
 * @brief Unit tests for CFAR (Constant False Alarm Rate) detection
 */
#include "unity.h"
#include "mmwave_types.h"
#include "cfar_detector.h"

#include <string.h>

#define NUM_RANGE_BINS  128

/* --------------- Fixtures --------------- */

static float32 g_range_profile[NUM_RANGE_BINS];
static cfar_detection_t g_detections[CFAR_MAX_DETECTIONS];
static cfar_config_t g_cfar_cfg;

void setUp(void)
{
    memset(g_range_profile, 0, sizeof(g_range_profile));
    memset(g_detections, 0, sizeof(g_detections));

    g_cfar_cfg.num_guard_cells  = 2;
    g_cfar_cfg.num_train_cells  = 8;
    g_cfar_cfg.threshold_scale  = 4.0f;
    g_cfar_cfg.method           = CFAR_CA;  /* cell-averaging */
}

void tearDown(void)
{
    /* nothing to clean up */
}

/* --------------- Tests --------------- */

/** CA-CFAR must detect a single target well above noise floor. */
void test_ca_cfar_single_target(void)
{
    /* Flat noise floor at 1.0, target at bin 50 with amplitude 20 */
    for (uint32 i = 0; i < NUM_RANGE_BINS; i++) {
        g_range_profile[i] = 1.0f;
    }
    g_range_profile[50] = 20.0f;

    uint32 num_det = 0;
    int32 ret = cfar_detect(g_range_profile, NUM_RANGE_BINS,
                            &g_cfar_cfg, g_detections,
                            CFAR_MAX_DETECTIONS, &num_det);

    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(1, num_det);
    TEST_ASSERT_EQUAL_UINT32(50, g_detections[0].range_bin);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, g_detections[0].amplitude);
}

/** No target above threshold must yield zero detections. */
void test_ca_cfar_no_target(void)
{
    for (uint32 i = 0; i < NUM_RANGE_BINS; i++) {
        g_range_profile[i] = 1.0f;
    }

    uint32 num_det = 0;
    int32 ret = cfar_detect(g_range_profile, NUM_RANGE_BINS,
                            &g_cfar_cfg, g_detections,
                            CFAR_MAX_DETECTIONS, &num_det);

    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0, num_det);
}

/** OS-CFAR must detect multiple separated targets. */
void test_os_cfar_multiple_targets(void)
{
    g_cfar_cfg.method = CFAR_OS;  /* ordered-statistic */

    for (uint32 i = 0; i < NUM_RANGE_BINS; i++) {
        g_range_profile[i] = 1.0f;
    }
    g_range_profile[30] = 25.0f;
    g_range_profile[80] = 18.0f;

    uint32 num_det = 0;
    int32 ret = cfar_detect(g_range_profile, NUM_RANGE_BINS,
                            &g_cfar_cfg, g_detections,
                            CFAR_MAX_DETECTIONS, &num_det);

    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(2, num_det);
}

/** Guard cells must prevent target leakage into training window. */
void test_guard_cells_configuration(void)
{
    g_cfar_cfg.num_guard_cells = 4;
    g_cfar_cfg.num_train_cells = 16;

    for (uint32 i = 0; i < NUM_RANGE_BINS; i++) {
        g_range_profile[i] = 1.0f;
    }
    g_range_profile[60] = 30.0f;

    uint32 num_det = 0;
    cfar_detect(g_range_profile, NUM_RANGE_BINS,
                &g_cfar_cfg, g_detections, CFAR_MAX_DETECTIONS, &num_det);

    /* Must still detect only the target, not ghost side-bands */
    TEST_ASSERT_EQUAL_UINT32(1, num_det);
    TEST_ASSERT_EQUAL_UINT32(60, g_detections[0].range_bin);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ca_cfar_single_target);
    RUN_TEST(test_ca_cfar_no_target);
    RUN_TEST(test_os_cfar_multiple_targets);
    RUN_TEST(test_guard_cells_configuration);
    return UNITY_END();
}
