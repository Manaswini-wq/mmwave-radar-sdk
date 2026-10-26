/**
 * @file hal_interrupt.h
 * @brief Interrupt Controller HAL for mmWave Radar SDK
 * @details VIM (Vectored Interrupt Manager) based interrupt controller
 *          abstraction for ARM R5F / Cortex-R cores used in TI mmWave sensors.
 */
#ifndef HAL_INTERRUPT_H
#define HAL_INTERRUPT_H

#include "mmwave_types.h"

/** Maximum number of interrupt vectors supported */
#define HAL_IRQ_MAX_VECTORS         64U

/** Radar-specific IRQ numbers (VIM channel assignments) */
#define IRQ_FRAME_DONE              16U   /**< Frame processing complete */
#define IRQ_CHIRP_AVAIL             17U   /**< Chirp data available in ADC buffer */
#define IRQ_CHIRP_ERR               18U   /**< Chirp timing or sequencing error */
#define IRQ_ADC_BUFFER_FULL         19U   /**< ADC ping/pong buffer full */
#define IRQ_DMA_COMPLETE_CH0        20U   /**< DMA channel 0 transfer complete */
#define IRQ_DMA_COMPLETE_CH1        21U   /**< DMA channel 1 transfer complete */
#define IRQ_DMA_ERROR               22U   /**< DMA transfer error */
#define IRQ_SPI_COMPLETE            23U   /**< SPI transfer complete */
#define IRQ_UART_RX                 24U   /**< UART receive interrupt */
#define IRQ_UART_TX                 25U   /**< UART transmit complete */
#define IRQ_TIMER0                  26U   /**< General-purpose timer 0 */
#define IRQ_TIMER1                  27U   /**< General-purpose timer 1 */
#define IRQ_GPIO_PORT_A             28U   /**< GPIO port A interrupt */
#define IRQ_GPIO_PORT_B             29U   /**< GPIO port B interrupt */
#define IRQ_CAL_DONE                30U   /**< Calibration sequence complete */
#define IRQ_TEMP_SENSOR             31U   /**< Temperature sensor threshold alert */

/** Interrupt priority levels */
#define HAL_IRQ_PRIORITY_HIGHEST    0U
#define HAL_IRQ_PRIORITY_HIGH       1U
#define HAL_IRQ_PRIORITY_MEDIUM     2U
#define HAL_IRQ_PRIORITY_LOW        3U

/** ISR handler function type */
typedef void (*hal_isr_handler_t)(void* arg);

/**
 * @brief Initialize the interrupt controller (VIM)
 * @return MMWAVE_OK on success
 */
int32 hal_interrupt_init(void);

/**
 * @brief Register an interrupt handler
 * @param[in] irq_num   IRQ number (use IRQ_* defines)
 * @param[in] handler   ISR handler function
 * @param[in] arg       User argument passed to handler
 * @return MMWAVE_OK on success
 */
int32 hal_interrupt_register(uint32 irq_num, hal_isr_handler_t handler, void* arg);

/**
 * @brief Unregister an interrupt handler
 * @param[in] irq_num  IRQ number
 * @return MMWAVE_OK on success
 */
int32 hal_interrupt_unregister(uint32 irq_num);

/**
 * @brief Enable a specific interrupt
 * @param[in] irq_num  IRQ number
 * @return MMWAVE_OK on success
 */
int32 hal_interrupt_enable(uint32 irq_num);

/**
 * @brief Disable a specific interrupt
 * @param[in] irq_num  IRQ number
 * @return MMWAVE_OK on success
 */
int32 hal_interrupt_disable(uint32 irq_num);

/**
 * @brief Set interrupt priority
 * @param[in] irq_num   IRQ number
 * @param[in] priority  Priority level (0 = highest)
 * @return MMWAVE_OK on success
 */
int32 hal_interrupt_set_priority(uint32 irq_num, uint32 priority);

/**
 * @brief Globally disable interrupts (enter critical section)
 * @return Previous interrupt state (for nested disable/restore)
 */
uint32 hal_interrupt_disable_global(void);

/**
 * @brief Globally restore interrupts (exit critical section)
 * @param[in] prev_state  State returned by hal_interrupt_disable_global
 */
void hal_interrupt_restore_global(uint32 prev_state);

/**
 * @brief De-initialize the interrupt controller
 * @return MMWAVE_OK on success
 */
int32 hal_interrupt_deinit(void);

#endif /* HAL_INTERRUPT_H */
