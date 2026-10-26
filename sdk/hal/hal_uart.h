/**
 * @file hal_uart.h
 * @brief UART Hardware Abstraction Layer for mmWave Radar SDK
 * @details Interrupt-driven UART driver with ring-buffer receive for
 *          debug console output and host communication.
 */
#ifndef HAL_UART_H
#define HAL_UART_H

#include "mmwave_types.h"

/** UART instance identifiers */
#define HAL_UART_INSTANCE_0         0U
#define HAL_UART_INSTANCE_1         1U
#define HAL_UART_MAX_INSTANCES      2U

/** Standard baud rates */
#define HAL_UART_BAUD_115200        115200U
#define HAL_UART_BAUD_921600        921600U
#define HAL_UART_BAUD_3000000       3000000U

/** Ring buffer size for RX (must be power of 2) */
#define HAL_UART_RX_BUF_SIZE        256U

/** UART receive callback */
typedef void (*hal_uart_rx_callback_t)(uint32 instance, uint8 byte);

/**
 * @brief Initialize a UART peripheral instance
 * @param[in] instance  UART instance index
 * @param[in] baud_rate Baud rate in bps
 * @return MMWAVE_OK on success, negative error code on failure
 */
int32 hal_uart_init(uint32 instance, uint32 baud_rate);

/**
 * @brief Send a block of data over UART (blocking)
 * @param[in] instance  UART instance index
 * @param[in] data      Pointer to data buffer
 * @param[in] len       Number of bytes to send
 * @return MMWAVE_OK on success
 */
int32 hal_uart_send(uint32 instance, const uint8* data, uint32 len);

/**
 * @brief Receive data from UART ring buffer
 * @param[in]  instance  UART instance index
 * @param[out] data      Pointer to receive buffer
 * @param[in]  max_len   Maximum bytes to read
 * @param[out] actual    Actual bytes read
 * @return MMWAVE_OK on success
 */
int32 hal_uart_receive(uint32 instance, uint8* data, uint32 max_len, uint32* actual);

/**
 * @brief Register a per-byte receive callback (called from ISR context)
 * @param[in] instance  UART instance index
 * @param[in] callback  Callback function (NULL to unregister)
 * @return MMWAVE_OK on success
 */
int32 hal_uart_register_rx_callback(uint32 instance, hal_uart_rx_callback_t callback);

/**
 * @brief Send a null-terminated string over UART
 * @param[in] instance  UART instance index
 * @param[in] str       Null-terminated string
 * @return MMWAVE_OK on success
 */
int32 hal_uart_puts(uint32 instance, const char* str);

/**
 * @brief De-initialize a UART peripheral instance
 * @param[in] instance  UART instance index
 * @return MMWAVE_OK on success
 */
int32 hal_uart_deinit(uint32 instance);

#endif /* HAL_UART_H */
