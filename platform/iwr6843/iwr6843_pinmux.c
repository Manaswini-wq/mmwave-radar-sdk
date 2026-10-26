/**
 * @file iwr6843_pinmux.c
 * @brief IWR6843 Pin Mux Configuration Implementation
 * @details Configures IOMUX registers for SPI, UART, and GPIO
 *          peripheral pin assignments on the IWR6843 radar sensor.
 *          Note: IWR6843 does not have CSI-2 (unlike AWR2944).
 */

#include "iwr6843_pinmux.h"
#include "../common/compiler_abstraction.h"

/* -------------------------------------------------------------------------- */
/*  Register Access                                                           */
/* -------------------------------------------------------------------------- */
static MMWAVE_INLINE void iomux_write(uint32 offset, uint32 value)
{
    volatile uint32* reg = (volatile uint32*)(IWR6843_IOMUX_BASE + offset);
    *reg = value;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                */
/* -------------------------------------------------------------------------- */

void iwr6843_pinmux_set(uint32 offset, uint32 mode, uint32 flags)
{
    iomux_write(offset, mode | flags);
}

int32 iwr6843_pinmux_init(void)
{
    /* ---- SPI (Mode 0): Radar front-end communication ---- */
    iwr6843_pinmux_set(PIN_SPI_CLK,  PINMUX_MODE_0, PINMUX_SLEW_FAST);
    iwr6843_pinmux_set(PIN_SPI_MOSI, PINMUX_MODE_0, PINMUX_SLEW_FAST);
    iwr6843_pinmux_set(PIN_SPI_MISO, PINMUX_MODE_0,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_UP);
    iwr6843_pinmux_set(PIN_SPI_CS0,  PINMUX_MODE_0, 0);

    /* ---- UART (Mode 1): Debug console ---- */
    iwr6843_pinmux_set(PIN_UART_TX, PINMUX_MODE_1, PINMUX_SLEW_FAST);
    iwr6843_pinmux_set(PIN_UART_RX, PINMUX_MODE_1,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_UP);

    /* ---- GPIO (Mode 7): General purpose I/O ---- */
    iwr6843_pinmux_set(PIN_GPIO_0, PINMUX_MODE_GPIO,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_DISABLE);
    iwr6843_pinmux_set(PIN_GPIO_1, PINMUX_MODE_GPIO,
                        PINMUX_INPUT_ENABLE | PINMUX_PULL_DISABLE);
    iwr6843_pinmux_set(PIN_GPIO_2, PINMUX_MODE_GPIO, PINMUX_PULL_DISABLE);
    iwr6843_pinmux_set(PIN_GPIO_3, PINMUX_MODE_GPIO, PINMUX_PULL_DISABLE);

    return 0;
}
