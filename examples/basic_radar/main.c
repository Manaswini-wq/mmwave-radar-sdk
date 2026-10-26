/**
 * @file main.c
 * @brief Basic Radar Example - Minimal SDK usage demonstration
 * @details Shows how to initialize, configure, and capture radar frames
 *          using the mmWave Radar SDK's easy-to-use API.
 *
 * This example demonstrates the simplest possible radar application:
 *   1. Initialize the SDK with default configuration
 *   2. Open the sensor hardware
 *   3. Configure chirp and frame parameters
 *   4. Capture frames and print range-FFT peak information
 *
 * Hardware: AWR2944 or IWR6843 EVM
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <math.h>

#include "mmwave_sdk.h"

/* -------------------------------------------------------------------------- */
/*  Configuration Constants                                                   */
/* -------------------------------------------------------------------------- */
#define CHIRP_START_FREQ_GHZ     77.0f
#define CHIRP_SLOPE_MHZ_US       70.0f
#define CHIRP_ADC_SAMPLES        256U
#define FRAME_CHIRPS             128U
#define FRAME_RATE_FPS           30U
#define FRAME_PERIOD_MS          (1000.0f / (float32)FRAME_RATE_FPS)
#define MAX_FRAMES               1000U

/* -------------------------------------------------------------------------- */
/*  Global State                                                              */
/* -------------------------------------------------------------------------- */
static volatile bool g_running = true;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
}

/* -------------------------------------------------------------------------- */
/*  Frame Callback                                                            */
/* -------------------------------------------------------------------------- */
static void frame_callback(const mmwave_frame_t* frame, void* user_data)
{
    (void)user_data;
    printf("[CB] Frame %u received  ts=%u us  chirps=%u  samples=%u\n",
           frame->frame_number, frame->timestamp_us,
           frame->num_chirps, frame->num_adc_samples);
}

/* -------------------------------------------------------------------------- */
/*  Range-FFT Peak Finder (trivial magnitude search)                          */
/* -------------------------------------------------------------------------- */
static uint16 find_range_peak(const int16* adc_buf, uint16 num_samples)
{
    uint32  max_mag = 0;
    uint16  peak_idx = 0;

    for (uint16 i = 0; i < num_samples; i++) {
        uint32 mag = (uint32)((int32)adc_buf[i] * (int32)adc_buf[i]);
        if (mag > max_mag) {
            max_mag = mag;
            peak_idx = i;
        }
    }
    return peak_idx;
}

/* -------------------------------------------------------------------------- */
/*  Main                                                                      */
/* -------------------------------------------------------------------------- */
int main(void)
{
    mmwave_handle_t handle = NULL;
    int32 status;

    signal(SIGINT, signal_handler);

    /* --- Step 1: Initialize SDK with default configuration --- */
    mmwave_config_t config = MMWAVE_DEFAULT_CONFIG;
    status = mmwave_init(&handle, &config);
    if (status != MMWAVE_OK) {
        printf("ERROR: mmwave_init failed (%d): %s\n", status, mmwave_strerror(status));
        return EXIT_FAILURE;
    }
    printf("SDK initialized successfully.\n");

    /* --- Step 2: Open sensor hardware --- */
    status = mmwave_open(handle);
    if (status != MMWAVE_OK) { printf("ERROR: mmwave_open (%d)\n", status); goto cleanup; }

    /* --- Step 3: Configure chirp profile (77 GHz, 70 MHz/us, 256 samples) --- */
    mmwave_chirp_profile_t chirp = config.chirp_profile;
    chirp.start_freq_ghz    = CHIRP_START_FREQ_GHZ;
    chirp.freq_slope_mhz_us = CHIRP_SLOPE_MHZ_US;
    chirp.adc_samples       = CHIRP_ADC_SAMPLES;
    status = mmwave_config_chirp(handle, &chirp);
    if (status != MMWAVE_OK) { printf("ERROR: config_chirp (%d)\n", status); goto cleanup; }

    /* --- Step 4: Configure frame (128 chirps, 30 fps) --- */
    mmwave_frame_config_t frame_cfg = config.frame_config;
    frame_cfg.chirps_per_frame    = FRAME_CHIRPS;
    frame_cfg.frame_periodicity_ms = FRAME_PERIOD_MS;
    frame_cfg.num_frames          = 0; /* infinite */
    status = mmwave_config_frame(handle, &frame_cfg);
    if (status != MMWAVE_OK) { printf("ERROR: config_frame (%d)\n", status); goto cleanup; }

    /* --- Step 5: Register frame callback --- */
    status = mmwave_register_frame_callback(handle, frame_callback, NULL);
    if (status != MMWAVE_OK) { printf("ERROR: register_cb (%d)\n", status); goto cleanup; }

    /* --- Step 6: Calibrate and start --- */
    status = mmwave_calibrate(handle, NULL);
    if (status != MMWAVE_OK) { printf("ERROR: calibrate (%d)\n", status); goto cleanup; }

    status = mmwave_start(handle);
    if (status != MMWAVE_OK) { printf("ERROR: start (%d)\n", status); goto cleanup; }
    printf("Sensor started. Press Ctrl+C to stop.\n");

    /* --- Step 7: Main loop - get frames, print range-FFT peak --- */
    mmwave_frame_t frame;
    uint32 frame_count = 0;

    while (g_running && frame_count < MAX_FRAMES) {
        status = mmwave_get_frame(handle, &frame, 1000);
        if (status == MMWAVE_ERR_TIMEOUT) {
            continue;
        }
        if (status != MMWAVE_OK) {
            printf("ERROR: get_frame (%d)\n", status);
            break;
        }

        uint16 peak_bin = find_range_peak(frame.adc_data, frame.num_adc_samples);
        float32 range_res_m = 3.0e8f / (2.0f * CHIRP_SLOPE_MHZ_US * 1e12f *
                               (float32)CHIRP_ADC_SAMPLES / (chirp.sample_rate_ksps * 1e3f));
        float32 peak_range_m = (float32)peak_bin * range_res_m;

        printf("Frame %4u | Peak bin: %3u | Range: %.2f m\n",
               frame.frame_number, peak_bin, peak_range_m);
        frame_count++;
    }

    /* --- Step 8: Cleanup --- */
cleanup:
    printf("Shutting down...\n");
    mmwave_stop(handle);
    mmwave_close(handle);
    mmwave_deinit(handle);
    printf("Done. Processed %u frames.\n", frame_count);
    return EXIT_SUCCESS;
}
