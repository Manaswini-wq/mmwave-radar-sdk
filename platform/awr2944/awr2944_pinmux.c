/**
 * @file awr2944_pinmux.c
 * @brief AWR2944 Pin Mux Configuration Implementation
 * @details Configures IOMUX registers for SPI, UART, GPIO, and CSI-2
 *          peripheral pin assignments on the AWR2944 radar sensor.
 */

#include "awr2944_pinmux.h"
#include "../common/compiler_abstraction.h"

/* -------------------------------------------------------------------------- */
/*  Register Access                                                           */
/* -------------------------------------------------------------------------- */
static MMWAVE_INLINE void iomux_write(uint32 offset, uint32 value)
{
    volatile uint32* reg = (volatile uint32*)(AWR2944_IOMUX_BASE + offset);
    *reg = value;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

void awr2944_pinmux_set(uint32 offset, uint32 mode, uint32 flags)
{
    iomux_write(offset, mode | flags);
}

int32 awr2944_pinmux_init(void)
{
    /* ---- SPI (Mode 0): Radar front-end communication ---- */
    awr2944_pinmux_set(PIN_SPI_CLK,  PINMUX_MODE_0, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_SPI_MOSI, PINMUX_MODE_0, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_SPI_MISO, PINMUX_MODE_0,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_UP);
    awr2944_pinmux_set(PIN_SPI_CS0,  PINMUX_MODE_0, 0);
    awr2944_pinmux_set(PIN_SPI_CS1,  PINMUX_MODE_0, 0);

    /* ---- UART (Mode 1): Debug console ---- */
    awr2944_pinmux_set(PIN_UART_TX, PINMUX_MODE_1, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_UART_RX, PINMUX_MODE_1,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_UP);

    /* ---- GPIO (Mode 7): General purpose I/O ---- */
    awr2944_pinmux_set(PIN_GPIO_0, PINMUX_MODE_GPIO,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_DISABLE);
    awr2944_pinmux_set(PIN_GPIO_1, PINMUX_MODE_GPIO,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_DISABLE);
    awr2944_pinmux_set(PIN_GPIO_2, PINMUX_MODE_GPIO, PINMUX_PULL_DISABLE);
    awr2944_pinmux_set(PIN_GPIO_3, PINMUX_MODE_GPIO, PINMUX_PULL_DISABLE);

    /* ---- CSI-2 (Mode 2): High-speed data output ---- */
    awr2944_pinmux_set(PIN_CSI2_CLK_P, PINMUX_MODE_2, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_CSI2_CLK_N, PINMUX_MODE_2, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_CSI2_D0_P,  PINMUX_MODE_2, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_CSI2_D0_N,  PINMUX_MODE_2, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_CSI2_D1_P,  PINMUX_MODE_2, PINMUX_SLEW_FAST);
    awr2944_pinmux_set(PIN_CSI2_D1_N,  PINMUX_MODE_2, PINMUX_SLEW_FAST);

    return 0;
}
