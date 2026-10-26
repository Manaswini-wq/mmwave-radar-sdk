/**
 * @file range_fft.h
 * @brief Range FFT processing for mmWave Radar SDK
 * @details Provides in-place radix-2 FFT for complex I/Q radar ADC data,
 *          windowing functions (Hann, Blackman-Harris), zero-padding,
 *          and log-magnitude output.
 */
#ifndef RANGE_FFT_H
#define RANGE_FFT_H

#include "mmwave_types.h"

/** Supported window types */
typedef enum {
    FFT_WINDOW_NONE          = 0, /**< No windowing (rectangular) */
    FFT_WINDOW_HANN          = 1, /**< Hann (raised cosine) window */
    FFT_WINDOW_BLACKMAN_HARRIS = 2  /**< 4-term Blackman-Harris window */
} fft_window_type_t;

/** Range FFT configuration */
typedef struct {
    uint16              num_samples;       /**< Input ADC samples (must be power of 2) */
    uint16              fft_size;          /**< FFT size (>= num_samples, power of 2, zero-padded) */
    fft_window_type_t   window_type;       /**< Pre-FFT window function */
    bool                compute_magnitude; /**< true = output |X[k]| in log2 scale */
} range_fft_config_t;

/**
 * @brief Initialise twiddle factors and window coefficients
 * @param[in] config FFT configuration
 * @return MMWAVE_OK on success
 */
int32 range_fft_init(const range_fft_config_t* config);

/**
 * @brief Execute range FFT on one chirp of complex I/Q data
 * @param[in,out] data_i  In-place real (I) samples – replaced by FFT output
 * @param[in,out] data_q  In-place imaginary (Q) samples
 * @param[out]    mag_out Optional log-magnitude output (NULL to skip)
 * @param[in]     config  FFT configuration
 * @return MMWAVE_OK on success
 */
int32 range_fft_run(int16* data_i, int16* data_q, uint16* mag_out,
                    const range_fft_config_t* config);

/**
 * @brief Free twiddle / window tables
 */
void range_fft_deinit(void);

#endif /* RANGE_FFT_H */
