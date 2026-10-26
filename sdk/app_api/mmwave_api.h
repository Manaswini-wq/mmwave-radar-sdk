/**
 * @file mmwave_api.h
 * @brief Top-level mmWave Radar SDK API
 * @details Easy-to-use interface that abstracts and simplifies customer usage
 *          of TI's mmWave radar sensors. Provides complete sensor lifecycle
 *          management including configuration, calibration, and data acquisition.
 */
#ifndef MMWAVE_API_H
#define MMWAVE_API_H

#include "mmwave_types.h"

/**
 * @brief Initialize the mmWave SDK
 * @param[out] handle  Pointer to receive SDK handle
 * @param[in]  config  SDK configuration (NULL for defaults)
 * @return MMWAVE_OK on success, negative error code on failure
 */
int32 mmwave_init(mmwave_handle_t* handle, const mmwave_config_t* config);

/**
 * @brief Open the radar sensor hardware
 * @param[in] handle  SDK handle from mmwave_init
 * @return MMWAVE_OK on success, negative error code on failure
 */
int32 mmwave_open(mmwave_handle_t handle);

/**
 * @brief Configure chirp profile
 * @param[in] handle   SDK handle
 * @param[in] profile  Chirp profile configuration
 * @return MMWAVE_OK on success
 */
int32 mmwave_config_chirp(mmwave_handle_t handle, const mmwave_chirp_profile_t* profile);

/**
 * @brief Configure frame parameters
 * @param[in] handle  SDK handle
 * @param[in] config  Frame configuration
 * @return MMWAVE_OK on success
 */
int32 mmwave_config_frame(mmwave_handle_t handle, const mmwave_frame_config_t* config);

/**
 * @brief Run sensor calibration
 * @param[in] handle  SDK handle
 * @param[in] config  Calibration configuration (NULL for defaults)
 * @return MMWAVE_OK on success
 */
int32 mmwave_calibrate(mmwave_handle_t handle, const mmwave_cal_config_t* config);

/**
 * @brief Start radar sensor operation
 * @param[in] handle  SDK handle
 * @return MMWAVE_OK on success
 */
int32 mmwave_start(mmwave_handle_t handle);

/**
 * @brief Stop radar sensor operation
 * @param[in] handle  SDK handle
 * @return MMWAVE_OK on success
 */
int32 mmwave_stop(mmwave_handle_t handle);

/**
 * @brief Close the radar sensor hardware
 * @param[in] handle  SDK handle
 * @return MMWAVE_OK on success
 */
int32 mmwave_close(mmwave_handle_t handle);

/**
 * @brief De-initialize the SDK and free resources
 * @param[in] handle  SDK handle
 * @return MMWAVE_OK on success
 */
int32 mmwave_deinit(mmwave_handle_t handle);

/**
 * @brief Register frame data callback
 * @param[in] handle    SDK handle
 * @param[in] callback  Frame callback function
 * @param[in] user_data User context passed to callback
 * @return MMWAVE_OK on success
 */
int32 mmwave_register_frame_callback(mmwave_handle_t handle,
                                      mmwave_frame_callback_t callback,
                                      void* user_data);

/**
 * @brief Register error callback
 * @param[in] handle    SDK handle
 * @param[in] callback  Error callback function
 * @param[in] user_data User context passed to callback
 * @return MMWAVE_OK on success
 */
int32 mmwave_register_error_callback(mmwave_handle_t handle,
                                      mmwave_error_callback_t callback,
                                      void* user_data);

/**
 * @brief Get current sensor status
 * @param[in]  handle  SDK handle
 * @param[out] status  Pointer to receive status
 * @return MMWAVE_OK on success
 */
int32 mmwave_get_status(mmwave_handle_t handle, mmwave_status_t* status);

/**
 * @brief Get a radar data frame (blocking)
 * @param[in]  handle     SDK handle
 * @param[out] frame      Pointer to receive frame data
 * @param[in]  timeout_ms Timeout in ms (MMWAVE_WAIT_FOREVER for infinite)
 * @return MMWAVE_OK on success, MMWAVE_ERR_TIMEOUT on timeout
 */
int32 mmwave_get_frame(mmwave_handle_t handle, mmwave_frame_t* frame, uint32 timeout_ms);

#endif /* MMWAVE_API_H */
