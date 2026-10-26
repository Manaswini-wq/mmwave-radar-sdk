/**
 * @file radar_config_mgr.h
 * @brief Radar Configuration Manager for mmWave Radar SDK
 * @details Manages chirp profile creation, validation, timing calculations,
 *          and register programming for the radar front-end via SPI.
 *          Supports multi-profile operation with runtime switching.
 */
#ifndef RADAR_CONFIG_MGR_H
#define RADAR_CONFIG_MGR_H

#include "mmwave_types.h"

/** Opaque handle to radar configuration manager */
typedef void* radar_config_handle_t;

/** Computed chirp timing parameters (read-only output) */
typedef struct {
    float32 bandwidth_ghz;       /**< Effective chirp bandwidth in GHz */
    float32 sweep_time_us;       /**< Active sweep time in us */
    float32 if_bandwidth_mhz;    /**< IF bandwidth in MHz */
    float32 range_resolution_m;  /**< Range resolution in meters */
    float32 max_range_m;         /**< Maximum unambiguous range in meters */
    float32 chirp_duration_us;   /**< Total chirp duration (idle + ramp) */
} radar_chirp_timing_t;

/**
 * @brief Initialize the radar configuration manager
 * @param[out] handle  Pointer to receive config manager handle
 * @return MMWAVE_OK on success
 */
int32 radar_config_init(radar_config_handle_t* handle);

/**
 * @brief Set/program a chirp profile
 * @param[in] handle   Config manager handle
 * @param[in] profile  Chirp profile configuration
 * @return MMWAVE_OK on success
 */
int32 radar_config_set_chirp(radar_config_handle_t handle,
                             const mmwave_chirp_profile_t* profile);

/**
 * @brief Set/program a frame configuration
 * @param[in] handle  Config manager handle
 * @param[in] config  Frame configuration
 * @return MMWAVE_OK on success
 */
int32 radar_config_set_frame(radar_config_handle_t handle,
                             const mmwave_frame_config_t* config);

/**
 * @brief Get computed chirp timing for an active profile
 * @param[in]  handle      Config manager handle
 * @param[in]  profile_id  Profile identifier
 * @param[out] timing      Pointer to receive timing data
 * @return MMWAVE_OK on success
 */
int32 radar_config_get_timing(radar_config_handle_t handle,
                              uint32 profile_id,
                              radar_chirp_timing_t* timing);

/**
 * @brief Readback and verify register configuration
 * @param[in] handle  Config manager handle
 * @return MMWAVE_OK if verification passes
 */
int32 radar_config_verify(radar_config_handle_t handle);

/**
 * @brief De-initialize the radar configuration manager
 * @param[in] handle  Config manager handle
 * @return MMWAVE_OK on success
 */
int32 radar_config_deinit(radar_config_handle_t handle);

#endif /* RADAR_CONFIG_MGR_H */
