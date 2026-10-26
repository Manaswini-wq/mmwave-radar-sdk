/**
 * @file hal_gpio.c
 * @brief GPIO HAL implementation for mmWave Radar SDK
 * @details Register-level GPIO driver supporting input/output, toggle, and
 *          edge-triggered interrupts for radar control signals.
 */
#include "hal_gpio.h"
#include "hal_interrupt.h"
#include "mmwave_error_codes.h"

#include <string.h>

/* ---- GPIO Register Map ---- */
#define GPIO_PORTA_BASE             0xFFF7BC00U
#define GPIO_PORTB_BASE             0xFFF7BC40U
#define GPIO_PORTC_BASE             0xFFF7BC80U

#define GPIO_REG_DIR                0x00U   /**< Data direction (0=out, 1=in) */
#define GPIO_REG_DIN                0x04U   /**< Data input register          */
#define GPIO_REG_DOUT               0x08U   /**< Data output register         */
#define GPIO_REG_DSET               0x0CU   /**< Data output set register     */
#define GPIO_REG_DCLR               0x10U   /**< Data output clear register   */
#define GPIO_REG_PDR                0x14U   /**< Open-drain control           */
#define GPIO_REG_PULDIS             0x18U   /**< Pull-up/down disable         */
#define GPIO_REG_INTDET             0x1CU   /**< Interrupt detection (edge)   */
#define GPIO_REG_POL                0x20U   /**< Interrupt polarity            */
#define GPIO_REG_INTENASET          0x24U   /**< Interrupt enable set          */
#define GPIO_REG_INTENACLR          0x28U   /**< Interrupt enable clear        */
#define GPIO_REG_INTFLG             0x2CU   /**< Interrupt flag register       */

#define GPIO_REG(base, off)  (*(volatile uint32*)((base) + (off)))

/* ---- Module state ---- */
typedef struct {
    hal_gpio_isr_callback_t callback;
    void*                   user_data;
} gpio_pin_isr_t;

typedef struct {
    uint32          base_addr;
    gpio_pin_isr_t  pin_isr[32];
} gpio_port_t;

static gpio_port_t  g_gpio_ports[HAL_GPIO_MAX_PORTS];
static bool         g_gpio_initialized = false;

static const uint32 gpio_base_addrs[HAL_GPIO_MAX_PORTS] = {
    GPIO_PORTA_BASE,
    GPIO_PORTB_BASE,
    GPIO_PORTC_BASE
};

static const uint32 gpio_irq_nums[HAL_GPIO_MAX_PORTS] = {
    IRQ_GPIO_PORT_A,
    IRQ_GPIO_PORT_B,
    IRQ_GPIO_PORT_A   /* Port C shares with port A in simplified model */
};

/* Forward declaration */
static void gpio_port_isr(void* arg);

/* ---- Public API ---- */

int32 hal_gpio_init(void)
{
    if (g_gpio_initialized) {
        return MMWAVE_ERR_ALREADY_INITIALIZED;
    }

    memset(g_gpio_ports, 0, sizeof(g_gpio_ports));

    for (uint32 p = 0U; p < HAL_GPIO_MAX_PORTS; p++) {
        g_gpio_ports[p].base_addr = gpio_base_addrs[p];
        uint32 base = g_gpio_ports[p].base_addr;

        /* Default all pins to input */
        GPIO_REG(base, GPIO_REG_DIR) = 0xFFFFFFFFU;

        /* Clear all outputs */
        GPIO_REG(base, GPIO_REG_DCLR) = 0xFFFFFFFFU;

        /* Disable all GPIO interrupts */
        GPIO_REG(base, GPIO_REG_INTENACLR) = 0xFFFFFFFFU;

        /* Clear pending interrupt flags */
        GPIO_REG(base, GPIO_REG_INTFLG) = 0xFFFFFFFFU;

        /* Register the port ISR */
        hal_interrupt_register(gpio_irq_nums[p], gpio_port_isr, (void*)(uintptr_t)p);
    }

    g_gpio_initialized = true;
    return MMWAVE_OK;
}

int32 hal_gpio_set_direction(uint32 port, uint32 pin, uint32 dir)
{
    if (!g_gpio_initialized || port >= HAL_GPIO_MAX_PORTS || pin >= 32U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uint32 base = g_gpio_ports[port].base_addr;
    uint32 reg = GPIO_REG(base, GPIO_REG_DIR);

    if (dir == HAL_GPIO_DIR_OUTPUT) {
        reg &= ~(1U << pin);   /* 0 = output */
    } else {
        reg |= (1U << pin);    /* 1 = input */
    }

    GPIO_REG(base, GPIO_REG_DIR) = reg;
    return MMWAVE_OK;
}

int32 hal_gpio_write(uint32 port, uint32 pin, uint32 value)
{
    if (!g_gpio_initialized || port >= HAL_GPIO_MAX_PORTS || pin >= 32U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uint32 base = g_gpio_ports[port].base_addr;

    if (value == HAL_GPIO_HIGH) {
        GPIO_REG(base, GPIO_REG_DSET) = (1U << pin);
    } else {
        GPIO_REG(base, GPIO_REG_DCLR) = (1U << pin);
    }

    return MMWAVE_OK;
}

int32 hal_gpio_read(uint32 port, uint32 pin, uint32* value)
{
    if (!g_gpio_initialized || port >= HAL_GPIO_MAX_PORTS || pin >= 32U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (value == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    uint32 base = g_gpio_ports[port].base_addr;
    uint32 din = GPIO_REG(base, GPIO_REG_DIN);

    *value = (din >> pin) & 1U;
    return MMWAVE_OK;
}

int32 hal_gpio_toggle(uint32 port, uint32 pin)
{
    if (!g_gpio_initialized || port >= HAL_GPIO_MAX_PORTS || pin >= 32U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    uint32 base = g_gpio_ports[port].base_addr;
    uint32 current = GPIO_REG(base, GPIO_REG_DOUT);

    if (current & (1U << pin)) {
        GPIO_REG(base, GPIO_REG_DCLR) = (1U << pin);
    } else {
        GPIO_REG(base, GPIO_REG_DSET) = (1U << pin);
    }

    return MMWAVE_OK;
}

int32 hal_gpio_config_interrupt(uint32 port, uint32 pin, uint32 edge,
                                hal_gpio_isr_callback_t callback, void* user_data)
{
    if (!g_gpio_initialized || port >= HAL_GPIO_MAX_PORTS || pin >= 32U) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    gpio_port_t* gp = &g_gpio_ports[port];
    uint32 base = gp->base_addr;

    /* Store callback */
    gp->pin_isr[pin].callback  = callback;
    gp->pin_isr[pin].user_data = user_data;

    if (edge == HAL_GPIO_INT_NONE) {
        /* Disable interrupt for this pin */
        GPIO_REG(base, GPIO_REG_INTENACLR) = (1U << pin);
        return MMWAVE_OK;
    }

    /* Configure edge detection */
    uint32 det = GPIO_REG(base, GPIO_REG_INTDET);
    uint32 pol = GPIO_REG(base, GPIO_REG_POL);

    if (edge == HAL_GPIO_INT_RISING) {
        det |= (1U << pin);    /* Edge detect enable */
        pol |= (1U << pin);    /* Rising edge */
    } else if (edge == HAL_GPIO_INT_FALLING) {
        det |= (1U << pin);
        pol &= ~(1U << pin);   /* Falling edge */
    } else { /* HAL_GPIO_INT_BOTH */
        det |= (1U << pin);
        pol |= (1U << pin);    /* Will toggle in ISR for both edges */
    }

    GPIO_REG(base, GPIO_REG_INTDET) = det;
    GPIO_REG(base, GPIO_REG_POL) = pol;

    /* Clear pending flag and enable */
    GPIO_REG(base, GPIO_REG_INTFLG) = (1U << pin);
    GPIO_REG(base, GPIO_REG_INTENASET) = (1U << pin);

    hal_interrupt_enable(gpio_irq_nums[port]);

    return MMWAVE_OK;
}

int32 hal_gpio_deinit(void)
{
    if (!g_gpio_initialized) {
        return MMWAVE_OK;
    }

    for (uint32 p = 0U; p < HAL_GPIO_MAX_PORTS; p++) {
        uint32 base = g_gpio_ports[p].base_addr;

        GPIO_REG(base, GPIO_REG_INTENACLR) = 0xFFFFFFFFU;
        GPIO_REG(base, GPIO_REG_INTFLG) = 0xFFFFFFFFU;

        hal_interrupt_unregister(gpio_irq_nums[p]);
    }

    memset(g_gpio_ports, 0, sizeof(g_gpio_ports));
    g_gpio_initialized = false;
    return MMWAVE_OK;
}

/* ---- ISR handler ---- */

static void gpio_port_isr(void* arg)
{
    uint32 port = (uint32)(uintptr_t)arg;
    if (port >= HAL_GPIO_MAX_PORTS) {
        return;
    }

    gpio_port_t* gp = &g_gpio_ports[port];
    uint32 base = gp->base_addr;
    uint32 flags = GPIO_REG(base, GPIO_REG_INTFLG);

    /* Process each asserted pin interrupt */
    while (flags != 0U) {
        uint32 pin = 0U;
        uint32 tmp = flags;
        /* Find lowest set bit */
        while ((tmp & 1U) == 0U) {
            tmp >>= 1U;
            pin++;
        }

        /* Clear the flag */
        GPIO_REG(base, GPIO_REG_INTFLG) = (1U << pin);

        /* Invoke callback if registered */
        if (gp->pin_isr[pin].callback != NULL) {
            gp->pin_isr[pin].callback(port, pin, gp->pin_isr[pin].user_data);
        }

        flags &= ~(1U << pin);
    }
}
