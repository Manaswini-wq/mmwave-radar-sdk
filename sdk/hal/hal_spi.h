/**
 * @file hal_spi.h
 * @brief SPI Hardware Abstraction Layer for mmWave Radar SDK
 * @details Register-level SPI master driver supporting blocking and DMA transfers.
 *          Designed for communication with the radar front-end ASIC over
 *          high-speed SPI (up to 25 MHz).
 */
#ifndef HAL_SPI_H
#define HAL_SPI_H

#include "mmwave_types.h"

/** SPI instance identifiers */
#define HAL_SPI_INSTANCE_0          0U
#define HAL_SPI_INSTANCE_1          1U
#define HAL_SPI_MAX_INSTANCES       2U

/** SPI clock speed presets (Hz) */
#define HAL_SPI_SPEED_1MHZ          1000000U
#define HAL_SPI_SPEED_5MHZ          5000000U
#define HAL_SPI_SPEED_10MHZ         10000000U
#define HAL_SPI_SPEED_20MHZ         20000000U
#define HAL_SPI_SPEED_25MHZ         25000000U

/** SPI chip select lines */
#define HAL_SPI_CS_0                0U
#define HAL_SPI_CS_1                1U
#define HAL_SPI_CS_2                2U
#define HAL_SPI_CS_MAX              3U

/** SPI transfer timeout (loop iterations) */
#define HAL_SPI_TIMEOUT_LOOPS       100000U

/**
 * @brief DMA transfer completion callback
 * @param[in] instance  SPI instance that completed
 * @param[in] user_data User context pointer
 */
typedef void (*hal_spi_dma_callback_t)(uint32 instance, void* user_data);

/**
 * @brief Initialize an SPI peripheral instance
 * @param[in] instance  SPI instance index (HAL_SPI_INSTANCE_0/1)
 * @param[in] speed_hz  Clock speed in Hz (use HAL_SPI_SPEED_* defines)
 * @return MMWAVE_OK on success, negative error code on failure
 */
int32 hal_spi_init(uint32 instance, uint32 speed_hz);

/**
 * @brief Perform a blocking SPI transfer
 * @param[in]  instance  SPI instance index
 * @param[in]  tx_buf    Transmit buffer (NULL for receive-only)
 * @param[out] rx_buf    Receive buffer (NULL for transmit-only)
 * @param[in]  len       Number of bytes to transfer
 * @return MMWAVE_OK on success, MMWAVE_ERR_SPI_TIMEOUT on timeout
 */
int32 hal_spi_transfer(uint32 instance, const uint8* tx_buf, uint8* rx_buf, uint32 len);

/**
 * @brief Perform a DMA-based SPI transfer (non-blocking)
 * @param[in]  instance  SPI instance index
 * @param[in]  tx_buf    Transmit buffer (NULL for receive-only)
 * @param[out] rx_buf    Receive buffer (NULL for transmit-only)
 * @param[in]  len       Number of bytes to transfer
 * @param[in]  callback  Completion callback (NULL if not needed)
 * @return MMWAVE_OK on success, negative error code on failure
 */
int32 hal_spi_dma_transfer(uint32 instance, const uint8* tx_buf, uint8* rx_buf,
                           uint32 len, hal_spi_dma_callback_t callback);

/**
 * @brief Control a chip-select line
 * @param[in] instance  SPI instance index
 * @param[in] cs_num    Chip select number (HAL_SPI_CS_0/1/2)
 * @param[in] active    true = assert (active low), false = de-assert
 * @return MMWAVE_OK on success, negative error code on failure
 */
int32 hal_spi_chip_select(uint32 instance, uint32 cs_num, bool active);

/**
 * @brief De-initialize an SPI peripheral instance
 * @param[in] instance  SPI instance index
 * @return MMWAVE_OK on success
 */
int32 hal_spi_deinit(uint32 instance);

#endif /* HAL_SPI_H */
