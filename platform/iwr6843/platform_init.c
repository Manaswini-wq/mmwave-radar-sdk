/**
 * @file platform_init.c
 * @brief IWR6843 Platform Initialization Implementation
 * @details Brings up the IWR6843 hardware:
 *          - PLL configuration (200 MHz R5F, 400 MHz C674x DSP)
 *          - Pin mux setup
 *          - Memory initialization (TCMA, TCMB, L2, L3)
 *          - Peripheral power-up (SPI, UART, DMA, ADC)
 */

#include <string.h>
#include "platform_init.h"
#include "iwr6843_pinmux.h"
#include "../common/compiler_abstraction.h"
#include "../common/memory_map.h"

/* -------------------------------------------------------------------------- */
/*  IWR6843 Register Base Addresses                                           */
/* -------------------------------------------------------------------------- */
#define IWR6843_TOPRCM_BASE         0x43000000U
#define IWR6843_MSS_RCM_BASE        0x43020000U
#define IWR6843_DSS_RCM_BASE        0x43040000U

/* PLL registers (offsets from TOPRCM_BASE) */
#define PLL_CORE_CLKCTL             0x0100U
#define PLL_CORE_CLKSTAT            0x0104U
#define PLL_DSP_CLKCTL              0x0200U
#define PLL_DSP_CLKSTAT             0x0204U
#define PLL_LOCK_BIT                (1U << 0)

/* Memory region addresses */
#define IWR6843_TCMA_BASE           0x00000000U
#define IWR6843_TCMA_SIZE           (16U * 1024U)   /* 16 KB */
#define IWR6843_TCMB_BASE           0x00080000U
#define IWR6843_TCMB_SIZE           (16U * 1024U)   /* 16 KB */
#define IWR6843_L2RAM_BASE          0x50800000U
#define IWR6843_L2RAM_SIZE          (192U * 1024U)  /* 192 KB */
#define IWR6843_L3RAM_BASE          0x51000000U
#define IWR6843_L3RAM_SIZE          (384U * 1024U)  /* 384 KB */

/* Peripheral power control (offsets from MSS_RCM_BASE) */
#define PERIPH_PWR_SPI              0x0400U
#define PERIPH_PWR_UART             0x0404U
#define PERIPH_PWR_DMA              0x0408U
#define PERIPH_PWR_ADC              0x040CU
#define PERIPH_PWR_ENABLE           (1U << 0)

/* Clock frequencies */
#define R5F_CLOCK_HZ                200000000U  /* 200 MHz */
#define DSP_CLOCK_HZ                400000000U  /* 400 MHz C674x */

/* PLL multiplier/divider for 200 MHz R5F from 40 MHz XTAL */
#define PLL_CORE_MUL                10U
#define PLL_CORE_DIV                2U
/* PLL for 400 MHz DSP from 40 MHz XTAL */
#define PLL_DSP_MUL                 20U
#define PLL_DSP_DIV                 2U

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
    reg_write(IWR6843_TOPRCM_BASE, PLL_CORE_CLKCTL,
              (PLL_CORE_MUL << 16) | (PLL_CORE_DIV << 0));

    uint32 timeout = PLL_LOCK_TIMEOUT;
    while (!(reg_read(IWR6843_TOPRCM_BASE, PLL_CORE_CLKSTAT) & PLL_LOCK_BIT)) {
        if (--timeout == 0) { return PLATFORM_ERR_PLL_FAIL; }
    }

    /* Configure DSP PLL: 40 MHz * 20 / 2 = 400 MHz */
    reg_write(IWR6843_TOPRCM_BASE, PLL_DSP_CLKCTL,
              (PLL_DSP_MUL << 16) | (PLL_DSP_DIV << 0));

    timeout = PLL_LOCK_TIMEOUT;
    while (!(reg_read(IWR6843_TOPRCM_BASE, PLL_DSP_CLKSTAT) & PLL_LOCK_BIT)) {
        if (--timeout == 0) { return PLATFORM_ERR_PLL_FAIL; }
    }

    return PLATFORM_OK;
}

/* -------------------------------------------------------------------------- */
/*  Memory Initialization                                                     */
/* -------------------------------------------------------------------------- */
int32 platform_memory_init(void)
{
    memset((void*)IWR6843_TCMA_BASE,  0, IWR6843_TCMA_SIZE);
    memset((void*)IWR6843_TCMB_BASE,  0, IWR6843_TCMB_SIZE);
    memset((void*)IWR6843_L2RAM_BASE, 0, IWR6843_L2RAM_SIZE);
    memset((void*)IWR6843_L3RAM_BASE, 0, IWR6843_L3RAM_SIZE);
    return PLATFORM_OK;
}

/* -------------------------------------------------------------------------- */
/*  Peripheral Power-Up                                                       */
/* -------------------------------------------------------------------------- */
int32 platform_peripheral_init(void)
{
    reg_write(IWR6843_MSS_RCM_BASE, PERIPH_PWR_SPI,  PERIPH_PWR_ENABLE);
    reg_write(IWR6843_MSS_RCM_BASE, PERIPH_PWR_UART, PERIPH_PWR_ENABLE);
    reg_write(IWR6843_MSS_RCM_BASE, PERIPH_PWR_DMA,  PERIPH_PWR_ENABLE);
    reg_write(IWR6843_MSS_RCM_BASE, PERIPH_PWR_ADC,  PERIPH_PWR_ENABLE);
    return PLATFORM_OK;
}

/* -------------------------------------------------------------------------- */
/*  Top-Level Init                                                            */
/* -------------------------------------------------------------------------- */
int32 platform_init(void)
{
    int32 status;

    status = platform_clock_init();
    if (status != PLATFORM_OK) { return status; }

    status = iwr6843_pinmux_init();
    if (status != PLATFORM_OK) { return PLATFORM_ERR_PINMUX_FAIL; }

    status = platform_memory_init();
    if (status != PLATFORM_OK) { return status; }

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
