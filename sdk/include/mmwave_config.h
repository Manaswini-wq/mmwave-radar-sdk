/**
 * @file mmwave_config.h
 * @brief Compile-time SDK configuration
 * @details Configurable parameters for scalability across sensor platforms
 */
#ifndef MMWAVE_CONFIG_H
#define MMWAVE_CONFIG_H

/* SDK feature enables */
#define MMWAVE_FEATURE_CALIBRATION      1
#define MMWAVE_FEATURE_DSP_PROCESSING   1
#define MMWAVE_FEATURE_DATA_VALIDATION  1
#define MMWAVE_FEATURE_ERROR_CALLBACKS  1

/* Buffer configuration */
#define MMWAVE_ADC_BUFFER_SIZE          (64U * 1024U)  /* 64 KB per ping/pong */
#define MMWAVE_NUM_PING_PONG_BUFFERS    2U
#define MMWAVE_LOG_BUFFER_SIZE          1024U

/* DMA configuration */
#define MMWAVE_DMA_CHANNEL_ADC          0U
#define MMWAVE_DMA_CHANNEL_RESULT       1U
#define MMWAVE_DMA_PRIORITY_HIGH        0U

/* UART debug console */
#define MMWAVE_DEBUG_UART_BAUD          921600U
#define MMWAVE_DEBUG_UART_INSTANCE      0U

/* Timing constraints */
#define MMWAVE_INIT_TIMEOUT_MS          5000U
#define MMWAVE_CAL_TIMEOUT_MS           10000U
#define MMWAVE_PLL_LOCK_TIMEOUT_US      200U

/* Platform-specific overrides */
#if defined(PLATFORM_AWR2944)
    #define MMWAVE_MAX_BANDWIDTH_GHZ    5.0f
    #define MMWAVE_MAX_TX_POWER_DBM     12.0f
    #define MMWAVE_ADC_MAX_RATE_KSPS    22500U
#elif defined(PLATFORM_IWR6843)
    #define MMWAVE_MAX_BANDWIDTH_GHZ    4.0f
    #define MMWAVE_MAX_TX_POWER_DBM     12.0f
    #define MMWAVE_ADC_MAX_RATE_KSPS    12500U
#elif defined(PLATFORM_AWR1843)
    #define MMWAVE_MAX_BANDWIDTH_GHZ    4.0f
    #define MMWAVE_MAX_TX_POWER_DBM     12.0f
    #define MMWAVE_ADC_MAX_RATE_KSPS    12500U
#else
    #error "No platform defined. Use -DPLATFORM_AWR2944, -DPLATFORM_IWR6843, or -DPLATFORM_AWR1843"
#endif

#endif /* MMWAVE_CONFIG_H */
