/**
 * @file mmwave_error_codes.h
 * @brief Error code definitions for mmWave Radar SDK
 * @details Comprehensive error codes for fault-tolerant, fail-safe operation
 */
#ifndef MMWAVE_ERROR_CODES_H
#define MMWAVE_ERROR_CODES_H

/** Success */
#define MMWAVE_OK                       (0)

/** General errors */
#define MMWAVE_ERR_INVALID_PARAM        (-1)
#define MMWAVE_ERR_NULL_POINTER         (-2)
#define MMWAVE_ERR_NOT_INITIALIZED      (-3)
#define MMWAVE_ERR_ALREADY_INITIALIZED  (-4)
#define MMWAVE_ERR_NOT_OPEN             (-5)
#define MMWAVE_ERR_ALREADY_OPEN         (-6)
#define MMWAVE_ERR_NOT_RUNNING          (-7)
#define MMWAVE_ERR_ALREADY_RUNNING      (-8)
#define MMWAVE_ERR_TIMEOUT              (-9)
#define MMWAVE_ERR_OUT_OF_MEMORY        (-10)
#define MMWAVE_ERR_BUFFER_OVERFLOW      (-11)

/** Hardware errors */
#define MMWAVE_ERR_HW_INIT_FAILED       (-100)
#define MMWAVE_ERR_PLL_LOCK_FAILED      (-101)
#define MMWAVE_ERR_ADC_FAULT            (-102)
#define MMWAVE_ERR_DMA_ERROR            (-103)
#define MMWAVE_ERR_SPI_TIMEOUT          (-104)
#define MMWAVE_ERR_RF_FAULT             (-105)
#define MMWAVE_ERR_TEMP_SENSOR_FAULT    (-106)

/** Calibration errors */
#define MMWAVE_ERR_CAL_TX_POWER         (-200)
#define MMWAVE_ERR_CAL_RX_GAIN          (-201)
#define MMWAVE_ERR_CAL_LO_LEAKAGE      (-202)
#define MMWAVE_ERR_CAL_TIMEOUT          (-203)
#define MMWAVE_ERR_CAL_DATA_INVALID     (-204)

/** Configuration errors */
#define MMWAVE_ERR_CFG_INVALID_FREQ     (-300)
#define MMWAVE_ERR_CFG_INVALID_SLOPE    (-301)
#define MMWAVE_ERR_CFG_INVALID_SAMPLES  (-302)
#define MMWAVE_ERR_CFG_INVALID_CHANNELS (-303)
#define MMWAVE_ERR_CFG_BANDWIDTH_EXCEED (-304)

/** Data errors */
#define MMWAVE_ERR_DATA_CRC_MISMATCH    (-400)
#define MMWAVE_ERR_DATA_OVERFLOW        (-401)
#define MMWAVE_ERR_DATA_UNDERFLOW       (-402)
#define MMWAVE_ERR_FRAME_DROPPED        (-403)

/** Convert error code to string */
const char* mmwave_strerror(int32_t error_code);

#endif /* MMWAVE_ERROR_CODES_H */
