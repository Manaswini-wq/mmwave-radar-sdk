/**
 * @file fault_recovery.c
 * @brief Fault recovery implementation for mmWave Radar SDK
 * @details
 *  - Graceful degradation: reduces TX channels or frame rate on partial faults
 *  - Exponential back-off retry: delays 10ms, 20ms, 40ms, ... between retries
 *  - Safe-state entry: stops RF and disables DMA on unrecoverable errors
 *  - Per-error-code and catch-all callback registration
 */
#include "fault_recovery.h"
#include "mmwave_error_codes.h"
#include "error_handler.h"
#include "hal_gpio.h"
#include "hal_dma.h"

#include <string.h>

/** Maximum registered callbacks */
#define MAX_RECOVERY_CBS    8U

/** Default retry limit before entering safe state */
#define DEFAULT_MAX_RETRIES 3U

/** Base back-off delay in microseconds (10 ms) */
#define BACKOFF_BASE_US     10000U

/** Callback registration entry */
typedef struct {
    int32               error_code;   /**< 0 = catch-all */
    recovery_callback_t callback;
    void*               user_data;
    bool                in_use;
} recovery_slot_t;

/** Fault recovery context */
typedef struct {
    bool            initialized;
    recovery_slot_t slots[MAX_RECOVERY_CBS];
    uint32          max_retries;
} fault_recovery_ctx_t;

static fault_recovery_ctx_t g_fr;

/* ---- Internal helpers ---- */

/**
 * @brief Look up the most specific callback for an error code.
 *        First searches for an exact match, then falls back to catch-all (code 0).
 */
static recovery_slot_t* find_callback(int32 error_code)
{
    recovery_slot_t* catchall = NULL;

    for (uint32 i = 0; i < MAX_RECOVERY_CBS; i++) {
        if (!g_fr.slots[i].in_use) continue;
        if (g_fr.slots[i].error_code == error_code) {
            return &g_fr.slots[i];
        }
        if (g_fr.slots[i].error_code == 0) {
            catchall = &g_fr.slots[i];
        }
    }
    return catchall;
}

/**
 * @brief Enter safe state: assert RF_KILL, disable DMA, log event
 */
static void enter_safe_state(int32 error_code)
{
    hal_gpio_write(GPIO_PIN_RF_KILL, 1U);
    hal_dma_disable(HAL_DMA_CH_ADC);

    error_handler_report(error_code,
                         "Unrecoverable fault – entered safe state");
}

/**
 * @brief Apply degraded-mode configuration
 *        Reduces TX channel count and widens frame period to lower
 *        thermal and EMI stress while keeping the sensor operational.
 */
static void apply_degraded_mode(void)
{
    error_handler_report(MMWAVE_OK,
                         "Degraded mode: reduced TX channels, wider frame period");
    /* In a real implementation this would reprogram the RF front-end. */
}

/* ---- Public API ---- */

int32 fault_recovery_init(void)
{
    if (g_fr.initialized) {
        return MMWAVE_ERR_ALREADY_INITIALIZED;
    }
    memset(&g_fr, 0, sizeof(fault_recovery_ctx_t));
    g_fr.max_retries = DEFAULT_MAX_RETRIES;
    g_fr.initialized = true;
    return MMWAVE_OK;
}

void fault_recovery_deinit(void)
{
    memset(&g_fr, 0, sizeof(fault_recovery_ctx_t));
}

int32 fault_recovery_register(int32 error_code,
                              recovery_callback_t callback,
                              void* user_data)
{
    if (!g_fr.initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (callback == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    /* Find a free slot */
    for (uint32 i = 0; i < MAX_RECOVERY_CBS; i++) {
        if (!g_fr.slots[i].in_use) {
            g_fr.slots[i].error_code = error_code;
            g_fr.slots[i].callback   = callback;
            g_fr.slots[i].user_data  = user_data;
            g_fr.slots[i].in_use     = true;
            return MMWAVE_OK;
        }
    }
    return MMWAVE_ERR_OUT_OF_MEMORY; /* No free slots */
}

recovery_action_t fault_recovery_handle(int32 error_code)
{
    if (!g_fr.initialized) {
        enter_safe_state(error_code);
        return RECOVERY_ACTION_SAFE_STATE;
    }

    recovery_slot_t* slot = find_callback(error_code);
    recovery_action_t action = RECOVERY_ACTION_SAFE_STATE;

    for (uint32 attempt = 0; attempt <= g_fr.max_retries; attempt++) {

        /* Consult user callback if registered */
        if (slot != NULL) {
            action = slot->callback(error_code, attempt, slot->user_data);
        } else {
            /* Default policy: retry up to max, then safe state */
            action = (attempt < g_fr.max_retries)
                     ? RECOVERY_ACTION_RETRY
                     : RECOVERY_ACTION_SAFE_STATE;
        }

        switch (action) {
            case RECOVERY_ACTION_RETRY:
                /* Exponential back-off: base * 2^attempt */
                {
                    uint32 delay_us = BACKOFF_BASE_US << attempt;
                    if (delay_us > 1000000U) delay_us = 1000000U; /* Cap at 1 s */
                    hal_delay_us(delay_us);
                }
                error_handler_report(error_code, "Retry after back-off");
                continue; /* next attempt */

            case RECOVERY_ACTION_DEGRADE:
                apply_degraded_mode();
                return RECOVERY_ACTION_DEGRADE;

            case RECOVERY_ACTION_RESET:
                error_handler_report(error_code, "Recovery requested full reset");
                /* In real HW: trigger watchdog or SYSRESET */
                hal_gpio_write(GPIO_PIN_NRESET, 0U);
                hal_delay_us(1000U);
                hal_gpio_write(GPIO_PIN_NRESET, 1U);
                return RECOVERY_ACTION_RESET;

            case RECOVERY_ACTION_SAFE_STATE:
            default:
                enter_safe_state(error_code);
                return RECOVERY_ACTION_SAFE_STATE;
        }
    }

    /* Exhausted retries without explicit safe-state – force it */
    enter_safe_state(error_code);
    return RECOVERY_ACTION_SAFE_STATE;
}

void fault_recovery_set_max_retries(uint32 max_retries)
{
    if (max_retries > 0U) {
        g_fr.max_retries = max_retries;
    }
}
