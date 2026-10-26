/**
 * @file test_people_counting_app.c
 * @brief System test: validate people-counting demo output
 */
#include "unity.h"
#include "mmwave_api.h"
#include "mmwave_types.h"
#include "mmwave_error_codes.h"
#include "people_counting.h"

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

/* --------------- Tests --------------- */

/** With a synthetic scene of 3 people the detector must report count in [2,4]. */
void test_people_count_synthetic_scene(void)
{
    people_counting_config_t pc_cfg = {
        .max_range_m     = 6.0f,
        .min_range_m     = 0.5f,
        .azimuth_fov_deg = 120.0f,
        .group_dist_m    = 0.5f,
        .min_snr_db      = 12.0f
    };
    people_counting_handle_t pc;
    int32 ret = people_counting_init(&pc, &pc_cfg);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);

    /* Inject 10 synthetic frames with 3 known point clusters */
    for (uint32 f = 0; f < 10; f++) {
        point_cloud_t cloud;
        people_counting_generate_test_scene(&cloud, 3, f);
        ret = people_counting_process(pc, &cloud);
        TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    }

    uint32 count = people_counting_get_count(pc);
    TEST_ASSERT_TRUE(count >= 2 && count <= 4);

    people_counting_deinit(pc);
}

/** Empty scene must yield zero people. */
void test_people_count_empty_scene(void)
{
    people_counting_config_t pc_cfg = {
        .max_range_m = 6.0f, .min_range_m = 0.5f,
        .azimuth_fov_deg = 120.0f, .group_dist_m = 0.5f, .min_snr_db = 12.0f
    };
    people_counting_handle_t pc;
    people_counting_init(&pc, &pc_cfg);

    point_cloud_t empty = { .num_points = 0 };
    people_counting_process(pc, &empty);

    TEST_ASSERT_EQUAL_UINT32(0, people_counting_get_count(pc));
    people_counting_deinit(pc);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_people_count_synthetic_scene);
    RUN_TEST(test_people_count_empty_scene);
    return UNITY_END();
}
