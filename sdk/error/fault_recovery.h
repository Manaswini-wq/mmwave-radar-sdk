/**
 * @file fault_recovery.h
 * @brief Fault recovery for mmWave Radar SDK
 * @details Graceful degradation, automatic retry with exponential back-off,
 *          safe-state entry for unrecoverable faults, and user-defined
 *          recovery callback registration.
 */
#ifndef FAULT_RECOVERY_H
#define FAULT_RECOVERY_H

#include "mmwave_types.h"

/** Recovery action returned by a recovery callback */
typedef enum {
    RECOVERY_ACTION_RETRY       = 0, /**< Retry the failed operation */
    RECOVERY_ACTION_DEGRADE     = 1, /**< Continue in degraded mode */
    RECOVERY_ACTION_SAFE_STATE  = 2, /**< Enter safe state (stop RF) */
    RECOVERY_ACTION_RESET       = 3  /**< Full hardware reset */
} recovery_action_t;

/**
 * @brief User recovery callback type
 * @param error_code  The error that triggered recovery
 * @param attempt     Current retry attempt number (0-based)
 * @param user_data   User context
 * @return Requested recovery action
 */
typedef recovery_action_t (*recovery_callback_t)(int32 error_code,
                                                  uint32 attempt,
                                                  void* user_data);

/**
 * @brief Initialise the fault-recovery subsystem
 * @return MMWAVE_OK on success
 */
int32 fault_recovery_init(void);

/**
 * @brief De-initialise and clear all callbacks
 */
void  fault_recovery_deinit(void);

/**
 * @brief Register a recovery callback for a specific error code
 * @param error_code  Error code to handle (or 0 for catch-all)
 * @param callback    Recovery callback function
 * @param user_data   User context passed to callback
 * @return MMWAVE_OK on success
 */
int32 fault_recovery_register(int32 error_code,
                              recovery_callback_t callback,
                              void* user_data);

/**
 * @brief Execute fault recovery for a given error
 * @param error_code  The error that occurred
 * @return Final recovery action taken
 */
recovery_action_t fault_recovery_handle(int32 error_code);

/**
 * @brief Set maximum retry count before entering safe state
 * @param max_retries Maximum retries (default 3)
 */
void fault_recovery_set_max_retries(uint32 max_retries);

#endif /* FAULT_RECOVERY_H */
