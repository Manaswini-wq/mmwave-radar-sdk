/**
 * @file awr2944_pinmux.h
 * @brief AWR2944 Pin Mux Configuration Interface
 * @details Pin definitions and mux control for SPI, UART, GPIO, and CSI-2
 *          peripherals on the AWR2944 radar sensor.
 */
#ifndef AWR2944_PINMUX_H
#define AWR2944_PINMUX_H

#include "../common/platform_types.h"

/* -------------------------------------------------------------------------- */
/*  IOMUX Register Base                                                       */
/* -------------------------------------------------------------------------- */
#define AWR2944_IOMUX_BASE          0x43010000U

/* -------------------------------------------------------------------------- */
/*  Pin Mux Modes                                                             */
/* -------------------------------------------------------------------------- */
#define PINMUX_MODE_0               0x00U   /**< Primary function */
#define PINMUX_MODE_1               0x01U   /**< Alternate function 1 */
#define PINMUX_MODE_2               0x02U   /**< Alternate function 2 */
#define PINMUX_MODE_3               0x03U   /**< Alternate function 3 */
#define PINMUX_MODE_GPIO            0x07U   /**< GPIO mode */

/* -------------------------------------------------------------------------- */
/*  Pin Control Flags                                                         */
/* -------------------------------------------------------------------------- */
#define PINMUX_PULL_DISABLE         (1U << 8)
#define PINMUX_PULL_UP              (1U << 9)
#define PINMUX_INPUT_ENABLE         (1U << 10)
#define PINMUX_SLEW_FAST            (1U << 11)

/* -------------------------------------------------------------------------- */
/*  SPI Pin Offsets (IOMUX register offsets)                                  */
/* -------------------------------------------------------------------------- */
#define PIN_SPI_CLK                 0x0040U
#define PIN_SPI_MOSI                0x0044U
#define PIN_SPI_MISO                0x0048U
#define PIN_SPI_CS0                 0x004CU
#define PIN_SPI_CS1                 0x0050U

/* -------------------------------------------------------------------------- */
/*  UART Pin Offsets                                                          */
/* -------------------------------------------------------------------------- */
#define PIN_UART_TX                 0x0060U
#define PIN_UART_RX                 0x0064U

/* -------------------------------------------------------------------------- */
/*  GPIO Pin Offsets                                                          */
/* -------------------------------------------------------------------------- */
#define PIN_GPIO_0                  0x0080U
#define PIN_GPIO_1                  0x0084U
#define PIN_GPIO_2                  0x0088U
#define PIN_GPIO_3                  0x008CU

/* -------------------------------------------------------------------------- */
/*  CSI-2 Pin Offsets                                                         */
/* -------------------------------------------------------------------------- */
#define PIN_CSI2_CLK_P              0x00A0U
#define PIN_CSI2_CLK_N              0x00A4U
#define PIN_CSI2_D0_P               0x00A8U
#define PIN_CSI2_D0_N               0x00ACU
#define PIN_CSI2_D1_P               0x00B0U
#define PIN_CSI2_D1_N               0x00B4U

/* -------------------------------------------------------------------------- */
/*  API                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize pin mux for all AWR2944 peripherals
 * @return 0 on success, negative on failure
 */
int32 awr2944_pinmux_init(void);

/**
 * @brief Configure a single pin
 * @param offset  IOMUX register offset for the pin
 * @param mode    Pin mux mode (PINMUX_MODE_x)
 * @param flags   Control flags (pull, input enable, slew)
 */
void awr2944_pinmux_set(uint32 offset, uint32 mode, uint32 flags);

#endif /* AWR2944_PINMUX_H */
