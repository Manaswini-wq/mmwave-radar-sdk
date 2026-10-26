/**
 * @file vital_signs_monitor.h
 * @brief Vital Signs Monitoring Algorithm Interface
 * @details Non-contact breathing rate and heart rate estimation via
 *          radar phase extraction and bandpass filtering.
 */
#ifndef VITAL_SIGNS_MONITOR_H
#define VITAL_SIGNS_MONITOR_H

#include "mmwave_types.h"

/* -------------------------------------------------------------------------- */
/*  Constants                                                                 */
/* -------------------------------------------------------------------------- */
#define VS_PHASE_HISTORY_LEN    512U    /**< phase samples stored */
#define VS_SMOOTHING_MAX        16U     /**< max moving average window */

/* -------------------------------------------------------------------------- */
/*  Types                                                                     */
/* -------------------------------------------------------------------------- */

/** Vital signs configuration */
typedef struct {
    uint16  target_range_bin;       /**< range bin of subject */
    float32 sample_rate_hz;         /**< frame rate (= sample rate) */
    float32 breath_bpm_min;         /**< breathing low cutoff (BPM) */
    float32 breath_bpm_max;         /**< breathing high cutoff (BPM) */
    float32 heart_bpm_min;          /**< heart rate low cutoff (BPM) */
    float32 heart_bpm_max;          /**< heart rate high cutoff (BPM) */
    uint16  smoothing_window;       /**< moving average length for rate output */
} vital_signs_config_t;

/** Vital signs measurement result */
typedef struct {
    float32 breathing_rate_bpm;     /**< estimated breathing rate */
    float32 breathing_confidence;   /**< 0.0 - 1.0 */
    float32 heart_rate_bpm;         /**< estimated heart rate */
    float32 heart_confidence;       /**< 0.0 - 1.0 */
} vital_signs_result_t;

/** Vital signs monitor runtime context */
typedef struct {
    vital_signs_config_t cfg;
    float32 phase_history[VS_PHASE_HISTORY_LEN];
    uint32  phase_idx;
    uint32  sample_count;
    float32 breath_rate_buf[VS_SMOOTHING_MAX];
    float32 heart_rate_buf[VS_SMOOTHING_MAX];
    uint16  smooth_idx;
    vital_signs_result_t latest;
} vital_signs_monitor_t;

/* -------------------------------------------------------------------------- */
/*  API                                                                       */
/* -------------------------------------------------------------------------- */

/** Initialize vital signs monitor */
int32 vital_signs_init(vital_signs_monitor_t* ctx, const vital_signs_config_t* cfg);

/** Process one radar frame */
int32 vital_signs_process_frame(vital_signs_monitor_t* ctx, const mmwave_frame_t* frame);

/** Get latest measurement result */
int32 vital_signs_get_result(const vital_signs_monitor_t* ctx, vital_signs_result_t* result);

/** De-initialize monitor */
void vital_signs_deinit(vital_signs_monitor_t* ctx);

#endif /* VITAL_SIGNS_MONITOR_H */
