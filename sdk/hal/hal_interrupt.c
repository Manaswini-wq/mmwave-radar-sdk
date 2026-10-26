/**
 * @file hal_interrupt.c
 * @brief Interrupt Controller HAL implementation for mmWave Radar SDK
 * @details VIM (Vectored Interrupt Manager) driver for the ARM R5F cores
 *          on TI AWR/IWR mmWave SoCs. Manages vector table registration,
 *          priority assignment, and global interrupt enable/disable.
 */
#include "hal_interrupt.h"
#include "mmwave_error_codes.h"

#include <string.h>

/* ---- VIM Register Map ---- */
#define VIM_BASE_ADDR               0xFFFFFD00U

#define VIM_REG_IRQINDEX            0x00U   /**< IRQ index register           */
#define VIM_REG_FIQINDEX            0x04U   /**< FIQ index register           */
#define VIM_REG_FIRQPR0             0x10U   /**< FIQ/IRQ program reg (0-31)   */
#define VIM_REG_FIRQPR1             0x14U   /**< FIQ/IRQ program reg (32-63)  */
#define VIM_REG_REQENASET0          0x30U   /**< Request enable set (0-31)    */
#define VIM_REG_REQENASET1          0x34U   /**< Request enable set (32-63)   */
#define VIM_REG_REQENACLR0          0x40U   /**< Request enable clear (0-31)  */
#define VIM_REG_REQENACLR1          0x44U   /**< Request enable clear (32-63) */
#define VIM_REG_CHANCTRL_BASE       0x80U   /**< Channel mapping base         */

/* Priority registers: 4 channels per 32-bit word */
#define VIM_REG_CHANPRI_BASE        0xD00U  /**< Channel priority base        */

#define VIM_REG(off)  (*(volatile uint32*)(VIM_BASE_ADDR + (off)))

/* ---- ISR vector table (software) ---- */
typedef struct {
    hal_isr_handler_t   handler;
    void*               arg;
    bool                registered;
    bool                enabled;
    uint32              priority;
} irq_entry_t;

static irq_entry_t  g_irq_table[HAL_IRQ_MAX_VECTORS];
static bool          g_int_initialized = false;

/* ---- Public API ---- */

int32 hal_interrupt_init(void)
{
    if (g_int_initialized) {
        return MMWAVE_ERR_ALREADY_INITIALIZED;
    }

    memset(g_irq_table, 0, sizeof(g_irq_table));

    /* Disable all interrupt channels */
    VIM_REG(VIM_REG_REQENACLR0) = 0xFFFFFFFFU;
    VIM_REG(VIM_REG_REQENACLR1) = 0xFFFFFFFFU;

    /* Map all channels to IRQ (not FIQ) by default */
    VIM_REG(VIM_REG_FIRQPR0) = 0x00000000U;
    VIM_REG(VIM_REG_FIRQPR1) = 0x00000000U;

    /* Program default 1:1 channel mapping (channel N -> VIM slot N) */
    for (uint32 i = 0U; i < HAL_IRQ_MAX_VECTORS; i += 4U) {
        uint32 offset = VIM_REG_CHANCTRL_BASE + (i / 4U) * 4U;
        VIM_REG(offset) = ((i + 0U) << 0)  |
                          ((i + 1U) << 8)  |
                          ((i + 2U) << 16) |
                          ((i + 3U) << 24);
    }

    g_int_initialized = true;
    return MMWAVE_OK;
}

int32 hal_interrupt_register(uint32 irq_num, hal_isr_handler_t handler, void* arg)
{
    if (!g_int_initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (irq_num >= HAL_IRQ_MAX_VECTORS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (handler == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    g_irq_table[irq_num].handler    = handler;
    g_irq_table[irq_num].arg        = arg;
    g_irq_table[irq_num].registered = true;

    return MMWAVE_OK;
}

int32 hal_interrupt_unregister(uint32 irq_num)
{
    if (!g_int_initialized || irq_num >= HAL_IRQ_MAX_VECTORS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    /* Disable the interrupt first */
    hal_interrupt_disable(irq_num);

    g_irq_table[irq_num].handler    = NULL;
    g_irq_table[irq_num].arg        = NULL;
    g_irq_table[irq_num].registered = false;
    g_irq_table[irq_num].enabled    = false;

    return MMWAVE_OK;
}

int32 hal_interrupt_enable(uint32 irq_num)
{
    if (!g_int_initialized || irq_num >= HAL_IRQ_MAX_VECTORS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    if (irq_num < 32U) {
        VIM_REG(VIM_REG_REQENASET0) = (1U << irq_num);
    } else {
        VIM_REG(VIM_REG_REQENASET1) = (1U << (irq_num - 32U));
    }

    g_irq_table[irq_num].enabled = true;
    return MMWAVE_OK;
}

int32 hal_interrupt_disable(uint32 irq_num)
{
    if (!g_int_initialized || irq_num >= HAL_IRQ_MAX_VECTORS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    if (irq_num < 32U) {
        VIM_REG(VIM_REG_REQENACLR0) = (1U << irq_num);
    } else {
        VIM_REG(VIM_REG_REQENACLR1) = (1U << (irq_num - 32U));
    }

    g_irq_table[irq_num].enabled = false;
    return MMWAVE_OK;
}

int32 hal_interrupt_set_priority(uint32 irq_num, uint32 priority)
{
    if (!g_int_initialized || irq_num >= HAL_IRQ_MAX_VECTORS) {
        return MMWAVE_ERR_INVALID_PARAM;
    }
    if (priority > HAL_IRQ_PRIORITY_LOW) {
        return MMWAVE_ERR_INVALID_PARAM;
    }

    g_irq_table[irq_num].priority = priority;

    /* Program the VIM priority register (4 channels per word, 8 bits each) */
    uint32 word_offset = VIM_REG_CHANPRI_BASE + (irq_num / 4U) * 4U;
    uint32 bit_offset  = (irq_num % 4U) * 8U;
    uint32 reg_val = VIM_REG(word_offset);

    reg_val &= ~(0xFFU << bit_offset);
    reg_val |= (priority << bit_offset);

    VIM_REG(word_offset) = reg_val;

    return MMWAVE_OK;
}

uint32 hal_interrupt_disable_global(void)
{
    /*
     * On ARM R5F: read CPSR, set I-bit to disable IRQ.
     * This is a simplified portable implementation using a global flag.
     */
    uint32 prev_state = g_int_initialized ? 1U : 0U;

    /* In real firmware this would be:
     * __asm volatile("MRS %0, CPSR" : "=r"(prev_state));
     * __asm volatile("CPSID i");
     */

    VIM_REG(VIM_REG_REQENACLR0) = 0xFFFFFFFFU;
    VIM_REG(VIM_REG_REQENACLR1) = 0xFFFFFFFFU;

    return prev_state;
}

void hal_interrupt_restore_global(uint32 prev_state)
{
    (void)prev_state;

    /*
     * In real firmware this would be:
     * __asm volatile("MSR CPSR_c, %0" :: "r"(prev_state));
     */

    /* Re-enable only those interrupts that were individually enabled */
    for (uint32 i = 0U; i < HAL_IRQ_MAX_VECTORS; i++) {
        if (g_irq_table[i].enabled) {
            if (i < 32U) {
                VIM_REG(VIM_REG_REQENASET0) |= (1U << i);
            } else {
                VIM_REG(VIM_REG_REQENASET1) |= (1U << (i - 32U));
            }
        }
    }
}

int32 hal_interrupt_deinit(void)
{
    if (!g_int_initialized) {
        return MMWAVE_OK;
    }

    /* Disable all interrupts */
    VIM_REG(VIM_REG_REQENACLR0) = 0xFFFFFFFFU;
    VIM_REG(VIM_REG_REQENACLR1) = 0xFFFFFFFFU;

    memset(g_irq_table, 0, sizeof(g_irq_table));
    g_int_initialized = false;

    return MMWAVE_OK;
}
