/**
 * @file mmwave_types.h
 * @brief Core type definitions for mmWave Radar SDK
 * @details Platform-independent type abstractions for bare-metal and RTOS builds
 */
#ifndef MMWAVE_TYPES_H
#define MMWAVE_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t   uint8;
typedef uint16_t  uint16;
typedef uint32_t  uint32;
typedef uint64_t  uint64;
typedef int8_t    int8;
typedef int16_t   int16;
typedef int32_t   int32;
typedef int64_t   int64;
typedef float     float32;
typedef double    float64;

/** Opaque SDK handle */
typedef void* mmwave_handle_t;

/** Wait timeout values */
#define MMWAVE_NO_WAIT      (0U)
#define MMWAVE_WAIT_FOREVER (0xFFFFFFFFU)

/** Maximum supported channels */
#define MMWAVE_MAX_TX_CHANNELS  3U
#define MMWAVE_MAX_RX_CHANNELS  4U
#define MMWAVE_MAX_PROFILES     4U
#define MMWAVE_MAX_CHIRPS_PER_FRAME  256U

/** Chirp profile configuration */
typedef struct {
    uint32  profile_id;           /**< Profile identifier (0-3) */
    float32 start_freq_ghz;       /**< Start frequency in GHz (e.g. 77.0) */
    float32 freq_slope_mhz_us;    /**< Frequency slope in MHz/us */
    float32 idle_time_us;         /**< Idle time between chirps in us */
    float32 adc_start_time_us;    /**< ADC sampling start time in us */
    float32 ramp_end_time_us;     /**< Chirp ramp end time in us */
    uint16  adc_samples;          /**< Number of ADC samples per chirp */
    uint16  sample_rate_ksps;     /**< ADC sample rate in ksps */
    uint8   rx_gain_db;           /**< RX gain in dB */
    uint8   tx_power_backoff_db;  /**< TX power backoff in dB */
} mmwave_chirp_profile_t;

/** Frame configuration */
typedef struct {
    uint16  chirps_per_frame;     /**< Total chirps in one frame */
    uint16  num_frames;           /**< Number of frames (0 = infinite) */
    float32 frame_periodicity_ms; /**< Frame period in ms */
    uint8   tx_channel_mask;      /**< TX channel enable bitmask */
    uint8   rx_channel_mask;      /**< RX channel enable bitmask */
    uint8   profile_id;           /**< Chirp profile to use */
} mmwave_frame_config_t;

/** Calibration configuration */
typedef struct {
    bool    enable_tx_power_cal;      /**< Enable TX power calibration */
    bool    enable_rx_gain_phase_cal;  /**< Enable RX gain/phase calibration */
    bool    enable_lo_leakage_cal;    /**< Enable LO leakage calibration */
    bool    enable_temp_compensation; /**< Enable temperature-based recalibration */
    float32 temp_recal_threshold_c;   /**< Temperature delta to trigger recal */
} mmwave_cal_config_t;

/** SDK configuration */
typedef struct {
    mmwave_chirp_profile_t  chirp_profile;
    mmwave_frame_config_t   frame_config;
    mmwave_cal_config_t     cal_config;
    uint32                  data_output_mode;  /**< 0=raw ADC, 1=range FFT */
} mmwave_config_t;

/** Radar frame data */
typedef struct {
    uint32   frame_number;        /**< Monotonic frame counter */
    uint32   timestamp_us;        /**< Frame start timestamp */
    uint16   num_chirps;          /**< Chirps in this frame */
    uint16   num_rx_channels;     /**< Active RX channels */
    uint16   num_adc_samples;     /**< Samples per chirp per channel */
    int16*   adc_data;            /**< Raw ADC data buffer pointer */
    uint32   adc_data_size;       /**< Data size in bytes */
    uint32   crc;                 /**< E2E CRC for data integrity */
} mmwave_frame_t;

/** Sensor status */
typedef struct {
    bool    is_initialized;
    bool    is_open;
    bool    is_running;
    float32 temperature_c;
    uint32  frames_processed;
    uint32  error_count;
} mmwave_status_t;

/** Frame callback function type */
typedef void (*mmwave_frame_callback_t)(const mmwave_frame_t* frame, void* user_data);

/** Error callback function type */
typedef void (*mmwave_error_callback_t)(int32 error_code, const char* msg, void* user_data);

/** Default configuration macro */
#define MMWAVE_DEFAULT_CONFIG { \
    .chirp_profile = { \
        .profile_id = 0, \
        .start_freq_ghz = 77.0f, \
        .freq_slope_mhz_us = 70.0f, \
        .idle_time_us = 7.0f, \
        .adc_start_time_us = 6.0f, \
        .ramp_end_time_us = 40.0f, \
        .adc_samples = 256, \
        .sample_rate_ksps = 10000, \
        .rx_gain_db = 30, \
        .tx_power_backoff_db = 0 \
    }, \
    .frame_config = { \
        .chirps_per_frame = 128, \
        .num_frames = 0, \
        .frame_periodicity_ms = 33.33f, \
        .tx_channel_mask = 0x07, \
        .rx_channel_mask = 0x0F, \
        .profile_id = 0 \
    }, \
    .cal_config = { \
        .enable_tx_power_cal = true, \
        .enable_rx_gain_phase_cal = true, \
        .enable_lo_leakage_cal = true, \
        .enable_temp_compensation = true, \
        .temp_recal_threshold_c = 10.0f \
    }, \
    .data_output_mode = 0 \
}

#endif /* MMWAVE_TYPES_H */
