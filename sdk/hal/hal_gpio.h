/**
 * @file hal_gpio.h
 * @brief GPIO Hardware Abstraction Layer for mmWave Radar SDK
 * @details General-purpose I/O driver for radar control signals including
 *          NRESET, SOP pins, LED indicators, and interrupt inputs.
 */
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "mmwave_types.h"

/** GPIO port identifiers */
#define HAL_GPIO_PORT_A             0U
#define HAL_GPIO_PORT_B             1U
#define HAL_GPIO_PORT_C             2U
#define HAL_GPIO_MAX_PORTS          3U

/** GPIO pin direction */
#define HAL_GPIO_DIR_INPUT          0U
#define HAL_GPIO_DIR_OUTPUT         1U

/** GPIO pin state */
#define HAL_GPIO_LOW                0U
#define HAL_GPIO_HIGH               1U

/** GPIO interrupt edge trigger */
#define HAL_GPIO_INT_NONE           0U
#define HAL_GPIO_INT_RISING         1U
#define HAL_GPIO_INT_FALLING        2U
#define HAL_GPIO_INT_BOTH           3U

/** Well-known radar control pins (port, pin) */
#define HAL_GPIO_NRESET_PORT        HAL_GPIO_PORT_A
#define HAL_GPIO_NRESET_PIN         0U
#define HAL_GPIO_SOP0_PORT          HAL_GPIO_PORT_A
#define HAL_GPIO_SOP0_PIN           1U
#define HAL_GPIO_SOP1_PORT          HAL_GPIO_PORT_A
#define HAL_GPIO_SOP1_PIN           2U
#define HAL_GPIO_LED_STATUS_PORT    HAL_GPIO_PORT_B
#define HAL_GPIO_LED_STATUS_PIN     0U

/** GPIO interrupt callback */
typedef void (*hal_gpio_isr_callback_t)(uint32 port, uint32 pin, void* user_data);

/**
 * @brief Initialize the GPIO subsystem
 * @return MMWAVE_OK on success
 */
int32 hal_gpio_init(void);

/**
 * @brief Configure a GPIO pin direction
 * @param[in] port  GPIO port
 * @param[in] pin   Pin number within port (0-31)
 * @param[in] dir   Direction (HAL_GPIO_DIR_INPUT/OUTPUT)
 * @return MMWAVE_OK on success
 */
int32 hal_gpio_set_direction(uint32 port, uint32 pin, uint32 dir);

/**
 * @brief Write a GPIO pin state
 * @param[in] port  GPIO port
 * @param[in] pin   Pin number
 * @param[in] value HAL_GPIO_LOW or HAL_GPIO_HIGH
 * @return MMWAVE_OK on success
 */
int32 hal_gpio_write(uint32 port, uint32 pin, uint32 value);

/**
 * @brief Read a GPIO pin state
 * @param[in]  port  GPIO port
 * @param[in]  pin   Pin number
 * @param[out] value Pointer to store pin state
 * @return MMWAVE_OK on success
 */
int32 hal_gpio_read(uint32 port, uint32 pin, uint32* value);

/**
 * @brief Toggle a GPIO output pin
 * @param[in] port  GPIO port
 * @param[in] pin   Pin number
 * @return MMWAVE_OK on success
 */
int32 hal_gpio_toggle(uint32 port, uint32 pin);

/**
 * @brief Configure GPIO pin interrupt
 * @param[in] port      GPIO port
 * @param[in] pin       Pin number
 * @param[in] edge      Edge trigger (HAL_GPIO_INT_RISING/FALLING/BOTH)
 * @param[in] callback  ISR callback function
 * @param[in] user_data User context pointer
 * @return MMWAVE_OK on success
 */
int32 hal_gpio_config_interrupt(uint32 port, uint32 pin, uint32 edge,
                                hal_gpio_isr_callback_t callback, void* user_data);

/**
 * @brief De-initialize the GPIO subsystem
 * @return MMWAVE_OK on success
 */
int32 hal_gpio_deinit(void);

#endif /* HAL_GPIO_H */
