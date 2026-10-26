/**
 * @file error_handler.h
 * @brief Error handler for mmWave Radar SDK
 * @details Provides error logging, severity classification, and fail-safe
 *          defaults for critical errors detected during radar operation.
 */
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "mmwave_types.h"

int32 error_handler_init(void);
void  error_handler_deinit(void);
void  error_handler_report(int32 error_code, const char* message);
int32 error_handler_get_last(void);
uint32 error_handler_get_count(void);

#endif
