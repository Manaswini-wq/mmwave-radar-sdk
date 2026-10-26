/**
 * @file iwr6843_pinmux.h
 * @brief IWR6843 Pin Mux Configuration Interface
 * @details Pin definitions and mux control for SPI, UART, and GPIO
 *          peripherals on the IWR6843 radar sensor.
 */
#ifndef IWR6843_PINMUX_H
#define IWR6843_PINMUX_H

#include "../common/platform_types.h"

/* -------------------------------------------------------------------------- */
/*  IOMUX Register Base                                                       */
/* -------------------------------------------------------------------------- */
#define IWR6843_IOMUX_BASE          0x43010000U

/* -------------------------------------------------------------------------- */
/*  Pin Mux Modes                                                             */
/* -------------------------------------------------------------------------- */
#define PINMUX_MODE_0               0x00U   /**< Primary function */
#define PINMUX_MODE_1               0x01U   /**< Alternate function 1 */
#define PINMUX_MODE_2               0x02U   /**< Alternate function 2 */
#define PINMUX_MODE_GPIO            0x07U   /**< GPIO mode */

/* -------------------------------------------------------------------------- */
/*  Pin Control Flags                                                         */
/* -------------------------------------------------------------------------- */
#define PINMUX_PULL_DISABLE         (1U << 8)
#define PINMUX_PULL_UP              (1U << 9)
#define PINMUX_INPUT_ENABLE         (1U << 10)
#define PINMUX_SLEW_FAST            (1U << 11)

/* -------------------------------------------------------------------------- */
/*  SPI Pin Offsets                                                           */
/* -------------------------------------------------------------------------- */
#define PIN_SPI_CLK                 0x0030U
#define PIN_SPI_MOSI                0x0034U
#define PIN_SPI_MISO                0x0038U
#define PIN_SPI_CS0                 0x003CU

/* -------------------------------------------------------------------------- */
/*  UART Pin Offsets                                                          */
/* -------------------------------------------------------------------------- */
#define PIN_UART_TX                 0x0050U
#define PIN_UART_RX                 0x0054U

/* -------------------------------------------------------------------------- */
/*  GPIO Pin Offsets                                                          */
/* -------------------------------------------------------------------------- */
#define PIN_GPIO_0                  0x0070U
#define PIN_GPIO_1                  0x0074U
#define PIN_GPIO_2                  0x0078U
#define PIN_GPIO_3                  0x007CU

/* -------------------------------------------------------------------------- */
/*  API                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize pin mux for all IWR6843 peripherals
 * @return 0 on success, negative on failure
 */
int32 iwr6843_pinmux_init(void);

/**
 * @brief Configure a single pin
 * @param offset  IOMUX register offset for the pin
 * @param mode    Pin mux mode (PINMUX_MODE_x)
 * @param flags   Control flags (pull, input enable, slew)
 */
void iwr6843_pinmux_set(uint32 offset, uint32 mode, uint32 flags);

#endif /* IWR6843_PINMUX_H */
