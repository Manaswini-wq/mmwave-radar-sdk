/**
 * @file cfar_detection.h
 * @brief CFAR (Constant False Alarm Rate) detection for mmWave Radar SDK
 * @details Provides CA-CFAR and OS-CFAR algorithms for adaptive threshold
 *          detection on range-FFT magnitude data.
 */
#ifndef CFAR_DETECTION_H
#define CFAR_DETECTION_H

#include "mmwave_types.h"

/** Maximum detections per scan */
#define CFAR_MAX_DETECTIONS     64U

/** CFAR algorithm variant */
typedef enum {
    CFAR_TYPE_CA   = 0, /**< Cell-Averaging CFAR */
    CFAR_TYPE_OS   = 1  /**< Ordered-Statistics CFAR */
} cfar_type_t;

/** Single CFAR detection */
typedef struct {
    uint16  range_bin;     /**< Range bin index of detection */
    uint16  magnitude;     /**< Magnitude at detected bin (log2 Q8) */
    uint16  threshold;     /**< Adaptive threshold used for this detection */
    uint16  snr_estimate;  /**< Estimated SNR above threshold (Q8) */
} cfar_detection_t;

/** CFAR detection list output */
typedef struct {
    cfar_detection_t  detections[CFAR_MAX_DETECTIONS];
    uint16            num_detections;  /**< Number of valid detections */
} cfar_detection_list_t;

/** CFAR configuration */
typedef struct {
    cfar_type_t type;            /**< CA-CFAR or OS-CFAR */
    uint16      num_guard_cells; /**< Guard cells on each side of CUT */
    uint16      num_train_cells; /**< Training cells on each side of CUT */
    uint16      threshold_scale; /**< Threshold multiplier (Q8 fixed-point) */
    uint16      os_rank;         /**< OS-CFAR rank (k-th ordered value, 1-based) */
} cfar_config_t;

/**
 * @brief Run CFAR detection on magnitude data
 * @param[in]  mag_data   Log-magnitude spectrum (Q8)
 * @param[in]  num_bins   Number of range bins
 * @param[in]  config     CFAR configuration
 * @param[out] det_list   Detection list output
 * @return MMWAVE_OK on success
 */
int32 cfar_detect(const uint16* mag_data, uint16 num_bins,
                  const cfar_config_t* config,
                  cfar_detection_list_t* det_list);

#endif /* CFAR_DETECTION_H */
