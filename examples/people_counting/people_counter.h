/**
 * @file people_counter.h
 * @brief People Counting Algorithm Interface
 * @details Zone-based occupancy counting using point cloud clustering
 *          and frame-to-frame tracking.
 */
#ifndef PEOPLE_COUNTER_H
#define PEOPLE_COUNTER_H

#include "mmwave_types.h"

/* -------------------------------------------------------------------------- */
/*  Constants                                                                 */
/* -------------------------------------------------------------------------- */
#define PC_MAX_ZONES            8U
#define PC_MAX_DETECTIONS       64U
#define PC_MAX_TRACKS           16U
#define PC_TRACK_TIMEOUT_FRAMES 10U

/* -------------------------------------------------------------------------- */
/*  Types                                                                     */
/* -------------------------------------------------------------------------- */

/** Rectangular zone definition (x/y in meters, sensor at origin) */
typedef struct {
    float32 x_min;
    float32 x_max;
    float32 y_min;      /**< range min */
    float32 y_max;      /**< range max */
} zone_def_t;

/** Single radar detection point */
typedef struct {
    float32 x_m;        /**< lateral position */
    float32 y_m;        /**< range position */
    float32 velocity;   /**< Doppler velocity m/s */
    float32 snr_db;     /**< detection SNR */
} detection_t;

/** Tracked object */
typedef struct {
    float32 x_m;
    float32 y_m;
    float32 vx;
    float32 vy;
    uint32  age_frames;
    uint32  missed_frames;
    bool    active;
} track_t;

/** Zone state */
typedef struct {
    zone_def_t  boundary;
    uint16      occupant_count;
    uint16      detection_count;
} zone_state_t;

/** People counter configuration */
typedef struct {
    uint32      num_zones;
    float32     max_range_m;
    uint16      cfar_guard;
    uint16      cfar_training;
    float32     cfar_threshold_db;
    float32     cluster_dist_m;
    zone_def_t  zones[PC_MAX_ZONES];
} people_counter_config_t;

/** People counter runtime context */
typedef struct {
    uint32              num_zones;
    zone_state_t        zones[PC_MAX_ZONES];
    track_t             tracks[PC_MAX_TRACKS];
    detection_t         detections[PC_MAX_DETECTIONS];
    uint32              num_detections;
    float32             cfar_threshold_db;
    float32             cluster_dist_m;
    uint16              cfar_guard;
    uint16              cfar_training;
    uint32              frame_count;
} people_counter_t;

/* -------------------------------------------------------------------------- */
/*  API                                                                       */
/* -------------------------------------------------------------------------- */

/** Initialize people counter */
int32 people_counter_init(people_counter_t* ctx, const people_counter_config_t* cfg);

/** Process one radar frame */
int32 people_counter_process_frame(people_counter_t* ctx, const mmwave_frame_t* frame);

/** Get total occupant count across all zones */
uint16 people_counter_get_total(const people_counter_t* ctx);

/** De-initialize and release resources */
void people_counter_deinit(people_counter_t* ctx);

#endif /* PEOPLE_COUNTER_H */
