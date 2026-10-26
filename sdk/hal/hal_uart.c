/**
 * @file hal_uart.c
 * @brief UART HAL implementation for mmWave Radar SDK
 * @details Interrupt-driven UART driver with ring-buffer receive.
 *          Supports the SCI/UART peripheral on TI AWR/IWR mmWave SoCs.
 */
#include "hal_uart.h"
#include "hal_interrupt.h"
#include "mmwave_error_codes.h"

#include <string.h>

/* ---- UART Register Map ---- */
#define UART0_BASE_ADDR             0xFFF7E400U
#define UART1_BASE_ADDR             0xFFF7E500U

#define UART_REG_SCIGCR0            0x00U   /**< Global control 0 (reset)    */
#define UART_REG_SCIGCR1            0x04U   /**< Global control 1 (enable)   */
#define UART_REG_SCISETINT          0x0CU   /**< Interrupt enable set        */
#define UART_REG_SCICLRINT          0x10U   /**< Interrupt enable clear      */
#define UART_REG_SCIFLR             0x1CU   /**< Flag register               */
#define UART_REG_SCIRD              0x34U   /**< Receive data register       */
#define UART_REG_SCITD              0x38U   /**< Transmit data register      */
#define UART_REG_SCIPIO0            0x3CU   /**< Pin I/O control 0           */
#define UART_REG_SCIBAUD            0x24U   /**< Baud rate register (BRS)    */

/* Flag register bits */
#define SCIFLR_TXRDY                (1U << 8)
#define SCIFLR_RXRDY                (1U << 9)
#define SCIFLR_TX_EMPTY             (1U << 11)
#define SCIFLR_FE                   (1U << 2)
#define SCIFLR_OE                   (1U << 1)

/* GCR1 bits */
#define SCIGCR1_SWNRST             (1U << 7)
#define SCIGCR1_TXENA               (1U << 25)
#define SCIGCR1_RXENA               (1U << 24)

/* Module clock */
#define UART_MODULE_CLK_HZ          200000000U

/* ---- Register access macro ---- */
#define UART_REG(base, off)  (*(volatile uint32*)((base) + (off)))

/* ---- Ring buffer structure ---- */
typedef struct {
    uint8   buffer[HAL_UART_RX_BUF_SIZE];
    volatile uint32 head;
    volatile uint32 tail;
} ring_buffer_t;

/* ---- Per-instance state ---- */
typedef struct {
    uint32                  base_addr;
    bool                    initialized;
    ring_buffer_t           rx_ring;
    hal_uart_rx_callback_t  rx_callback;
} uart_instance_t;

static uart_instance_t g_uart[HAL_UART_MAX_INSTANCES];

static const uint32 uart_base_addrs[HAL_UART_MAX_INSTANCES] = {
    UART0_BASE_ADDR,
    UART1_BASE_ADDR
};

static const uint32 uart_irq_nums[HAL_UART_MAX_INSTANCES] = {
    IRQ_UART_RX,
    IRQ_UART_RX  /* Second instance shares vector in simplified model */
};

/* Forward declaration of ISR */
static void uart_rx_isr(void* arg);

/* ---- Ring buffer helpers ---- */
static inline void ring_buf_reset(ring_buffer_t* rb)
{
    rb->head = 0U;
    rb->tail = 0U;
}

static inline bool ring_buf_is_empty(const ring_buffer_t* rb)
{
    return (rb->head == rb->tail);
}

static inline bool ring_buf_put(ring_buffer_t* rb, uint8 byte)
{
    uint32 next = (rb->head + 1U) & (HAL_UART_RX_BUF_SIZE - 1U);
    if (next == rb->tail) {
        return false;  /* Buffer full */
    }
    rb->buffer[rb->head] = byte;
    rb->head = next;
    return true;
}

static inline bool ring_buf_get(ring_buffer_t* rb, uint8* byte)
{
    if (ring_buf_is_empty(rb)) {
        return false;
    }
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1U) & (HAL_UART_RX_BUF_SIZE - 1U);
    return true;
}

/* ---- Public API ---- */

int32 hal_uart_init(uint32 instance, uint32 baud_rate)
{
    if (instance >= HAL_UART_MAX_INSTANCES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (baud_rate == 0U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uart_instance_t* uart = &g_uart[instance];
    uart->base_addr   = uart_base_addrs[instance];
    uart->rx_callback = NULL;
    ring_buf_reset(&uart->rx_ring);

    uint32 base = uart->base_addr;

    /* Reset the SCI module */
    UART_REG(base, UART_REG_SCIGCR0) = 0U;
    UART_REG(base, UART_REG_SCIGCR0) = 1U;  /* Release reset */

    /* Configure: 8-bit, no parity, 1 stop, async mode */
    UART_REG(base, UART_REG_SCIGCR1) = (1U << 5)  /* TIMING_MODE async */
                                      | (1U << 4)   /* 1 stop bit */
                                      | (3U << 1);  /* 8-bit char length */

    /* Set baud rate: BRS = (VCLK / (16 * baud)) - 1 */
    uint32 brs = (UART_MODULE_CLK_HZ / (16U * baud_rate)) - 1U;
    UART_REG(base, UART_REG_SCIBAUD) = brs;

    /* Enable TX and RX pins as functional */
    UART_REG(base, UART_REG_SCIPIO0) = 0x06U;  /* TX, RX functional */

    /* Enable RX interrupt */
    UART_REG(base, UART_REG_SCISETINT) = (1U << 9);  /* RXRDY interrupt */

    /* Register ISR for UART RX */
    hal_interrupt_register(uart_irq_nums[instance], uart_rx_isr, (void*)(uintptr_t)instance);
    hal_interrupt_enable(uart_irq_nums[instance]);

    /* Enable TX and RX, bring module out of reset */
    UART_REG(base, UART_REG_SCIGCR1) |= SCIGCR1_TXENA | SCIGCR1_RXENA | SCIGCR1_SWNRST;

    uart->initialized = true;
    return MMWAVE_OK;
}

int32 hal_uart_send(uint32 instance, const uint8* data, uint32 len)
{
    if (instance >= HAL_UART_MAX_INSTANCES || !g_uart[instance].initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (data == NULL || len == 0U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uint32 base = g_uart[instance].base_addr;

    for (uint32 i = 0U; i < len; i++) {
        /* Wait for TX ready */
        while (!(UART_REG(base, UART_REG_SCIFLR) & SCIFLR_TXRDY)) {
            /* spin */
        }
        UART_REG(base, UART_REG_SCITD) = (uint32)data[i];
    }

    /* Wait for last byte to shift out */
    while (!(UART_REG(base, UART_REG_SCIFLR) & SCIFLR_TX_EMPTY)) {
        /* spin */
    }

    return MMWAVE_OK;
}

int32 hal_uart_receive(uint32 instance, uint8* data, uint32 max_len, uint32* actual)
{
    if (instance >= HAL_UART_MAX_INSTANCES || !g_uart[instance].initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (data == NULL || actual == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    uart_instance_t* uart = &g_uart[instance];
    uint32 count = 0U;

    while (count < max_len) {
        uint8 byte;
        if (!ring_buf_get(&uart->rx_ring, &byte)) {
            break;  /* No more data in ring buffer */
        }
        data[count] = byte;
        count++;
    }

    *actual = count;
    return MMWAVE_OK;
}

int32 hal_uart_register_rx_callback(uint32 instance, hal_uart_rx_callback_t callback)
{
    if (instance >= HAL_UART_MAX_INSTANCES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    g_uart[instance].rx_callback = callback;
    return MMWAVE_OK;
}

int32 hal_uart_puts(uint32 instance, const char* str)
{
    if (str == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    uint32 len = 0U;
    while (str[len] != '\0') {
        len++;
    }
    return hal_uart_send(instance, (const uint8*)str, len);
}

int32 hal_uart_deinit(uint32 instance)
{
    if (instance >= HAL_UART_MAX_INSTANCES) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    if (g_uart[instance].initialized) {
        uint32 base = g_uart[instance].base_addr;

        /* Disable interrupts */
        UART_REG(base, UART_REG_SCICLRINT) = 0xFFFFFFFFU;

        hal_interrupt_disable(uart_irq_nums[instance]);
        hal_interrupt_unregister(uart_irq_nums[instance]);

        /* Put module in reset */
        UART_REG(base, UART_REG_SCIGCR0) = 0U;

        memset(&g_uart[instance], 0, sizeof(uart_instance_t));
    }

    return MMWAVE_OK;
}

/* ---- ISR handler ---- */

static void uart_rx_isr(void* arg)
{
    uint32 instance = (uint32)(uintptr_t)arg;
    if (instance >= HAL_UART_MAX_INSTANCES) {
        return;
    }

    uart_instance_t* uart = &g_uart[instance];
    uint32 base = uart->base_addr;

    /* Read all available bytes */
    while (UART_REG(base, UART_REG_SCIFLR) & SCIFLR_RXRDY) {
        uint8 byte = (uint8)(UART_REG(base, UART_REG_SCIRD) & 0xFFU);

        ring_buf_put(&uart->rx_ring, byte);

        if (uart->rx_callback != NULL) {
            uart->rx_callback(instance, byte);
        }
    }
}
