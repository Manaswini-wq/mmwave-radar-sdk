/**
 * @file hal_spi.c
 * @brief SPI HAL implementation for mmWave Radar SDK
 * @details Register-level SPI master driver with blocking and DMA transfer
 *          support. Targets the SPI peripheral found on TI AWR/IWR mmWave SoCs.
 */
#include "hal_spi.h"
#include "hal_dma.h"
#include "mmwave_error_codes.h"

#include <string.h>

/* ---- SPI Register Map (base address per instance) ---- */
#define SPI0_BASE_ADDR              0xFFF7F400U
#define SPI1_BASE_ADDR              0xFFF7F600U

#define SPI_REG_SPIGCR0             0x00U   /**< Global control 0 (reset)   */
#define SPI_REG_SPIGCR1             0x04U   /**< Global control 1 (enable)  */
#define SPI_REG_SPIINT              0x08U   /**< Interrupt enable            */
#define SPI_REG_SPILVL              0x0CU   /**< Interrupt level             */
#define SPI_REG_SPIFLG              0x10U   /**< Status flags                */
#define SPI_REG_SPIPC0              0x14U   /**< Pin control (func select)   */
#define SPI_REG_SPIDAT0             0x38U   /**< TX data register            */
#define SPI_REG_SPIBUF              0x3CU   /**< RX data buffer register     */
#define SPI_REG_SPIEMU              0x40U   /**< Emulation register          */
#define SPI_REG_SPIDELAY            0x44U   /**< Delay register              */
#define SPI_REG_SPIDEF              0x4CU   /**< Default chip-select pattern */
#define SPI_REG_SPIFMT0             0x50U   /**< Data format register 0      */
#define SPI_REG_SPIBRR              0x54U   /**< Baud rate register          */

/* Flag bits in SPIFLG */
#define SPIFLG_TXINTFLAG            (1U << 9)
#define SPIFLG_RXINTFLAG            (1U << 8)
#define SPIFLG_OVRNINTFLG           (1U << 6)
#define SPIFLG_BITERR               (1U << 4)
#define SPIFLG_BUFINITACTIVE        (1U << 24)

/* GCR1 bits */
#define SPIGCR1_MASTER              (1U << 0)
#define SPIGCR1_CLKMOD              (1U << 1)
#define SPIGCR1_ENABLE              (1U << 24)

/* SPI module input clock (VCLK) - platform dependent */
#define SPI_MODULE_CLK_HZ           200000000U

/* ---- Register access macros ---- */
#define SPI_REG(base, off)  (*(volatile uint32*)((base) + (off)))

/* ---- Per-instance state ---- */
typedef struct {
    uint32                  base_addr;
    bool                    initialized;
    uint32                  speed_hz;
    hal_spi_dma_callback_t  dma_cb;
} spi_instance_t;

static spi_instance_t g_spi[HAL_SPI_MAX_INSTANCES];

static const uint32 spi_base_addrs[HAL_SPI_MAX_INSTANCES] = {
    SPI0_BASE_ADDR,
    SPI1_BASE_ADDR
};

/* ---- Public API ---- */

int32 hal_spi_init(uint32 instance, uint32 speed_hz)
{
    if (instance >= HAL_SPI_MAX_INSTANCES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (speed_hz == 0U || speed_hz > HAL_SPI_SPEED_25MHZ) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    spi_instance_t* spi = &g_spi[instance];
    spi->base_addr = spi_base_addrs[instance];
    spi->speed_hz  = speed_hz;
    spi->dma_cb    = NULL;

    uint32 base = spi->base_addr;

    /* Reset the SPI module */
    SPI_REG(base, SPI_REG_SPIGCR0) = 0U;
    SPI_REG(base, SPI_REG_SPIGCR0) = 1U;  /* Release reset */

    /* Configure as master, internal clock */
    SPI_REG(base, SPI_REG_SPIGCR1) = SPIGCR1_MASTER | SPIGCR1_CLKMOD;

    /* Set baud rate: BRR = (VCLK / desired_speed) - 1 */
    uint32 brr = (SPI_MODULE_CLK_HZ / speed_hz) - 1U;
    if (brr > 0xFFU) {
        brr = 0xFFU;
    }
    SPI_REG(base, SPI_REG_SPIBRR) = brr;

    /* Data format 0: 8-bit, CPOL=0, CPHA=0, MSB-first */
    SPI_REG(base, SPI_REG_SPIFMT0) = (8U << 0);  /* CHARLEN = 8 */

    /* Pin control: enable CLK, SIMO, SOMI, CS0 as functional */
    SPI_REG(base, SPI_REG_SPIPC0) = 0x0EU;

    /* Default CS high (inactive) */
    SPI_REG(base, SPI_REG_SPIDEF) = 0x07U;

    /* Configure CS-to-CLK delay, CLK-to-CS delay */
    SPI_REG(base, SPI_REG_SPIDELAY) = (0x08U << 24) | (0x08U << 16);

    /* Enable the SPI module */
    SPI_REG(base, SPI_REG_SPIGCR1) |= SPIGCR1_ENABLE;

    spi->initialized = true;
    return MMWAVE_OK;
}

int32 hal_spi_transfer(uint32 instance, const uint8* tx_buf, uint8* rx_buf, uint32 len)
{
    if (instance >= HAL_SPI_MAX_INSTANCES || !g_spi[instance].initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (len == 0U) {
        return MMWAVE_OK;
    }

    uint32 base = g_spi[instance].base_addr;

    for (uint32 i = 0U; i < len; i++) {
        /* Write TX byte (send 0xFF dummy if tx_buf is NULL) */
        uint32 tx_byte = (tx_buf != NULL) ? (uint32)tx_buf[i] : 0xFFU;
        SPI_REG(base, SPI_REG_SPIDAT0) = tx_byte;

        /* Wait for TX complete and RX data ready */
        uint32 timeout = HAL_SPI_TIMEOUT_LOOPS;
        while (!(SPI_REG(base, SPI_REG_SPIFLG) & SPIFLG_RXINTFLAG)) {
            if (--timeout == 0U) {
                return MMWAVE_ERR_SPI_TIMEOUT;
            }
        }

        /* Read RX byte */
        uint32 rx_byte = SPI_REG(base, SPI_REG_SPIBUF) & 0xFFU;
        if (rx_buf != NULL) {
            rx_buf[i] = (uint8)rx_byte;
        }

        /* Check for overrun or bit error */
        uint32 flags = SPI_REG(base, SPI_REG_SPIFLG);
        if (flags & (SPIFLG_OVRNINTFLG | SPIFLG_BITERR)) {
            /* Clear error flags by reading SPIBUF again */
            (void)SPI_REG(base, SPI_REG_SPIBUF);
            return MMWAVE_ERR_SPI_TIMEOUT;
        }
    }

    return MMWAVE_OK;
}

int32 hal_spi_dma_transfer(uint32 instance, const uint8* tx_buf, uint8* rx_buf,
                           uint32 len, hal_spi_dma_callback_t callback)
{
    if (instance >= HAL_SPI_MAX_INSTANCES || !g_spi[instance].initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (len == 0U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    spi_instance_t* spi = &g_spi[instance];
    spi->dma_cb = callback;

    uint32 base = spi->base_addr;

    /* Configure TX DMA channel */
    if (tx_buf != NULL) {
        hal_dma_config_t tx_cfg;
        memset(&tx_cfg, 0, sizeof(tx_cfg));
        tx_cfg.channel       = (instance == 0U) ? HAL_DMA_CHANNEL_0 : HAL_DMA_CHANNEL_2;
        tx_cfg.direction     = HAL_DMA_DIR_MEM_TO_PERIPH;
        tx_cfg.data_width    = HAL_DMA_WIDTH_8BIT;
        tx_cfg.priority      = HAL_DMA_PRIORITY_HIGH;
        tx_cfg.src_addr      = (uint32)(uintptr_t)tx_buf;
        tx_cfg.dst_addr      = base + SPI_REG_SPIDAT0;
        tx_cfg.transfer_size = len;
        tx_cfg.src_increment = true;
        tx_cfg.dst_increment = false;
        tx_cfg.circular_mode = false;

        hal_dma_configure(&tx_cfg);
        hal_dma_start(tx_cfg.channel);
    }

    /* Configure RX DMA channel */
    if (rx_buf != NULL) {
        hal_dma_config_t rx_cfg;
        memset(&rx_cfg, 0, sizeof(rx_cfg));
        rx_cfg.channel       = (instance == 0U) ? HAL_DMA_CHANNEL_1 : HAL_DMA_CHANNEL_3;
        rx_cfg.direction     = HAL_DMA_DIR_PERIPH_TO_MEM;
        rx_cfg.data_width    = HAL_DMA_WIDTH_8BIT;
        rx_cfg.priority      = HAL_DMA_PRIORITY_HIGH;
        rx_cfg.src_addr      = base + SPI_REG_SPIBUF;
        rx_cfg.dst_addr      = (uint32)(uintptr_t)rx_buf;
        rx_cfg.transfer_size = len;
        rx_cfg.src_increment = false;
        rx_cfg.dst_increment = true;
        rx_cfg.circular_mode = false;

        hal_dma_configure(&rx_cfg);
        hal_dma_start(rx_cfg.channel);
    }

    /* Enable SPI DMA request lines */
    SPI_REG(base, SPI_REG_SPIINT) |= (1U << 16);  /* DMA REQ enable */

    return MMWAVE_OK;
}

int32 hal_spi_chip_select(uint32 instance, uint32 cs_num, bool active)
{
    if (instance >= HAL_SPI_MAX_INSTANCES || !g_spi[instance].initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (cs_num >= HAL_SPI_CS_MAX) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uint32 base = g_spi[instance].base_addr;
    uint32 def_val = SPI_REG(base, SPI_REG_SPIDEF);

    if (active) {
        /* CS is active low: clear the bit */
        def_val &= ~(1U << cs_num);
    } else {
        /* De-assert: set the bit high */
        def_val |= (1U << cs_num);
    }

    SPI_REG(base, SPI_REG_SPIDEF) = def_val;
    return MMWAVE_OK;
}

int32 hal_spi_deinit(uint32 instance)
{
    if (instance >= HAL_SPI_MAX_INSTANCES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    if (g_spi[instance].initialized) {
        uint32 base = g_spi[instance].base_addr;

        /* Disable the SPI module */
        SPI_REG(base, SPI_REG_SPIGCR1) &= ~SPIGCR1_ENABLE;

        /* Put module in reset */
        SPI_REG(base, SPI_REG_SPIGCR0) = 0U;

        memset(&g_spi[instance], 0, sizeof(spi_instance_t));
    }

    return MMWAVE_OK;
}
