/**
 * @file error_handler.c
 * @brief Error handler implementation for mmWave Radar SDK
 * @details
 *  - Circular log buffer with timestamps for post-mortem analysis
 *  - Severity classification (INFO, WARNING, ERROR, CRITICAL)
 *  - Optional UART output for real-time debug
 *  - Fail-safe defaults on critical errors (stop RF, enter standby)
 */
#include "error_handler.h"
#include "mmwave_error_codes.h"
#include "hal_uart.h"

#include <string.h>

/** Error log ring buffer size (must be power of 2) */
#define ERR_LOG_SIZE    32U
#define ERR_LOG_MASK    (ERR_LOG_SIZE - 1U)

/** Maximum message length stored per entry */
#define ERR_MSG_MAX     64U

/** Severity levels */
typedef enum {
    ERR_SEV_INFO     = 0,
    ERR_SEV_WARNING  = 1,
    ERR_SEV_ERROR    = 2,
    ERR_SEV_CRITICAL = 3
} error_severity_t;

/** Single error log entry */
typedef struct {
    uint32            timestamp_us;
    int32             error_code;
    error_severity_t  severity;
    char              message[ERR_MSG_MAX];
} error_entry_t;

/** Error handler state */
typedef struct {
    bool            initialized;
    error_entry_t   log[ERR_LOG_SIZE];
    uint32          head;           /**< Next write position */
    uint32          total_count;    /**< Lifetime error counter */
    int32           last_error;
    bool            uart_output;    /**< Enable UART debug prints */
} error_handler_ctx_t;

static error_handler_ctx_t g_err;

/* ---- Internal helpers ---- */

/**
 * @brief Classify an error code into a severity level
 */
static error_severity_t classify_severity(int32 code)
{
    if (code >= 0) {
        return ERR_SEV_INFO;
    }
    /* Hardware / calibration faults are critical */
    if (code <= -100 && code > -200) {
        return ERR_SEV_CRITICAL;     /* HW errors */
    }
    if (code <= -200 && code > -300) {
        return ERR_SEV_ERROR;        /* Calibration errors */
    }
    if (code <= -400 && code > -500) {
        return ERR_SEV_WARNING;      /* Data errors (recoverable) */
    }
    return ERR_SEV_ERROR;
}

/**
 * @brief Enter fail-safe state on critical errors.
 *        Stops RF transmission and reports via UART if available.
 */
static void enter_failsafe(int32 code, const char* msg)
{
    /* In a real system this would:
     *  1. Assert RF_KILL GPIO to cut TX power
     *  2. Disable DMA transfers
     *  3. Place analog front-end in standby
     * Here we log and halt further operation.
     */
    const char failsafe_hdr[] = "[CRITICAL FAILSAFE] ";
    if (g_err.uart_output) {
        hal_uart_write_string(MMWAVE_DEBUG_UART_INSTANCE, failsafe_hdr);
        hal_uart_write_string(MMWAVE_DEBUG_UART_INSTANCE, msg);
        hal_uart_write_string(MMWAVE_DEBUG_UART_INSTANCE, "\r\n");
    }
    (void)code;
}

/**
 * @brief Format and send one log entry over UART
 */
static void uart_print_entry(const error_entry_t* entry)
{
    static const char* sev_str[] = { "INFO", "WARN", "ERR ", "CRIT" };

    char buf[128];
    /* Simple integer-to-string formatting without printf */
    uint32 ts = entry->timestamp_us;
    int idx = 0;

    buf[idx++] = '[';
    /* Timestamp: seconds.millis */
    uint32 sec  = ts / 1000000U;
    uint32 msec = (ts / 1000U) % 1000U;
    /* Write seconds (up to 5 digits) */
    char tmp[12];
    int tlen = 0;
    do { tmp[tlen++] = '0' + (char)(sec % 10U); sec /= 10U; } while (sec > 0U);
    while (tlen > 0) buf[idx++] = tmp[--tlen];
    buf[idx++] = '.';
    buf[idx++] = '0' + (char)((msec / 100U) % 10U);
    buf[idx++] = '0' + (char)((msec /  10U) % 10U);
    buf[idx++] = '0' + (char)( msec         % 10U);
    buf[idx++] = ']';
    buf[idx++] = ' ';

    /* Severity tag */
    const char* sv = sev_str[entry->severity];
    while (*sv) buf[idx++] = *sv++;
    buf[idx++] = ':';
    buf[idx++] = ' ';

    /* Message (truncated) */
    const char* m = entry->message;
    while (*m && idx < 120) buf[idx++] = *m++;
    buf[idx++] = '\r';
    buf[idx++] = '\n';
    buf[idx]   = '\0';

    hal_uart_write_string(MMWAVE_DEBUG_UART_INSTANCE, buf);
}

/* ---- Public API ---- */

int32 error_handler_init(void)
{
    if (g_err.initialized) {
        return MMWAVE_ERR_ALREADY_INITIALIZED;
    }
    memset(&g_err, 0, sizeof(error_handler_ctx_t));
    g_err.uart_output  = true;  /* Enable UART by default in debug builds */
    g_err.initialized  = true;
    return MMWAVE_OK;
}

void error_handler_deinit(void)
{
    g_err.initialized = false;
}

void error_handler_report(int32 error_code, const char* message)
{
    if (!g_err.initialized) return;

    error_severity_t sev = classify_severity(error_code);

    /* Write entry into circular buffer */
    uint32 idx = g_err.head & ERR_LOG_MASK;
    g_err.log[idx].timestamp_us = hal_get_timestamp_us();
    g_err.log[idx].error_code   = error_code;
    g_err.log[idx].severity     = sev;

    /* Safe string copy */
    if (message != NULL) {
        uint32 i;
        for (i = 0; i < ERR_MSG_MAX - 1U && message[i] != '\0'; i++) {
            g_err.log[idx].message[i] = message[i];
        }
        g_err.log[idx].message[i] = '\0';
    } else {
        g_err.log[idx].message[0] = '\0';
    }

    g_err.head++;
    g_err.total_count++;
    g_err.last_error = error_code;

    /* UART output */
    if (g_err.uart_output) {
        uart_print_entry(&g_err.log[idx]);
    }

    /* Trigger fail-safe on critical severity */
    if (sev == ERR_SEV_CRITICAL) {
        enter_failsafe(error_code, message ? message : "Unknown critical fault");
    }
}

int32 error_handler_get_last(void)
{
    return g_err.last_error;
}

uint32 error_handler_get_count(void)
{
    return g_err.total_count;
}
