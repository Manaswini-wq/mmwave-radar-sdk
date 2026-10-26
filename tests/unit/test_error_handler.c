/**
 * @file test_error_handler.c
 * @brief Unit tests for the SDK error handler / logging subsystem
 */
#include "unity.h"
#include "mmwave_types.h"
#include "mmwave_error_codes.h"
#include "error_handler.h"

/* --------------- Fixtures --------------- */

void setUp(void)
{
    error_handler_init();
}

void tearDown(void)
{
    error_handler_deinit();
}

/* --------------- Tests --------------- */

/** Logging an error must be retrievable from the error log. */
void test_error_logging(void)
{
    error_handler_report(MMWAVE_ERR_HW_INIT_FAILED, "SPI init timeout");

    error_entry_t entry;
    int32 ret = error_handler_get_last(&entry);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_HW_INIT_FAILED, entry.error_code);
    TEST_ASSERT_NOT_NULL(entry.message);
}

/** Circular buffer must wrap after reaching capacity. */
void test_circular_buffer_wrap(void)
{
    /* Fill the log buffer beyond its capacity (MMWAVE_LOG_BUFFER_SIZE entries).
       The oldest entries must be overwritten silently. */
    for (uint32 i = 0; i < MMWAVE_LOG_BUFFER_SIZE + 10; i++) {
        error_handler_report(MMWAVE_ERR_TIMEOUT, "wrap test");
    }

    /* Most recent entry must still be valid */
    error_entry_t entry;
    int32 ret = error_handler_get_last(&entry);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_OK, ret);
    TEST_ASSERT_EQUAL_INT32(MMWAVE_ERR_TIMEOUT, entry.error_code);

    /* Total stored entries must be capped at buffer capacity */
    uint32 count = error_handler_get_log_count();
    TEST_ASSERT_TRUE(count <= MMWAVE_LOG_BUFFER_SIZE);
}

/** Error count must reflect the total number of reports, not just stored. */
void test_error_count(void)
{
    const uint32 num_reports = 5;
    for (uint32 i = 0; i < num_reports; i++) {
        error_handler_report(MMWAVE_ERR_DMA_ERROR, "dma fault");
    }

    uint32 total = error_handler_get_total_count();
    TEST_ASSERT_EQUAL_UINT32(num_reports, total);
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_error_logging);
    RUN_TEST(test_circular_buffer_wrap);
    RUN_TEST(test_error_count);
    return UNITY_END();
}
