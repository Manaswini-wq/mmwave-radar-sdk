/**
 * @file frame_scheduler.h
 * @brief Frame Scheduler for mmWave Radar SDK
 * @details Timer-based frame triggering with chirp sequence management,
 *          inter-frame gap handling, and frame count tracking.
 */
#ifndef FRAME_SCHEDULER_H
#define FRAME_SCHEDULER_H

#include "mmwave_types.h"

/** Opaque handle to frame scheduler */
typedef void* frame_sched_handle_t;

/** Frame scheduler state */
#define FRAME_SCHED_IDLE            0U
#define FRAME_SCHED_RUNNING         1U
#define FRAME_SCHED_PAUSED          2U

/** Frame scheduler statistics */
typedef struct {
    uint32  state;                  /**< Current scheduler state */
    uint32  frames_completed;       /**< Total frames completed */
    uint32  chirps_in_current_frame; /**< Chirps completed in current frame */
    uint32  frames_remaining;       /**< Frames remaining (0 = infinite) */
    uint32  frame_start_timestamp;  /**< Last frame start timestamp (us) */
} frame_sched_stats_t;

/**
 * @brief Initialize the frame scheduler
 * @param[out] handle  Pointer to receive scheduler handle
 * @return MMWAVE_OK on success
 */
int32 frame_scheduler_init(frame_sched_handle_t* handle);

/**
 * @brief Start the frame scheduler with given frame configuration
 * @param[in] handle  Scheduler handle
 * @param[in] config  Frame configuration
 * @return MMWAVE_OK on success
 */
int32 frame_scheduler_start(frame_sched_handle_t handle,
                            const mmwave_frame_config_t* config);

/**
 * @brief Stop the frame scheduler
 * @param[in] handle  Scheduler handle
 * @return MMWAVE_OK on success
 */
int32 frame_scheduler_stop(frame_sched_handle_t handle);

/**
 * @brief Get frame scheduler statistics
 * @param[in]  handle  Scheduler handle
 * @param[out] stats   Pointer to receive statistics
 * @return MMWAVE_OK on success
 */
int32 frame_scheduler_get_stats(frame_sched_handle_t handle,
                                frame_sched_stats_t* stats);

/**
 * @brief De-initialize the frame scheduler
 * @param[in] handle  Scheduler handle
 * @return MMWAVE_OK on success
 */
int32 frame_scheduler_deinit(frame_sched_handle_t handle);

#endif /* FRAME_SCHEDULER_H */
