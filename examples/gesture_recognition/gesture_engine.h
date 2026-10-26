/**
 * @file gesture_engine.h
 * @brief Gesture Recognition Engine Interface
 * @details Micro-Doppler spectrogram feature extraction and template-matching
 *          classifier with debouncing for reliable gesture detection.
 */
#ifndef GESTURE_ENGINE_H
#define GESTURE_ENGINE_H

#include "mmwave_types.h"

/* -------------------------------------------------------------------------- */
/*  Constants                                                                 */
/* -------------------------------------------------------------------------- */
#define GE_MAX_DOPPLER_BINS     256U
#define GE_MAX_RANGE_BINS       256U
#define GE_SPECTROGRAM_DEPTH    16U     /**< frames of history for classification */
#define GE_NUM_FEATURES         8U      /**< extracted feature vector length */

/* -------------------------------------------------------------------------- */
/*  Types                                                                     */
/* -------------------------------------------------------------------------- */

/** Supported gesture types */
typedef enum {
    GESTURE_NONE = 0,
    GESTURE_SWIPE_LEFT,
    GESTURE_SWIPE_RIGHT,
    GESTURE_PUSH,
    GESTURE_PULL,
    GESTURE_COUNT   /**< sentinel - number of gesture types */
} gesture_type_t;

/** Gesture event callback */
typedef void (*gesture_callback_t)(gesture_type_t gesture, float32 confidence,
                                    void* user_data);

/** Gesture engine configuration */
typedef struct {
    uint16  num_range_bins;
    uint16  num_doppler_bins;
    float32 frame_rate_hz;
    uint16  min_range_bin;          /**< near boundary of detection zone */
    uint16  max_range_bin;          /**< far boundary of detection zone */
    float32 min_confidence;         /**< classification confidence threshold */
    uint16  debounce_frames;        /**< min frames between gesture events */
} gesture_engine_config_t;

/** Gesture engine runtime context */
typedef struct {
    gesture_engine_config_t cfg;
    float32  spectrogram[GE_SPECTROGRAM_DEPTH][GE_MAX_DOPPLER_BINS];
    uint16   spec_write_idx;
    float32  features[GE_NUM_FEATURES];
    gesture_callback_t callback;
    void*    callback_user_data;
    uint32   frames_since_gesture;
    uint32   frame_count;
} gesture_engine_t;

/* -------------------------------------------------------------------------- */
/*  API                                                                       */
/* -------------------------------------------------------------------------- */

/** Initialize gesture engine */
int32 gesture_engine_init(gesture_engine_t* ctx, const gesture_engine_config_t* cfg);

/** Process one radar frame */
int32 gesture_engine_process_frame(gesture_engine_t* ctx, const mmwave_frame_t* frame);

/** Register gesture event callback */
int32 gesture_engine_register_callback(gesture_engine_t* ctx,
                                        gesture_callback_t cb, void* user_data);

/** De-initialize gesture engine */
void gesture_engine_deinit(gesture_engine_t* ctx);

#endif /* GESTURE_ENGINE_H */
