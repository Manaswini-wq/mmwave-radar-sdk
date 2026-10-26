/**
 * @file profile_manager.h
 * @brief Profile Manager for mmWave Radar SDK
 * @details Stores multiple chirp profiles and provides runtime profile
 *          switching with validation. Used by the radar configuration
 *          manager for multi-profile chirp sequences.
 */
#ifndef PROFILE_MANAGER_H
#define PROFILE_MANAGER_H

#include "mmwave_types.h"

/** Opaque handle to profile manager */
typedef void* profile_mgr_handle_t;

/** Profile state flags */
#define PROFILE_STATE_EMPTY         0U
#define PROFILE_STATE_CONFIGURED    1U
#define PROFILE_STATE_ACTIVE        2U

/** Profile summary information */
typedef struct {
    uint32  profile_id;     /**< Profile identifier */
    uint32  state;          /**< Current state (PROFILE_STATE_*) */
    float32 start_freq_ghz; /**< Start frequency */
    float32 bandwidth_ghz;  /**< Configured bandwidth */
} profile_info_t;

/**
 * @brief Initialize the profile manager
 * @param[out] handle  Pointer to receive profile manager handle
 * @return MMWAVE_OK on success
 */
int32 profile_mgr_init(profile_mgr_handle_t* handle);

/**
 * @brief Add or update a chirp profile
 * @param[in] handle   Profile manager handle
 * @param[in] profile  Chirp profile configuration
 * @return MMWAVE_OK on success
 */
int32 profile_mgr_set(profile_mgr_handle_t handle,
                      const mmwave_chirp_profile_t* profile);

/**
 * @brief Get a stored chirp profile
 * @param[in]  handle      Profile manager handle
 * @param[in]  profile_id  Profile identifier
 * @param[out] profile     Pointer to receive profile data
 * @return MMWAVE_OK on success
 */
int32 profile_mgr_get(profile_mgr_handle_t handle,
                      uint32 profile_id,
                      mmwave_chirp_profile_t* profile);

/**
 * @brief Switch the active profile
 * @param[in] handle      Profile manager handle
 * @param[in] profile_id  Profile to activate
 * @return MMWAVE_OK on success
 */
int32 profile_mgr_switch(profile_mgr_handle_t handle, uint32 profile_id);

/**
 * @brief Validate a chirp profile against hardware limits
 * @param[in] profile  Chirp profile to validate
 * @return MMWAVE_OK if valid, negative error code if invalid
 */
int32 profile_mgr_validate(const mmwave_chirp_profile_t* profile);

/**
 * @brief Get information about all configured profiles
 * @param[in]  handle  Profile manager handle
 * @param[out] info    Array of MMWAVE_MAX_PROFILES entries
 * @param[out] count   Number of configured profiles
 * @return MMWAVE_OK on success
 */
int32 profile_mgr_get_info(profile_mgr_handle_t handle,
                           profile_info_t* info,
                           uint32* count);

/**
 * @brief De-initialize the profile manager
 * @param[in] handle  Profile manager handle
 * @return MMWAVE_OK on success
 */
int32 profile_mgr_deinit(profile_mgr_handle_t handle);

#endif /* PROFILE_MANAGER_H */
