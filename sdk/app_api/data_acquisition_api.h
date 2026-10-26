/**
 * @file data_acquisition_api.h
 * @brief High-speed data acquisition API for mmWave Radar SDK
 */
#ifndef DATA_ACQUISITION_API_H
#define DATA_ACQUISITION_API_H

#include "mmwave_types.h"

typedef void* data_acq_handle_t;

int32 data_acq_init(data_acq_handle_t* handle, uint32 buffer_size);
int32 data_acq_start(data_acq_handle_t handle);
int32 data_acq_stop(data_acq_handle_t handle);
int32 data_acq_get_frame(data_acq_handle_t handle, mmwave_frame_t* frame, uint32 timeout_ms);
int32 data_acq_get_latest_frame(data_acq_handle_t handle, mmwave_frame_t* frame);
int32 data_acq_deinit(data_acq_handle_t handle);

#endif
