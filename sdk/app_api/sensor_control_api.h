/**
 * @file sensor_control_api.h
 * @brief Sensor control API for mmWave Radar SDK
 * @details Provides power management, dynamic reconfiguration,
 *          temperature monitoring, self-test, and firmware version query.
 */
#ifndef SENSOR_CONTROL_API_H
#define SENSOR_CONTROL_API_H

#include "mmwave_types.h"

/** Sensor power modes */
typedef enum {
    SENSOR_POWER_OFF     = 0, /**< Full power down – all clocks gated */
    SENSOR_POWER_STANDBY = 1, /**< Low-power standby – PLL remains locked */
    SENSOR_POWER_ON      = 2  /**< Fully operational */
} sensor_power_mode_t;

/** Self-test result */
typedef struct {
    bool    pll_ok;            /**< PLL lock test passed */
    bool    lo_ok;             /**< LO distribution test passed */
    bool    tx_ok;             /**< TX loopback power test passed */
    bool    rx_ok;             /**< RX gain / noise-floor test passed */
    bool    adc_ok;            /**< ADC ramp test passed */
    int32   overall_result;    /**< MMWAVE_OK if all passed */
} sensor_selftest_result_t;

/** Firmware / hardware version info */
typedef struct {
    uint8   fw_major;          /**< Firmware major version */
    uint8   fw_minor;          /**< Firmware minor version */
    uint8   fw_patch;          /**< Firmware patch level */
    uint8   hw_revision;       /**< Hardware revision ID */
    uint32  rom_version;       /**< ROM patch version */
} sensor_version_t;

/**
 * @brief Set sensor power mode
 * @param[in] handle SDK handle
 * @param[in] mode   Desired power mode
 * @return MMWAVE_OK on success, negative error code on failure
 */
int32 sensor_set_power_mode(mmwave_handle_t handle, sensor_power_mode_t mode);

/**
 * @brief Get current sensor power mode
 * @param[in]  handle SDK handle
 * @param[out] mode   Current power mode
 * @return MMWAVE_OK on success
 */
int32 sensor_get_power_mode(mmwave_handle_t handle, sensor_power_mode_t* mode);

/**
 * @brief Dynamically reconfigure chirp profile while sensor is running
 * @param[in] handle  SDK handle
 * @param[in] profile New chirp profile (applied at next frame boundary)
 * @return MMWAVE_OK on success
 */
int32 sensor_dynamic_reconfig(mmwave_handle_t handle,
                              const mmwave_chirp_profile_t* profile);

/**
 * @brief Read sensor die temperature
 * @param[in]  handle       SDK handle
 * @param[out] temp_celsius Temperature in degrees Celsius
 * @return MMWAVE_OK on success
 */
int32 sensor_read_temperature(mmwave_handle_t handle, float32* temp_celsius);

/**
 * @brief Execute sensor built-in self-test
 * @param[in]  handle SDK handle
 * @param[out] result Self-test results
 * @return MMWAVE_OK if all tests pass
 */
int32 sensor_self_test(mmwave_handle_t handle, sensor_selftest_result_t* result);

/**
 * @brief Query firmware and hardware version
 * @param[in]  handle  SDK handle
 * @param[out] version Version information
 * @return MMWAVE_OK on success
 */
int32 sensor_get_version(mmwave_handle_t handle, sensor_version_t* version);

#endif /* SENSOR_CONTROL_API_H */
