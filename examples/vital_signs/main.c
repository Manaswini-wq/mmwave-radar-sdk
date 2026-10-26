/**
 * @file main.c
 * @brief Vital Signs Monitoring Application
 * @details Non-contact breathing rate and heart rate estimation using
 *          mmWave radar phase extraction. Configures sensor for very
 *          short range (<1m) with high phase sensitivity.
 *
 * Hardware: AWR2944 or IWR6843 EVM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "mmwave_sdk.h"
#include "vital_signs_monitor.h"

/* -------------------------------------------------------------------------- */
/*  Configuration - Phase-based Vital Signs Sensing                           */
/* -------------------------------------------------------------------------- */
#define CHIRP_START_FREQ_GHZ    77.0f
#define CHIRP_SLOPE_MHZ_US      50.0f   /* moderate slope for phase accuracy */
#define CHIRP_ADC_SAMPLES       256U
#define FRAME_CHIRPS            1U      /* single chirp per frame for phase */
#define FRAME_RATE_FPS          20U     /* 20 Hz for heart rate resolution */
#define FRAME_PERIOD_MS         (1000.0f / (float32)FRAME_RATE_FPS)

/* Target range bin for subject (approx 0.5 m) */
#define TARGET_RANGE_BIN        10U

/* Output update rate */
#define OUTPUT_PERIOD_FRAMES    20U     /* update display every second */

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

    /* --- Open and configure for phase-sensitive detection --- */
    status = mmwave_open(handle);
    if (status != MMWAVE_OK) { printf("ERROR: open (%d)\n", status); goto cleanup; }

    mmwave_chirp_profile_t chirp = config.chirp_profile;
    chirp.start_freq_ghz    = CHIRP_START_FREQ_GHZ;
    chirp.freq_slope_mhz_us = CHIRP_SLOPE_MHZ_US;
    chirp.adc_samples       = CHIRP_ADC_SAMPLES;
    chirp.rx_gain_db        = 48;       /* max gain for tiny chest motion */
    status = mmwave_config_chirp(handle, &chirp);
    if (status != MMWAVE_OK) { printf("ERROR: chirp (%d)\n", status); goto cleanup; }

    mmwave_frame_config_t frame_cfg = config.frame_config;
    frame_cfg.chirps_per_frame     = FRAME_CHIRPS;
    frame_cfg.frame_periodicity_ms = FRAME_PERIOD_MS;
    frame_cfg.num_frames           = 0;
    frame_cfg.tx_channel_mask      = 0x01; /* single TX */
    frame_cfg.rx_channel_mask      = 0x01; /* single RX for simplicity */
    status = mmwave_config_frame(handle, &frame_cfg);
    if (status != MMWAVE_OK) { printf("ERROR: frame (%d)\n", status); goto cleanup; }

    /* --- Initialize vital signs monitor --- */
    vital_signs_monitor_t monitor;
    vital_signs_config_t vs_cfg = {
        .target_range_bin = TARGET_RANGE_BIN,
        .sample_rate_hz   = (float32)FRAME_RATE_FPS,
        .breath_bpm_min   = 6.0f,      /* 0.1 Hz */
        .breath_bpm_max   = 30.0f,     /* 0.5 Hz */
        .heart_bpm_min    = 48.0f,     /* 0.8 Hz */
        .heart_bpm_max    = 120.0f,    /* 2.0 Hz */
        .smoothing_window = 5,
    };
    vital_signs_init(&monitor, &vs_cfg);

    /* --- Calibrate and start --- */
    status = mmwave_calibrate(handle, NULL);
    if (status != MMWAVE_OK) { printf("ERROR: calibrate (%d)\n", status); goto cleanup; }

    status = mmwave_start(handle);
    if (status != MMWAVE_OK) { printf("ERROR: start (%d)\n", status); goto cleanup; }
    printf("Vital signs monitoring started. Subject should sit ~0.5 m from sensor.\n");

    /* --- Main processing loop --- */
    mmwave_frame_t frame;
    uint32 frame_count = 0;

    while (g_running) {
        status = mmwave_get_frame(handle, &frame, 500);
        if (status == MMWAVE_ERR_TIMEOUT) { continue; }
        if (status != MMWAVE_OK)          { break; }

        /* Process frame for vital signs */
        vital_signs_process_frame(&monitor, &frame);
        frame_count++;

        /* Periodic output */
        if ((frame_count % OUTPUT_PERIOD_FRAMES) == 0) {
            vital_signs_result_t result;
            vital_signs_get_result(&monitor, &result);

            printf("Breathing: %5.1f BPM (conf=%.0f%%)  |  "
                   "Heart: %5.1f BPM (conf=%.0f%%)\n",
                   result.breathing_rate_bpm,
                   result.breathing_confidence * 100.0f,
                   result.heart_rate_bpm,
                   result.heart_confidence * 100.0f);
        }
    }

    /* --- Cleanup --- */
cleanup:
    printf("Shutting down vital signs monitor...\n");
    vital_signs_deinit(&monitor);
    mmwave_stop(handle);
    mmwave_close(handle);
    mmwave_deinit(handle);
    printf("Done. Processed %u frames.\n", frame_count);
    return EXIT_SUCCESS;
}
