/**
 * @file hal_dma.h
 * @brief DMA Hardware Abstraction Layer for mmWave Radar SDK
 * @details DMA controller driver for high-throughput ADC data transfers
 *          and memory-to-memory operations. Supports channel priority,
 *          ping-pong buffering, and completion callbacks.
 */
#ifndef HAL_DMA_H
#define HAL_DMA_H

#include "mmwave_types.h"

/** DMA channel identifiers */
#define HAL_DMA_CHANNEL_0           0U
#define HAL_DMA_CHANNEL_1           1U
#define HAL_DMA_CHANNEL_2           2U
#define HAL_DMA_CHANNEL_3           3U
#define HAL_DMA_MAX_CHANNELS        4U

/** DMA transfer direction */
#define HAL_DMA_DIR_PERIPH_TO_MEM   0U
#define HAL_DMA_DIR_MEM_TO_PERIPH   1U
#define HAL_DMA_DIR_MEM_TO_MEM      2U

/** DMA transfer data width */
#define HAL_DMA_WIDTH_8BIT          0U
#define HAL_DMA_WIDTH_16BIT         1U
#define HAL_DMA_WIDTH_32BIT         2U

/** DMA channel priority */
#define HAL_DMA_PRIORITY_LOW        3U
#define HAL_DMA_PRIORITY_MEDIUM     2U
#define HAL_DMA_PRIORITY_HIGH       1U
#define HAL_DMA_PRIORITY_VERY_HIGH  0U

/** DMA channel status flags */
#define HAL_DMA_STATUS_IDLE         0U
#define HAL_DMA_STATUS_ACTIVE       1U
#define HAL_DMA_STATUS_COMPLETE     2U
#define HAL_DMA_STATUS_ERROR        3U

/** DMA completion callback */
typedef void (*hal_dma_callback_t)(uint32 channel, uint32 status, void* user_data);

/** DMA channel configuration */
typedef struct {
    uint32  channel;        /**< DMA channel number */
    uint32  direction;      /**< Transfer direction */
    uint32  data_width;     /**< Data width per element */
    uint32  priority;       /**< Channel priority */
    uint32  src_addr;       /**< Source address */
    uint32  dst_addr;       /**< Destination address */
    uint32  transfer_size;  /**< Number of elements to transfer */
    bool    src_increment;  /**< Increment source address */
    bool    dst_increment;  /**< Increment destination address */
    bool    circular_mode;  /**< Enable circular/ping-pong mode */
} hal_dma_config_t;

/**
 * @brief Initialize the DMA controller
 * @return MMWAVE_OK on success
 */
int32 hal_dma_init(void);

/**
 * @brief Configure a DMA channel
 * @param[in] config  Channel configuration structure
 * @return MMWAVE_OK on success
 */
int32 hal_dma_configure(const hal_dma_config_t* config);

/**
 * @brief Start a DMA channel transfer
 * @param[in] channel  DMA channel number
 * @return MMWAVE_OK on success
 */
int32 hal_dma_start(uint32 channel);

/**
 * @brief Stop a DMA channel transfer
 * @param[in] channel  DMA channel number
 * @return MMWAVE_OK on success
 */
int32 hal_dma_stop(uint32 channel);

/**
 * @brief Get DMA channel status
 * @param[in]  channel  DMA channel number
 * @param[out] status   Pointer to receive status
 * @return MMWAVE_OK on success
 */
int32 hal_dma_get_status(uint32 channel, uint32* status);

/**
 * @brief Register a completion/error callback for a DMA channel
 * @param[in] channel    DMA channel number
 * @param[in] callback   Callback function (NULL to unregister)
 * @param[in] user_data  User context pointer
 * @return MMWAVE_OK on success
 */
int32 hal_dma_register_callback(uint32 channel, hal_dma_callback_t callback, void* user_data);

/**
 * @brief De-initialize the DMA controller
 * @return MMWAVE_OK on success
 */
int32 hal_dma_deinit(void);

#endif /* HAL_DMA_H */
