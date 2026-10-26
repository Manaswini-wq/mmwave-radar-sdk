/**
 * @file range_fft.c
 * @brief In-place radix-2 range FFT for mmWave Radar SDK
 * @details
 *  - Windowing: Hann or 4-term Blackman-Harris applied before FFT
 *  - Zero-padding: FFT size may exceed sample count
 *  - Radix-2 Cooley-Tukey decimation-in-time butterfly
 *  - Log2 magnitude approximation for CFAR input
 */
#include "range_fft.h"
#include "mmwave_error_codes.h"

#include <string.h>

/** Maximum supported FFT size */
#define MAX_FFT_SIZE    4096U

/** Fixed-point scaling: Q15 for twiddle factors */
#define Q15_SHIFT       15
#define Q15_ONE         (1 << Q15_SHIFT)

/** Precomputed tables */
static int16  g_twiddle_re[MAX_FFT_SIZE / 2];
static int16  g_twiddle_im[MAX_FFT_SIZE / 2];
static int16  g_window[MAX_FFT_SIZE];
static uint16 g_init_fft_size = 0U;

/* ---- Helpers ---- */

/** Integer log2 for power-of-two values */
static uint16 ilog2(uint16 n)
{
    uint16 k = 0;
    while ((1U << k) < n) k++;
    return k;
}

/** Fast integer square root (Babylonian) */
static uint32 isqrt(uint32 val)
{
    if (val == 0U) return 0U;
    uint32 x = val;
    uint32 y = (x + 1U) >> 1;
    while (y < x) {
        x = y;
        y = (x + val / x) >> 1;
    }
    return x;
}

/** log2 approximation for 32-bit unsigned (returns Q8 fixed point) */
static uint16 log2_approx(uint32 val)
{
    if (val == 0U) return 0U;
    uint16 msb = 0;
    uint32 v = val;
    while (v > 1U) { v >>= 1; msb++; }
    /* Linear interpolation of fractional part */
    uint32 frac = (val - (1U << msb)) * 256U / (1U << msb);
    return (uint16)((msb << 8) | (frac & 0xFFU));
}

/* ---- Sine / cosine table generation (Q15) ---- */

/**
 * @brief Compute sin(x) via 3rd-order Taylor for table generation
 *        x is in radians * 2^15 scaled, but we use integer milliradians here.
 */
static int16 sin_q15(int32 phase_q16)
{
    /* phase_q16: full circle = 65536 */
    /* Map to [-pi, pi] in Q15 and use polynomial */
    int32 x = phase_q16;
    /* Normalise to [-32768, 32767] */
    x = x & 0xFFFF;
    if (x > 32768) x -= 65536;

    /* sin(pi*x/32768) approx: x*(pi/32768) - (x*(pi/32768))^3/6 */
    /* For simplicity use lookup-quality polynomial in Q15 */
    int32 s = (x * 25736) >> 15;                         /* x * pi in Q15 */
    int32 s3 = (int32)(((int64)s * s >> 15) * s >> 15);  /* s^3 in Q15 */
    int32 result = s - s3 / 6;
    if (result >  32767) result =  32767;
    if (result < -32768) result = -32768;
    return (int16)result;
}

static int16 cos_q15(int32 phase_q16)
{
    return sin_q15(phase_q16 + 16384); /* cos = sin(x + pi/2) */
}

/* ---- Public API ---- */

int32 range_fft_init(const range_fft_config_t* config)
{
    if (config == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (config->fft_size == 0U || config->fft_size > MAX_FFT_SIZE) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    /* FFT size must be power of 2 */
    if ((config->fft_size & (config->fft_size - 1U)) != 0U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uint16 N = config->fft_size;

    /* Generate twiddle factors W_N^k = exp(-j*2*pi*k/N) */
    for (uint16 k = 0; k < N / 2; k++) {
        int32 phase = -(int32)(((uint32)k * 65536U) / N);
        g_twiddle_re[k] = cos_q15(phase);
        g_twiddle_im[k] = sin_q15(phase);
    }

    /* Generate window coefficients in Q15 */
    uint16 M = config->num_samples;
    for (uint16 n = 0; n < M; n++) {
        int32 w;
        switch (config->window_type) {
            case FFT_WINDOW_HANN: {
                /* w[n] = 0.5 - 0.5*cos(2*pi*n/(M-1)) */
                int32 phase = (int32)(((uint32)n * 65536U) / (M - 1U));
                w = Q15_ONE / 2 - (cos_q15(phase) / 2);
                break;
            }
            case FFT_WINDOW_BLACKMAN_HARRIS: {
                /* 4-term Blackman-Harris:
                 * w[n] = 0.35875 - 0.48829*cos(2pi*n/(M-1))
                 *       + 0.14128*cos(4pi*n/(M-1)) - 0.01168*cos(6pi*n/(M-1)) */
                int32 p1 = (int32)(((uint32)n * 65536U) / (M - 1U));
                int32 p2 = p1 * 2;
                int32 p3 = p1 * 3;
                w = (int32)(0.35875f * Q15_ONE)
                  - (int32)(0.48829f * cos_q15(p1))
                  + (int32)(0.14128f * cos_q15(p2))
                  - (int32)(0.01168f * cos_q15(p3));
                break;
            }
            default: /* FFT_WINDOW_NONE */
                w = Q15_ONE;
                break;
        }
        g_window[n] = (int16)w;
    }
    /* Zero-pad region window = 0 */
    for (uint16 n = M; n < N; n++) {
        g_window[n] = 0;
    }

    g_init_fft_size = N;
    return MMWAVE_OK;
}

int32 range_fft_run(int16* data_i, int16* data_q, uint16* mag_out,
                    const range_fft_config_t* config)
{
    if (data_i == NULL || data_q == NULL || config == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    uint16 N = config->fft_size;
    if (N != g_init_fft_size) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    uint16 M = config->num_samples;
    uint16 log2N = ilog2(N);

    /* ---- Apply window and zero-pad ---- */
    for (uint16 n = 0; n < M; n++) {
        data_i[n] = (int16)(((int32)data_i[n] * g_window[n]) >> Q15_SHIFT);
        data_q[n] = (int16)(((int32)data_q[n] * g_window[n]) >> Q15_SHIFT);
    }
    for (uint16 n = M; n < N; n++) {
        data_i[n] = 0;
        data_q[n] = 0;
    }

    /* ---- Bit-reversal permutation ---- */
    for (uint16 i = 0; i < N; i++) {
        uint16 j = 0;
        for (uint16 b = 0; b < log2N; b++) {
            j |= (uint16)(((i >> b) & 1U) << (log2N - 1 - b));
        }
        if (j > i) {
            int16 tmp;
            tmp = data_i[i]; data_i[i] = data_i[j]; data_i[j] = tmp;
            tmp = data_q[i]; data_q[i] = data_q[j]; data_q[j] = tmp;
        }
    }

    /* ---- Radix-2 DIT butterfly ---- */
    for (uint16 s = 1; s <= log2N; s++) {
        uint16 m = (uint16)(1U << s);
        uint16 half_m = m >> 1;
        uint16 tw_step = N / m; /* twiddle index step */

        for (uint16 k = 0; k < N; k += m) {
            for (uint16 j = 0; j < half_m; j++) {
                uint16 tw_idx = j * tw_step;
                int32 wr = g_twiddle_re[tw_idx];
                int32 wi = g_twiddle_im[tw_idx];

                int32 tr = (wr * data_i[k + j + half_m] - wi * data_q[k + j + half_m]) >> Q15_SHIFT;
                int32 ti = (wr * data_q[k + j + half_m] + wi * data_i[k + j + half_m]) >> Q15_SHIFT;

                int32 ur = data_i[k + j];
                int32 ui = data_q[k + j];

                data_i[k + j]          = (int16)(ur + tr);
                data_q[k + j]          = (int16)(ui + ti);
                data_i[k + j + half_m] = (int16)(ur - tr);
                data_q[k + j + half_m] = (int16)(ui - ti);
            }
        }
    }

    /* ---- Magnitude in log2 scale (optional) ---- */
    if (mag_out != NULL && config->compute_magnitude) {
        for (uint16 k = 0; k < N; k++) {
            uint32 mag_sq = (uint32)((int32)data_i[k] * data_i[k] +
                                     (int32)data_q[k] * data_q[k]);
            uint32 mag = isqrt(mag_sq);
            mag_out[k] = log2_approx(mag);
        }
    }

    return MMWAVE_OK;
}

void range_fft_deinit(void)
{
    g_init_fft_size = 0U;
    memset(g_twiddle_re, 0, sizeof(g_twiddle_re));
    memset(g_twiddle_im, 0, sizeof(g_twiddle_im));
    memset(g_window, 0, sizeof(g_window));
}
