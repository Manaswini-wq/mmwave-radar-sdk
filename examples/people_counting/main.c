/**
 * @file main.c
 * @brief People Counting Application
 * @details Zone-based occupancy counting using mmWave radar.
 *          Configures sensor for short-range (2-5m) detection, processes
 *          frames through range-FFT and CFAR, then clusters detections
 *          into zones for occupancy counting.
 *
 * Hardware: AWR2944 or IWR6843 EVM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "mmwave_sdk.h"
#include "people_counter.h"

/* -------------------------------------------------------------------------- */
/*  Configuration - Short Range People Detection                              */
/* -------------------------------------------------------------------------- */
#define SHORT_RANGE_MAX_M           5.0f
#define CHIRP_START_FREQ_GHZ        77.0f
#define CHIRP_SLOPE_MHZ_US          30.0f   /* lower slope for short range */
#define CHIRP_ADC_SAMPLES           128U
#define FRAME_CHIRPS                64U
#define FRAME_RATE_FPS              10U
#define FRAME_PERIOD_MS             (1000.0f / (float32)FRAME_RATE_FPS)

/* CFAR detection thresholds */
#define CFAR_GUARD_CELLS            4U
#define CFAR_TRAINING_CELLS         8U
#define CFAR_THRESHOLD_DB           12.0f

/* Zone definitions (meters) */
#define NUM_ZONES                   3U

/* UART output */
#define UART_OUTPUT_PERIOD_FRAMES   10U

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
/*  UART Output (stub - replace with platform UART driver)                    */
/* -------------------------------------------------------------------------- */
static void uart_send_counts(const people_counter_t* counter)
{
    printf("UART>> ");
    for (uint32 z = 0; z < counter->num_zones; z++) {
        printf("Zone%u:%u ", z, counter->zones[z].occupant_count);
    }
    printf("| Total: %u\n", people_counter_get_total(counter));
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

    /* --- Open sensor --- */
    status = mmwave_open(handle);
    if (status != MMWAVE_OK) { printf("ERROR: open (%d)\n", status); goto cleanup; }

    /* --- Configure for short-range detection --- */
    mmwave_chirp_profile_t chirp = config.chirp_profile;
    chirp.start_freq_ghz    = CHIRP_START_FREQ_GHZ;
    chirp.freq_slope_mhz_us = CHIRP_SLOPE_MHZ_US;
    chirp.adc_samples       = CHIRP_ADC_SAMPLES;
    chirp.sample_rate_ksps  = 5000;
    status = mmwave_config_chirp(handle, &chirp);
    if (status != MMWAVE_OK) { printf("ERROR: chirp (%d)\n", status); goto cleanup; }

    mmwave_frame_config_t frame_cfg = config.frame_config;
    frame_cfg.chirps_per_frame     = FRAME_CHIRPS;
    frame_cfg.frame_periodicity_ms = FRAME_PERIOD_MS;
    frame_cfg.num_frames           = 0;
    status = mmwave_config_frame(handle, &frame_cfg);
    if (status != MMWAVE_OK) { printf("ERROR: frame (%d)\n", status); goto cleanup; }

    /* --- Initialize people counter with 3 zones --- */
    people_counter_t counter;
    people_counter_config_t pc_cfg = {
        .num_zones       = NUM_ZONES,
        .max_range_m     = SHORT_RANGE_MAX_M,
        .cfar_guard      = CFAR_GUARD_CELLS,
        .cfar_training   = CFAR_TRAINING_CELLS,
        .cfar_threshold_db = CFAR_THRESHOLD_DB,
        .cluster_dist_m  = 0.5f,
    };
    /* Define three monitoring zones */
    pc_cfg.zones[0] = (zone_def_t){ .x_min = -2.0f, .x_max = -0.5f, .y_min = 0.5f, .y_max = 3.0f };
    pc_cfg.zones[1] = (zone_def_t){ .x_min = -0.5f, .x_max =  0.5f, .y_min = 0.5f, .y_max = 5.0f };
    pc_cfg.zones[2] = (zone_def_t){ .x_min =  0.5f, .x_max =  2.0f, .y_min = 0.5f, .y_max = 3.0f };

    people_counter_init(&counter, &pc_cfg);

    /* --- Calibrate and start --- */
    status = mmwave_calibrate(handle, NULL);
    if (status != MMWAVE_OK) { printf("ERROR: calibrate (%d)\n", status); goto cleanup; }

    status = mmwave_start(handle);
    if (status != MMWAVE_OK) { printf("ERROR: start (%d)\n", status); goto cleanup; }
    printf("People counting started. Monitoring %u zones.\n", NUM_ZONES);

    /* --- Main processing loop --- */
    mmwave_frame_t frame;
    uint32 frame_count = 0;

    while (g_running) {
        status = mmwave_get_frame(handle, &frame, 1000);
        if (status == MMWAVE_ERR_TIMEOUT) { continue; }
        if (status != MMWAVE_OK)          { break; }

        /* Process frame: range-FFT -> CFAR -> cluster -> count */
        people_counter_process_frame(&counter, &frame);
        frame_count++;

        /* Periodic UART output */
        if ((frame_count % UART_OUTPUT_PERIOD_FRAMES) == 0) {
            uart_send_counts(&counter);
        }
    }

    /* --- Cleanup --- */
cleanup:
    printf("Shutting down people counter...\n");
    people_counter_deinit(&counter);
    mmwave_stop(handle);
    mmwave_close(handle);
    mmwave_deinit(handle);
    printf("Done. Processed %u frames.\n", frame_count);
    return EXIT_SUCCESS;
}
