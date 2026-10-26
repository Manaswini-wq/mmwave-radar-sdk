/**
 * @file platform_init.h
 * @brief IWR6843 Platform Initialization Interface
 * @details Hardware initialization for the IWR6843 radar sensor:
 *          clock setup, pin mux, memory init, peripheral power-up.
 */
#ifndef IWR6843_PLATFORM_INIT_H
#define IWR6843_PLATFORM_INIT_H

#include "../common/platform_types.h"

/** Platform initialization status codes */
#define PLATFORM_OK                 (0)
#define PLATFORM_ERR_CLK_FAIL       (-1)
#define PLATFORM_ERR_PLL_FAIL       (-2)
#define PLATFORM_ERR_MEM_FAIL       (-3)
#define PLATFORM_ERR_PINMUX_FAIL    (-4)
#define PLATFORM_ERR_PERIPH_FAIL    (-5)

/**
 * @brief Full platform initialization sequence
 * @return PLATFORM_OK on success, negative error on failure
 */
int32 platform_init(void);

/**
 * @brief Initialize system clocks and PLLs
 * @return PLATFORM_OK on success
 */
int32 platform_clock_init(void);

/**
 * @brief Initialize on-chip memories (TCM, L2, L3)
 * @return PLATFORM_OK on success
 */
int32 platform_memory_init(void);

/**
 * @brief Power up required peripherals
 * @return PLATFORM_OK on success
 */
int32 platform_peripheral_init(void);

/**
 * @brief Get R5F core clock frequency in Hz
 * @return Clock frequency
 */
uint32 platform_get_r5f_clock_hz(void);

/**
 * @brief Get DSP (C674x) clock frequency in Hz
 * @return Clock frequency
 */
uint32 platform_get_dsp_clock_hz(void);

#endif /* IWR6843_PLATFORM_INIT_H */
