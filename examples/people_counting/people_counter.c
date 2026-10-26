/**
 * @file people_counter.c
 * @brief People Counting Algorithm Implementation
 * @details Zone-based occupancy counting:
 *          1. Range-FFT to extract range profile
 *          2. CA-CFAR detection to find targets
 *          3. Distance-based clustering of detections
 *          4. Frame-to-frame track association
 *          5. Per-zone occupant count
 */

#include <string.h>
#include <math.h>

#include "people_counter.h"

/* -------------------------------------------------------------------------- */
/*  Internal Helpers                                                          */
/* -------------------------------------------------------------------------- */

/** Simple magnitude-squared for complex I/Q pair */
static inline float32 mag_sq(int16 i_val, int16 q_val)
{
    return (float32)i_val * (float32)i_val + (float32)q_val * (float32)q_val;
}

/** CA-CFAR detector on a power spectrum (1D) */
static uint32 cfar_detect(const float32* power, uint16 num_bins,
                           uint16 guard, uint16 training,
                           float32 threshold_db,
                           detection_t* dets, uint32 max_dets)
{
    uint32 count = 0;
    uint16 window = guard + training;

    for (uint16 bin = window; bin < (num_bins - window) && count < max_dets; bin++) {
        float32 noise_sum = 0.0f;
        for (uint16 k = 1; k <= training; k++) {
            noise_sum += power[bin - guard - k];
            noise_sum += power[bin + guard + k];
        }
        float32 noise_avg = noise_sum / (2.0f * (float32)training);
        float32 snr_db = 10.0f * log10f(power[bin] / (noise_avg + 1e-12f));

        if (snr_db > threshold_db) {
            dets[count].y_m      = (float32)bin;   /* bin index, converted later */
            dets[count].x_m      = 0.0f;           /* azimuth TBD */
            dets[count].velocity  = 0.0f;
            dets[count].snr_db   = snr_db;
            count++;
        }
    }
    return count;
}

/** Euclidean distance between two detections */
static inline float32 det_distance(const detection_t* a, const detection_t* b)
{
    float32 dx = a->x_m - b->x_m;
    float32 dy = a->y_m - b->y_m;
    return sqrtf(dx * dx + dy * dy);
}

/** Simple distance-based clustering: merge nearby detections */
static uint32 cluster_detections(detection_t* dets, uint32 num_dets,
                                  float32 merge_dist)
{
    bool merged[PC_MAX_DETECTIONS] = {false};
    detection_t clustered[PC_MAX_DETECTIONS];
    uint32 num_clusters = 0;

    for (uint32 i = 0; i < num_dets; i++) {
        if (merged[i]) { continue; }

        float32 cx = dets[i].x_m;
        float32 cy = dets[i].y_m;
        uint32 members = 1;

        for (uint32 j = i + 1; j < num_dets; j++) {
            if (!merged[j] && det_distance(&dets[i], &dets[j]) < merge_dist) {
                cx += dets[j].x_m;
                cy += dets[j].y_m;
                members++;
                merged[j] = true;
            }
        }
        clustered[num_clusters].x_m = cx / (float32)members;
        clustered[num_clusters].y_m = cy / (float32)members;
        num_clusters++;
    }
    memcpy(dets, clustered, num_clusters * sizeof(detection_t));
    return num_clusters;
}

/** Associate detections with existing tracks (nearest-neighbour) */
static void associate_tracks(people_counter_t* ctx)
{
    /* Age all existing tracks */
    for (uint32 t = 0; t < PC_MAX_TRACKS; t++) {
        if (ctx->tracks[t].active) {
            ctx->tracks[t].missed_frames++;
        }
    }

    /* Greedy nearest-neighbour association */
    for (uint32 d = 0; d < ctx->num_detections; d++) {
        float32 best_dist = 1e6f;
        int32 best_trk = -1;

        for (uint32 t = 0; t < PC_MAX_TRACKS; t++) {
            if (!ctx->tracks[t].active) { continue; }
            float32 dx = ctx->detections[d].x_m - ctx->tracks[t].x_m;
            float32 dy = ctx->detections[d].y_m - ctx->tracks[t].y_m;
            float32 dist = sqrtf(dx * dx + dy * dy);
            if (dist < best_dist) { best_dist = dist; best_trk = (int32)t; }
        }

        if (best_trk >= 0 && best_dist < 1.0f) {
            /* Update existing track */
            ctx->tracks[best_trk].x_m = ctx->detections[d].x_m;
            ctx->tracks[best_trk].y_m = ctx->detections[d].y_m;
            ctx->tracks[best_trk].missed_frames = 0;
            ctx->tracks[best_trk].age_frames++;
        } else {
            /* Create new track */
            for (uint32 t = 0; t < PC_MAX_TRACKS; t++) {
                if (!ctx->tracks[t].active) {
                    ctx->tracks[t].active = true;
                    ctx->tracks[t].x_m = ctx->detections[d].x_m;
                    ctx->tracks[t].y_m = ctx->detections[d].y_m;
                    ctx->tracks[t].vx = 0.0f;
                    ctx->tracks[t].vy = 0.0f;
                    ctx->tracks[t].age_frames = 1;
                    ctx->tracks[t].missed_frames = 0;
                    break;
                }
            }
        }
    }

    /* Remove stale tracks */
    for (uint32 t = 0; t < PC_MAX_TRACKS; t++) {
        if (ctx->tracks[t].active &&
            ctx->tracks[t].missed_frames > PC_TRACK_TIMEOUT_FRAMES) {
            ctx->tracks[t].active = false;
        }
    }
}

/** Count active tracks per zone */
static void count_zones(people_counter_t* ctx)
{
    for (uint32 z = 0; z < ctx->num_zones; z++) {
        ctx->zones[z].occupant_count = 0;
    }

    for (uint32 t = 0; t < PC_MAX_TRACKS; t++) {
        if (!ctx->tracks[t].active) { continue; }
        for (uint32 z = 0; z < ctx->num_zones; z++) {
            const zone_def_t* b = &ctx->zones[z].boundary;
            if (ctx->tracks[t].x_m >= b->x_min && ctx->tracks[t].x_m <= b->x_max &&
                ctx->tracks[t].y_m >= b->y_min && ctx->tracks[t].y_m <= b->y_max) {
                ctx->zones[z].occupant_count++;
                break; /* track belongs to first matching zone */
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

int32 people_counter_init(people_counter_t* ctx, const people_counter_config_t* cfg)
{
    if (ctx == NULL || cfg == NULL) { return MMWAVE_ERR_NULL_POINTER; }
    if (cfg->num_zones > PC_MAX_ZONES) { return MMWAVE_ERR_INVALID_PARAM; }

    memset(ctx, 0, sizeof(*ctx));
    ctx->num_zones        = cfg->num_zones;
    ctx->cfar_threshold_db = cfg->cfar_threshold_db;
    ctx->cfar_guard       = cfg->cfar_guard;
    ctx->cfar_training    = cfg->cfar_training;
    ctx->cluster_dist_m   = cfg->cluster_dist_m;

    for (uint32 z = 0; z < cfg->num_zones; z++) {
        ctx->zones[z].boundary = cfg->zones[z];
    }
    return MMWAVE_OK;
}

int32 people_counter_process_frame(people_counter_t* ctx, const mmwave_frame_t* frame)
{
    if (ctx == NULL || frame == NULL) { return MMWAVE_ERR_NULL_POINTER; }

    /* Compute power spectrum from first chirp, first RX channel */
    float32 power[256];
    uint16 n = (frame->num_adc_samples < 256) ? frame->num_adc_samples : 256;
    for (uint16 i = 0; i < n; i++) {
        power[i] = mag_sq(frame->adc_data[2 * i], frame->adc_data[2 * i + 1]);
    }

    /* CFAR detection */
    ctx->num_detections = cfar_detect(power, n,
                                       ctx->cfar_guard, ctx->cfar_training,
                                       ctx->cfar_threshold_db,
                                       ctx->detections, PC_MAX_DETECTIONS);

    /* Cluster nearby detections into single targets */
    ctx->num_detections = cluster_detections(ctx->detections,
                                              ctx->num_detections,
                                              ctx->cluster_dist_m);

    /* Track association and zone counting */
    associate_tracks(ctx);
    count_zones(ctx);
    ctx->frame_count++;

    return MMWAVE_OK;
}

uint16 people_counter_get_total(const people_counter_t* ctx)
{
    uint16 total = 0;
    if (ctx == NULL) { return 0; }
    for (uint32 z = 0; z < ctx->num_zones; z++) {
        total += ctx->zones[z].occupant_count;
    }
    return total;
}

void people_counter_deinit(people_counter_t* ctx)
{
    if (ctx != NULL) {
        memset(ctx, 0, sizeof(*ctx));
    }
}
