/**
 * @file test_range_fft.c
 * @brief Unit tests for range-FFT signal processing
 */
#include "unity.h"
#include "mmwave_types.h"
#include "range_fft.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TEST_FFT_SIZE  256

/* --------------- Fixtures --------------- */

static int16  g_adc_real[TEST_FFT_SIZE];
static int16  g_adc_imag[TEST_FFT_SIZE];
static float32 g_fft_mag[TEST_FFT_SIZE];

void setUp(void)
{
    memset(g_adc_real, 0, sizeof(g_adc_real));
    memset(g_adc_imag, 0, sizeof(g_adc_imag));
    memset(g_fft_mag,  0, sizeof(g_fft_mag));
}

void tearDown(void)
{
    /* nothing to clean up */
}

/* --------------- Tests --------------- */

/** A single-tone input must produce a peak at the correct FFT bin. */
void test_fft_known_single_tone(void)
{
    const uint32 tone_bin = 32;  /* expected peak */

    for (uint32 n = 0; n < TEST_FFT_SIZE; n++) {
        double phase = 2.0 * M_PI * tone_bin * n / TEST_FFT_SIZE;
        g_adc_real[n] = (int16)(1000.0 * cos(phase));
        g_adc_imag[n] = (int16)(1000.0 * sin(phase));
    }

    range_fft_run(g_adc_real, g_adc_imag, g_fft_mag, TEST_FFT_SIZE, NULL);

    /* Find peak bin */
    uint32 peak_bin = 0;
    float32 peak_val = 0.0f;
    for (uint32 k = 0; k < TEST_FFT_SIZE; k++) {
        if (g_fft_mag[k] > peak_val) {
            peak_val = g_fft_mag[k];
            peak_bin = k;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(tone_bin, peak_bin);
    TEST_ASSERT_TRUE(peak_val > 0.0f);
}

/** Applying a Hann window must reduce spectral leakage side-lobes. */
void test_fft_with_hann_window(void)
{
    const uint32 tone_bin = 32;
    float32 window[TEST_FFT_SIZE];
    range_fft_hann_window(window, TEST_FFT_SIZE);

    for (uint32 n = 0; n < TEST_FFT_SIZE; n++) {
        double phase = 2.0 * M_PI * tone_bin * n / TEST_FFT_SIZE;
        g_adc_real[n] = (int16)(1000.0 * cos(phase));
        g_adc_imag[n] = 0;
    }

    range_fft_run(g_adc_real, g_adc_imag, g_fft_mag, TEST_FFT_SIZE, window);

    /* Side-lobe at tone_bin+5 must be significantly below peak */
    float32 peak  = g_fft_mag[tone_bin];
    float32 slobe = g_fft_mag[tone_bin + 5];
    TEST_ASSERT_TRUE(peak > 0.0f);
    TEST_ASSERT_TRUE(slobe < peak * 0.1f);   /* > 20 dB suppression */
}

/** Zero-padded FFT output size must double and preserve tone location. */
void test_zero_padding(void)
{
    const uint32 tone_bin = 16;
    const uint32 padded_size = TEST_FFT_SIZE * 2;
    float32 mag_padded[TEST_FFT_SIZE * 2];

    for (uint32 n = 0; n < TEST_FFT_SIZE; n++) {
        double phase = 2.0 * M_PI * tone_bin * n / TEST_FFT_SIZE;
        g_adc_real[n] = (int16)(1000.0 * cos(phase));
        g_adc_imag[n] = 0;
    }

    range_fft_run_zeropad(g_adc_real, g_adc_imag, mag_padded,
                          TEST_FFT_SIZE, padded_size, NULL);

    /* Tone should appear at bin = tone_bin * (padded/orig) */
    uint32 expected_bin = tone_bin * 2;
    uint32 peak_bin = 0;
    float32 peak_val = 0.0f;
    for (uint32 k = 0; k < padded_size; k++) {
        if (mag_padded[k] > peak_val) {
            peak_val = mag_padded[k];
            peak_bin = k;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(expected_bin, peak_bin);
}

/** Magnitude must equal sqrt(re^2 + im^2) for each bin. */
void test_magnitude_calculation(void)
{
    /* Fill with known complex values: re=3, im=4 => mag=5 */
    for (uint32 n = 0; n < TEST_FFT_SIZE; n++) {
        g_adc_real[n] = 3;
        g_adc_imag[n] = 4;
    }

    range_fft_compute_magnitude(g_adc_real, g_adc_imag,
                                g_fft_mag, TEST_FFT_SIZE);

    for (uint32 k = 0; k < TEST_FFT_SIZE; k++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, g_fft_mag[k]);
    }
}

/* --------------- Runner --------------- */

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fft_known_single_tone);
    RUN_TEST(test_fft_with_hann_window);
    RUN_TEST(test_zero_padding);
    RUN_TEST(test_magnitude_calculation);
    return UNITY_END();
}
