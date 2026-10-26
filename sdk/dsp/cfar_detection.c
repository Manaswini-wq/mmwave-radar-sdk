/**
 * @file cfar_detection.c
 * @brief CA-CFAR and OS-CFAR detection for mmWave Radar SDK
 * @details
 *  - CA-CFAR: averages training cells on both sides of CUT (Cell Under Test),
 *    applies a scale factor to set an adaptive threshold.
 *  - OS-CFAR: sorts training cells, picks the k-th ordered value as the
 *    noise estimate for improved performance near clutter edges.
 *  - Guard cells prevent leakage from strong targets.
 *  - Outputs a list of detected range bins with magnitude and SNR.
 */
#include "cfar_detection.h"
#include "mmwave_error_codes.h"

#include <string.h>

/* ---- Forward declarations ---- */
static uint16 ca_cfar_threshold(const uint16* mag, uint16 num_bins,
                                uint16 cut_idx, const cfar_config_t* cfg);
static uint16 os_cfar_threshold(const uint16* mag, uint16 num_bins,
                                uint16 cut_idx, const cfar_config_t* cfg);
static void   insertion_sort_u16(uint16* arr, uint16 len);

int32 cfar_detect(const uint16* mag_data, uint16 num_bins,
                  const cfar_config_t* config,
                  cfar_detection_list_t* det_list)
{
    if (mag_data == NULL || config == NULL || det_list == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (num_bins == 0U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    /* Minimum span required around CUT */
    uint16 wing = config->num_guard_cells + config->num_train_cells;
    if (wing == 0U || wing >= num_bins / 2U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    det_list->num_detections = 0U;

    for (uint16 cut = wing; cut < num_bins - wing; cut++) {
        uint16 thresh;

        if (config->type == CFAR_TYPE_OS) {
            thresh = os_cfar_threshold(mag_data, num_bins, cut, config);
        } else {
            thresh = ca_cfar_threshold(mag_data, num_bins, cut, config);
        }

        /* Compare CUT against adaptive threshold */
        if (mag_data[cut] > thresh) {
            if (det_list->num_detections < CFAR_MAX_DETECTIONS) {
                cfar_detection_t* det = &det_list->detections[det_list->num_detections];
                det->range_bin   = cut;
                det->magnitude   = mag_data[cut];
                det->threshold   = thresh;
                det->snr_estimate = mag_data[cut] - thresh; /* log-domain subtraction = dB */
                det_list->num_detections++;
            }
        }
    }

    return MMWAVE_OK;
}

/**
 * @brief CA-CFAR: compute adaptive threshold for a single CUT
 *
 * Averages all training cells on both leading and lagging sides,
 * skipping guard cells, then scales by threshold_scale (Q8).
 */
static uint16 ca_cfar_threshold(const uint16* mag, uint16 num_bins,
                                uint16 cut_idx, const cfar_config_t* cfg)
{
    uint32 sum = 0U;
    uint16 count = 0U;

    /* Leading (left) training cells */
    int32 start_l = (int32)cut_idx - (int32)cfg->num_guard_cells - (int32)cfg->num_train_cells;
    int32 end_l   = (int32)cut_idx - (int32)cfg->num_guard_cells;
    for (int32 i = start_l; i < end_l; i++) {
        if (i >= 0 && i < (int32)num_bins) {
            sum += mag[i];
            count++;
        }
    }

    /* Lagging (right) training cells */
    int32 start_r = (int32)cut_idx + (int32)cfg->num_guard_cells + 1;
    int32 end_r   = start_r + (int32)cfg->num_train_cells;
    for (int32 i = start_r; i < end_r; i++) {
        if (i >= 0 && i < (int32)num_bins) {
            sum += mag[i];
            count++;
        }
    }

    if (count == 0U) {
        return 0xFFFFU; /* No training cells available – suppress detection */
    }

    /* Average noise estimate, scaled by threshold_scale (Q8) */
    uint32 noise_avg = sum / count;
    uint32 threshold = (noise_avg * (uint32)cfg->threshold_scale) >> 8;

    return (threshold > 0xFFFFU) ? 0xFFFFU : (uint16)threshold;
}

/**
 * @brief OS-CFAR: compute threshold using k-th ordered training cell
 *
 * Collects training cells (both sides), sorts them, and selects the
 * k-th value as the noise estimate, then applies threshold_scale.
 */
static uint16 os_cfar_threshold(const uint16* mag, uint16 num_bins,
                                uint16 cut_idx, const cfar_config_t* cfg)
{
    /* Collect training cell values */
    uint16 train_vals[128]; /* max 64 cells per side */
    uint16 n_vals = 0U;

    int32 start_l = (int32)cut_idx - (int32)cfg->num_guard_cells - (int32)cfg->num_train_cells;
    int32 end_l   = (int32)cut_idx - (int32)cfg->num_guard_cells;
    for (int32 i = start_l; i < end_l; i++) {
        if (i >= 0 && i < (int32)num_bins && n_vals < 128U) {
            train_vals[n_vals++] = mag[i];
        }
    }

    int32 start_r = (int32)cut_idx + (int32)cfg->num_guard_cells + 1;
    int32 end_r   = start_r + (int32)cfg->num_train_cells;
    for (int32 i = start_r; i < end_r; i++) {
        if (i >= 0 && i < (int32)num_bins && n_vals < 128U) {
            train_vals[n_vals++] = mag[i];
        }
    }

    if (n_vals == 0U) {
        return 0xFFFFU;
    }

    /* Sort training cell values in ascending order */
    insertion_sort_u16(train_vals, n_vals);

    /* Select k-th ordered statistic (1-based rank) */
    uint16 rank = cfg->os_rank;
    if (rank == 0U || rank > n_vals) {
        rank = (uint16)((3U * n_vals) / 4U); /* default: 75th percentile */
    }
    uint16 noise_est = train_vals[rank - 1U];

    uint32 threshold = ((uint32)noise_est * (uint32)cfg->threshold_scale) >> 8;
    return (threshold > 0xFFFFU) ? 0xFFFFU : (uint16)threshold;
}

/**
 * @brief In-place insertion sort for small uint16 arrays
 */
static void insertion_sort_u16(uint16* arr, uint16 len)
{
    for (uint16 i = 1; i < len; i++) {
        uint16 key = arr[i];
        int32 j = (int32)i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}
