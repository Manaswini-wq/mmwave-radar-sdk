/**
 * @file platform_init.c
 * @brief AWR2944 Platform Initialization Implementation
 * @details Brings up the AWR2944 hardware:
 *          - PLL configuration (200 MHz R5F, 450 MHz DSP)
 *          - Pin mux setup
 *          - Memory initialization (TCMA, TCMB, L2, L3)
 *          - Peripheral power-up (SPI, UART, DMA, ADC)
 */

#include <string.h>
#include "platform_init.h"
#include "awr2944_pinmux.h"
#include "../common/compiler_abstraction.h"
#include "../common/memory_map.h"

/* -------------------------------------------------------------------------- */
/*  AWR2944 Register Base Addresses                                           */
/* -------------------------------------------------------------------------- */
#define AWR2944_TOPRCM_BASE         0x43000000U
#define AWR2944_MSS_RCM_BASE        0x43020000U
#define AWR2944_DSS_RCM_BASE        0x43040000U
#define AWR2944_MSS_CTRL_BASE       0x43030000U

/* PLL registers (offsets from TOPRCM_BASE) */
#define PLL_CORE_CLKCTL             0x0100U
#define PLL_CORE_CLKSTAT            0x0104U
#define PLL_DSP_CLKCTL              0x0200U
#define PLL_DSP_CLKSTAT             0x0204U
#define PLL_LOCK_BIT                (1U << 0)

/* Memory region addresses */
#define AWR2944_TCMA_BASE           0x00000000U
#define AWR2944_TCMA_SIZE           (32U * 1024U)
#define AWR2944_TCMB_BASE          0x00080000U
#define AWR2944_TCMB_SIZE           (32U * 1024U)
#define AWR2944_L2RAM_BASE          0x50800000U
#define AWR2944_L2RAM_SIZE          (256U * 1024U)
#define AWR2944_L3RAM_BASE          0x51000000U
#define AWR2944_L3RAM_SIZE          (512U * 1024U)

/* Peripheral power control (offsets from MSS_RCM_BASE) */
#define PERIPH_PWR_SPI              0x0400U
#define PERIPH_PWR_UART             0x0404U
#define PERIPH_PWR_DMA              0x0408U
#define PERIPH_PWR_ADC              0x040CU
#define PERIPH_PWR_ENABLE           (1U << 0)

/* Clock frequencies */
#define R5F_CLOCK_HZ                200000000U  /* 200 MHz */
#define DSP_CLOCK_HZ                450000000U  /* 450 MHz */

/* PLL multiplier/divider for 200 MHz R5F from 40 MHz XTAL */
#define PLL_CORE_MUL                10U
#define PLL_CORE_DIV                2U
/* PLL for 450 MHz DSP from 40 MHz XTAL */
#define PLL_DSP_MUL                 45U
#define PLL_DSP_DIV                 4U

#define PLL_LOCK_TIMEOUT            10000U

/* -------------------------------------------------------------------------- */
/*  Register Access Helpers                                                   */
/* -------------------------------------------------------------------------- */
static MMWAVE_INLINE void reg_write(uint32 base, uint32 offset, uint32 value)
{
    volatile uint32* reg = (volatile uint32*)(base + offset);
    *reg = value;
}

static MMWAVE_INLINE uint32 reg_read(uint32 base, uint32 offset)
{
    volatile uint32* reg = (volatile uint32*)(base + offset);
    return *reg;
}

/* -------------------------------------------------------------------------- */
/*  Clock / PLL Configuration                                                 */
/* -------------------------------------------------------------------------- */
int32 platform_clock_init(void)
{
    /* Configure R5F core PLL: 40 MHz * 10 / 2 = 200 MHz */
    reg_write(AWR2944_TOPRCM_BASE, PLL_CORE_CLKCTL,
              (PLL_CORE_MUL << 16) | (PLL_CORE_DIV << 0));

    uint32 timeout = PLL_LOCK_TIMEOUT;
    while (!(reg_read(AWR2944_TOPRCM_BASE, PLL_CORE_CLKSTAT) & PLL_LOCK_BIT)) {
        if (--timeout == 0) { return PLATFORM_ERR_PLL_FAIL; }
    }

    /* Configure DSP PLL: 40 MHz * 45 / 4 = 450 MHz */
    reg_write(AWR2944_TOPRCM_BASE, PLL_DSP_CLKCTL,
              (PLL_DSP_MUL << 16) | (PLL_DSP_DIV << 0));

    timeout = PLL_LOCK_TIMEOUT;
    while (!(reg_read(AWR2944_TOPRCM_BASE, PLL_DSP_CLKSTAT) & PLL_LOCK_BIT)) {
        if (--timeout == 0) { return PLATFORM_ERR_PLL_FAIL; }
    }

    return PLATFORM_OK;
}

/* -------------------------------------------------------------------------- */
/*  Memory Initialization                                                     */
/* -------------------------------------------------------------------------- */
int32 platform_memory_init(void)
{
    /* Zero-initialize TCM A */
    memset((void*)AWR2944_TCMA_BASE, 0, AWR2944_TCMA_SIZE);

    /* Zero-initialize TCM B */
    memset((void*)AWR2944_TCMB_BASE, 0, AWR2944_TCMB_SIZE);

    /* Zero-initialize L2 RAM */
    memset((void*)AWR2944_L2RAM_BASE, 0, AWR2944_L2RAM_SIZE);

    /* Zero-initialize L3 RAM */
    memset((void*)AWR2944_L3RAM_BASE, 0, AWR2944_L3RAM_SIZE);

    return PLATFORM_OK;
}

/* -------------------------------------------------------------------------- */
/*  Peripheral Power-Up                                                       */
/* -------------------------------------------------------------------------- */
int32 platform_peripheral_init(void)
{
    /* Enable SPI (for radar front-end communication) */
    reg_write(AWR2944_MSS_RCM_BASE, PERIPH_PWR_SPI, PERIPH_PWR_ENABLE);

    /* Enable UART (debug console) */
    reg_write(AWR2944_MSS_RCM_BASE, PERIPH_PWR_UART, PERIPH_PWR_ENABLE);

    /* Enable DMA (ADC data transfer) */
    reg_write(AWR2944_MSS_RCM_BASE, PERIPH_PWR_DMA, PERIPH_PWR_ENABLE);

    /* Enable ADC subsystem */
    reg_write(AWR2944_MSS_RCM_BASE, PERIPH_PWR_ADC, PERIPH_PWR_ENABLE);

    return PLATFORM_OK;
}

/* -------------------------------------------------------------------------- */
/*  Top-Level Init                                                            */
/* -------------------------------------------------------------------------- */
int32 platform_init(void)
{
    int32 status;

    /* 1. Clock and PLL setup */
    status = platform_clock_init();
    if (status != PLATFORM_OK) { return status; }

    /* 2. Pin mux configuration */
    status = awr2944_pinmux_init();
    if (status != PLATFORM_OK) { return PLATFORM_ERR_PINMUX_FAIL; }

    /* 3. Memory initialization */
    status = platform_memory_init();
    if (status != PLATFORM_OK) { return status; }

    /* 4. Peripheral power-up */
    status = platform_peripheral_init();
    if (status != PLATFORM_OK) { return status; }

    return PLATFORM_OK;
}

/* -------------------------------------------------------------------------- */
/*  Clock Query                                                               */
/* -------------------------------------------------------------------------- */
uint32 platform_get_r5f_clock_hz(void)
{
    return R5F_CLOCK_HZ;
}

uint32 platform_get_dsp_clock_hz(void)
{
    return DSP_CLOCK_HZ;
}
