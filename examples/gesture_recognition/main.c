/**
 * @file main.c
 * @brief Gesture Recognition Application
 * @details Micro-Doppler based gesture classification using mmWave radar.
 *          Configures sensor for short-range, high frame rate operation to
 *          capture fine hand motion. Classifies swipe left/right, push, pull.
 *
 * Hardware: AWR2944 or IWR6843 EVM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "mmwave_sdk.h"
#include "gesture_engine.h"

/* -------------------------------------------------------------------------- */
/*  Configuration - Micro-Doppler Gesture Sensing                             */
/* -------------------------------------------------------------------------- */
#define CHIRP_START_FREQ_GHZ    77.0f
#define CHIRP_SLOPE_MHZ_US      100.0f  /* steep slope for fine velocity res */
#define CHIRP_ADC_SAMPLES       64U     /* fewer samples - short range only */
#define FRAME_CHIRPS            128U    /* many chirps for good Doppler res */
#define FRAME_RATE_FPS          50U     /* high frame rate for gesture */
#define FRAME_PERIOD_MS         (1000.0f / (float32)FRAME_RATE_FPS)

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
/*  Gesture Event Callback                                                    */
/* -------------------------------------------------------------------------- */
static void on_gesture(gesture_type_t gesture, float32 confidence, void* user_data)
{
    (void)user_data;
    const char* names[] = {
        [GESTURE_NONE]        = "NONE",
        [GESTURE_SWIPE_LEFT]  = "SWIPE LEFT",
        [GESTURE_SWIPE_RIGHT] = "SWIPE RIGHT",
        [GESTURE_PUSH]        = "PUSH",
        [GESTURE_PULL]        = "PULL",
    };
    if (gesture != GESTURE_NONE) {
        printf(">>> GESTURE: %-12s  confidence=%.1f%%\n",
               names[gesture], confidence * 100.0f);
    }
}

/* -------------------------------------------------------------------------- */
/*  Main                                                                      */
/* -------------------------------------------------------------------------- */
int main(void)
{
    mmwave_handle_t handle = NULL;
    int32 status;

    signal(SIGINT, signal_handler);

    /* --- Initialize SDK --- */
    mmwave_config_t config = MMWAVE_DEFAULT_CONFIG;
    status = mmwave_init(&handle, &config);
    if (status != MMWAVE_OK) {
        printf("ERROR: mmwave_init failed (%d): %s\n", status, mmwave_strerror(status));
        return EXIT_FAILURE;
    }

    /* --- Open and configure for micro-Doppler --- */
    status = mmwave_open(handle);
    if (status != MMWAVE_OK) { printf("ERROR: open (%d)\n", status); goto cleanup; }

    mmwave_chirp_profile_t chirp = config.chirp_profile;
    chirp.start_freq_ghz    = CHIRP_START_FREQ_GHZ;
    chirp.freq_slope_mhz_us = CHIRP_SLOPE_MHZ_US;
    chirp.adc_samples       = CHIRP_ADC_SAMPLES;
    chirp.idle_time_us      = 5.0f;
    chirp.ramp_end_time_us  = 20.0f;
    status = mmwave_config_chirp(handle, &chirp);
    if (status != MMWAVE_OK) { printf("ERROR: chirp (%d)\n", status); goto cleanup; }

    mmwave_frame_config_t frame_cfg = config.frame_config;
    frame_cfg.chirps_per_frame     = FRAME_CHIRPS;
    frame_cfg.frame_periodicity_ms = FRAME_PERIOD_MS;
    frame_cfg.num_frames           = 0;
    frame_cfg.tx_channel_mask      = 0x01; /* single TX for simplicity */
    status = mmwave_config_frame(handle, &frame_cfg);
    if (status != MMWAVE_OK) { printf("ERROR: frame (%d)\n", status); goto cleanup; }

    /* --- Initialize gesture engine --- */
    gesture_engine_t engine;
    gesture_engine_config_t ge_cfg = {
        .num_range_bins    = CHIRP_ADC_SAMPLES,
        .num_doppler_bins  = FRAME_CHIRPS,
        .frame_rate_hz     = (float32)FRAME_RATE_FPS,
        .min_range_bin     = 2,
        .max_range_bin     = 20,        /* ~0.3 m focus zone */
        .min_confidence    = 0.6f,
        .debounce_frames   = 5,
    };
    gesture_engine_init(&engine, &ge_cfg);
    gesture_engine_register_callback(&engine, on_gesture, NULL);

    /* --- Calibrate and start --- */
    status = mmwave_calibrate(handle, NULL);
    if (status != MMWAVE_OK) { printf("ERROR: calibrate (%d)\n", status); goto cleanup; }

    status = mmwave_start(handle);
    if (status != MMWAVE_OK) { printf("ERROR: start (%d)\n", status); goto cleanup; }
    printf("Gesture recognition started (50 fps). Wave your hand!\n");

    /* --- Main processing loop --- */
    mmwave_frame_t frame;

    while (g_running) {
        status = mmwave_get_frame(handle, &frame, 500);
        if (status == MMWAVE_ERR_TIMEOUT) { continue; }
        if (status != MMWAVE_OK)          { break; }

        /* Feed frame into gesture engine; callback fires on detection */
        gesture_engine_process_frame(&engine, &frame);
    }

    /* --- Cleanup --- */
cleanup:
    printf("Shutting down gesture engine...\n");
    gesture_engine_deinit(&engine);
    mmwave_stop(handle);
    mmwave_close(handle);
    mmwave_deinit(handle);
    printf("Done.\n");
    return EXIT_SUCCESS;
}
