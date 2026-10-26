/**
 * @file calibration_mgr.h
 * @brief Calibration Manager for mmWave Radar SDK
 * @details Orchestrates auto-calibration sequences including TX power,
 *          RX gain/phase, and LO leakage calibration. Supports temperature-
 *          triggered recalibration and flash store/restore of cal data.
 */
#ifndef CALIBRATION_MGR_H
#define CALIBRATION_MGR_H

#include "mmwave_types.h"

/** Opaque handle to calibration manager */
typedef void* cal_handle_t;

/** Calibration status codes */
#define CAL_STATUS_IDLE             0U
#define CAL_STATUS_IN_PROGRESS      1U
#define CAL_STATUS_COMPLETE         2U
#define CAL_STATUS_FAILED           3U

/** Calibration result summary */
typedef struct {
    uint32  status;                    /**< Overall status (CAL_STATUS_*) */
    bool    tx_power_cal_done;         /**< TX power cal completed */
    bool    rx_gain_phase_cal_done;    /**< RX gain/phase cal completed */
    bool    lo_leakage_cal_done;       /**< LO leakage cal completed */
    float32 last_cal_temperature_c;    /**< Temperature at last calibration */
    uint32  cal_count;                 /**< Number of calibrations performed */
} cal_status_t;

/**
 * @brief Initialize the calibration manager
 * @param[out] handle  Pointer to receive calibration handle
 * @return MMWAVE_OK on success
 */
int32 calibration_mgr_init(cal_handle_t* handle);

/**
 * @brief Run calibration sequence
 * @param[in] handle  Calibration handle
 * @param[in] config  Calibration configuration
 * @return MMWAVE_OK on success
 */
int32 calibration_mgr_run(cal_handle_t handle, const mmwave_cal_config_t* config);

/**
 * @brief Get calibration status
 * @param[in]  handle  Calibration handle
 * @param[out] status  Pointer to receive status
 * @return MMWAVE_OK on success
 */
int32 calibration_mgr_get_status(cal_handle_t handle, cal_status_t* status);

/**
 * @brief Store calibration data to flash
 * @param[in] handle  Calibration handle
 * @return MMWAVE_OK on success
 */
int32 calibration_mgr_store(cal_handle_t handle);

/**
 * @brief Restore calibration data from flash
 * @param[in] handle  Calibration handle
 * @return MMWAVE_OK on success, MMWAVE_ERR_CAL_DATA_INVALID if no valid data
 */
int32 calibration_mgr_restore(cal_handle_t handle);

/**
 * @brief De-initialize the calibration manager
 * @param[in] handle  Calibration handle
 * @return MMWAVE_OK on success
 */
int32 calibration_mgr_deinit(cal_handle_t handle);

#endif /* CALIBRATION_MGR_H */
