/**
 * @file hal_dma.c
 * @brief DMA HAL implementation for mmWave Radar SDK
 * @details DMA controller driver for high-throughput data transfers.
 *          Manages channel configuration, priority, circular mode, and
 *          completion callbacks for the EDMA-lite controller on TI mmWave SoCs.
 */
#include "hal_dma.h"
#include "hal_interrupt.h"
#include "mmwave_error_codes.h"

#include <string.h>

/* ---- DMA Register Map ---- */
#define DMA_BASE_ADDR               0xFFFFF000U

#define DMA_REG_GCTRL               0x00U   /**< Global control register      */
#define DMA_REG_PEND                0x04U   /**< Channel pending register     */
#define DMA_REG_STATUS              0x0CU   /**< HW status register           */
#define DMA_REG_HWCHENA_SET         0x14U   /**< HW channel enable set        */
#define DMA_REG_HWCHENA_CLR         0x18U   /**< HW channel enable clear      */
#define DMA_REG_SWCHENA_SET         0x1CU   /**< SW channel enable set        */
#define DMA_REG_SWCHENA_CLR         0x20U   /**< SW channel enable clear      */

/* Per-channel register block (offset from base + 0x100 + ch*0x20) */
#define DMA_CH_OFFSET(ch)           (0x100U + ((ch) * 0x20U))
#define DMA_CH_REG_CTRL             0x00U   /**< Channel control              */
#define DMA_CH_REG_SRC              0x04U   /**< Source address               */
#define DMA_CH_REG_DST              0x08U   /**< Destination address          */
#define DMA_CH_REG_COUNT            0x0CU   /**< Element count                */
#define DMA_CH_REG_CHCTRL           0x10U   /**< Channel-specific control     */

/* Global control bits */
#define DMA_GCTRL_ENABLE            (1U << 0)
#define DMA_GCTRL_RESET             (1U << 16)

/* Channel control bits */
#define DMA_CHCTRL_DIR_SHIFT        0U
#define DMA_CHCTRL_WIDTH_SHIFT      4U
#define DMA_CHCTRL_PRIO_SHIFT       8U
#define DMA_CHCTRL_SRCINC           (1U << 12)
#define DMA_CHCTRL_DSTINC           (1U << 13)
#define DMA_CHCTRL_CIRCULAR         (1U << 14)

#define DMA_REG(off)        (*(volatile uint32*)(DMA_BASE_ADDR + (off)))
#define DMA_CH_REG(ch, off) (*(volatile uint32*)(DMA_BASE_ADDR + DMA_CH_OFFSET(ch) + (off)))

/* ---- Per-channel state ---- */
typedef struct {
    bool                configured;
    uint32              status;
    hal_dma_callback_t  callback;
    void*               user_data;
    hal_dma_config_t    config;
} dma_channel_t;

static dma_channel_t g_dma_channels[HAL_DMA_MAX_CHANNELS];
static bool          g_dma_initialized = false;

/* Forward declarations */
static void dma_complete_isr(void* arg);
static void dma_error_isr(void* arg);

/* ---- Public API ---- */

int32 hal_dma_init(void)
{
    if (g_dma_initialized) {
        return MMWAVE_ERR_ALREADY_INITIALIZED;
    }

    memset(g_dma_channels, 0, sizeof(g_dma_channels));

    /* Reset the DMA controller */
    DMA_REG(DMA_REG_GCTRL) = DMA_GCTRL_RESET;

    /* Brief delay for reset to complete (hardware needs a few cycles) */
    volatile uint32 delay = 100U;
    while (delay-- > 0U) { /* spin */ }

    /* Release reset and enable */
    DMA_REG(DMA_REG_GCTRL) = DMA_GCTRL_ENABLE;

    /* Disable all channels */
    DMA_REG(DMA_REG_HWCHENA_CLR) = 0xFFFFFFFFU;
    DMA_REG(DMA_REG_SWCHENA_CLR) = 0xFFFFFFFFU;

    /* Register DMA interrupts */
    hal_interrupt_register(IRQ_DMA_COMPLETE_CH0, dma_complete_isr, NULL);
    hal_interrupt_register(IRQ_DMA_COMPLETE_CH1, dma_complete_isr, NULL);
    hal_interrupt_register(IRQ_DMA_ERROR, dma_error_isr, NULL);

    hal_interrupt_enable(IRQ_DMA_COMPLETE_CH0);
    hal_interrupt_enable(IRQ_DMA_COMPLETE_CH1);
    hal_interrupt_enable(IRQ_DMA_ERROR);

    g_dma_initialized = true;
    return MMWAVE_OK;
}

int32 hal_dma_configure(const hal_dma_config_t* config)
{
    if (!g_dma_initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (config == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (config->channel >= HAL_DMA_MAX_CHANNELS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (config->transfer_size == 0U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uint32 ch = config->channel;
    dma_channel_t* dma_ch = &g_dma_channels[ch];

    /* Store the configuration */
    memcpy(&dma_ch->config, config, sizeof(hal_dma_config_t));

    /* Program source and destination addresses */
    DMA_CH_REG(ch, DMA_CH_REG_SRC) = config->src_addr;
    DMA_CH_REG(ch, DMA_CH_REG_DST) = config->dst_addr;

    /* Program element count */
    DMA_CH_REG(ch, DMA_CH_REG_COUNT) = config->transfer_size;

    /* Build channel control word */
    uint32 ctrl = 0U;
    ctrl |= (config->direction  << DMA_CHCTRL_DIR_SHIFT);
    ctrl |= (config->data_width << DMA_CHCTRL_WIDTH_SHIFT);
    ctrl |= (config->priority   << DMA_CHCTRL_PRIO_SHIFT);

    if (config->src_increment) {
        ctrl |= DMA_CHCTRL_SRCINC;
    }
    if (config->dst_increment) {
        ctrl |= DMA_CHCTRL_DSTINC;
    }
    if (config->circular_mode) {
        ctrl |= DMA_CHCTRL_CIRCULAR;
    }

    DMA_CH_REG(ch, DMA_CH_REG_CHCTRL) = ctrl;

    dma_ch->configured = true;
    dma_ch->status = HAL_DMA_STATUS_IDLE;

    return MMWAVE_OK;
}

int32 hal_dma_start(uint32 channel)
{
    if (!g_dma_initialized || channel >= HAL_DMA_MAX_CHANNELS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (!g_dma_channels[channel].configured) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }

    /* Enable the channel via hardware or software trigger */
    if (g_dma_channels[channel].config.direction == HAL_DMA_DIR_MEM_TO_MEM) {
        /* Memory-to-memory uses software trigger */
        DMA_REG(DMA_REG_SWCHENA_SET) = (1U << channel);
    } else {
        /* Peripheral transfers use hardware trigger */
        DMA_REG(DMA_REG_HWCHENA_SET) = (1U << channel);
    }

    g_dma_channels[channel].status = HAL_DMA_STATUS_ACTIVE;
    return MMWAVE_OK;
}

int32 hal_dma_stop(uint32 channel)
{
    if (!g_dma_initialized || channel >= HAL_DMA_MAX_CHANNELS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    /* Disable the channel */
    DMA_REG(DMA_REG_HWCHENA_CLR) = (1U << channel);
    DMA_REG(DMA_REG_SWCHENA_CLR) = (1U << channel);

    g_dma_channels[channel].status = HAL_DMA_STATUS_IDLE;
    return MMWAVE_OK;
}

int32 hal_dma_get_status(uint32 channel, uint32* status)
{
    if (channel >= HAL_DMA_MAX_CHANNELS || status == NULL) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    *status = g_dma_channels[channel].status;
    return MMWAVE_OK;
}

int32 hal_dma_register_callback(uint32 channel, hal_dma_callback_t callback, void* user_data)
{
    if (channel >= HAL_DMA_MAX_CHANNELS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    g_dma_channels[channel].callback  = callback;
    g_dma_channels[channel].user_data = user_data;
    return MMWAVE_OK;
}

int32 hal_dma_deinit(void)
{
    if (!g_dma_initialized) {
        return MMWAVE_OK;
    }

    /* Stop all channels */
    for (uint32 ch = 0U; ch < HAL_DMA_MAX_CHANNELS; ch++) {
        hal_dma_stop(ch);
    }

    /* Disable DMA controller */
    DMA_REG(DMA_REG_GCTRL) = 0U;

    hal_interrupt_unregister(IRQ_DMA_COMPLETE_CH0);
    hal_interrupt_unregister(IRQ_DMA_COMPLETE_CH1);
    hal_interrupt_unregister(IRQ_DMA_ERROR);

    memset(g_dma_channels, 0, sizeof(g_dma_channels));
    g_dma_initialized = false;

    return MMWAVE_OK;
}

/* ---- ISR handlers ---- */

static void dma_complete_isr(void* arg)
{
    (void)arg;

    /* Read pending register to identify which channel completed */
    uint32 pending = DMA_REG(DMA_REG_PEND);

    for (uint32 ch = 0U; ch < HAL_DMA_MAX_CHANNELS; ch++) {
        if (pending & (1U << ch)) {
            g_dma_channels[ch].status = HAL_DMA_STATUS_COMPLETE;

            /* Clear the pending flag */
            DMA_REG(DMA_REG_PEND) = (1U << ch);

            if (g_dma_channels[ch].callback != NULL) {
                g_dma_channels[ch].callback(ch, HAL_DMA_STATUS_COMPLETE,
                                            g_dma_channels[ch].user_data);
            }
        }
    }
}

static void dma_error_isr(void* arg)
{
    (void)arg;

    uint32 status_reg = DMA_REG(DMA_REG_STATUS);

    for (uint32 ch = 0U; ch < HAL_DMA_MAX_CHANNELS; ch++) {
        if (status_reg & (1U << (ch + 16U))) {
            g_dma_channels[ch].status = HAL_DMA_STATUS_ERROR;

            /* Clear error flag */
            DMA_REG(DMA_REG_STATUS) = (1U << (ch + 16U));

            if (g_dma_channels[ch].callback != NULL) {
                g_dma_channels[ch].callback(ch, HAL_DMA_STATUS_ERROR,
                                            g_dma_channels[ch].user_data);
            }
        }
    }
}
