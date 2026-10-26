/**
 * @file frame_scheduler.c
 * @brief Frame Scheduler implementation for mmWave Radar SDK
 * @details Timer-based frame triggering with chirp sequence management,
 *          inter-frame gap handling, and frame count tracking.
 *          Uses a hardware timer to generate periodic frame start events.
 */
#include "frame_scheduler.h"
#include "hal_interrupt.h"
#include "mmwave_error_codes.h"
#include "mmwave_config.h"

#include <string.h>

/* ---- Timer Register Map ---- */
#define TIMER0_BASE_ADDR            0xFFF7E800U
#define TIMER1_BASE_ADDR            0xFFF7E900U

#define TIMER_REG_GCTRL             0x00U   /**< Global control           */
#define TIMER_REG_CNT               0x04U   /**< Counter value            */
#define TIMER_REG_PRD               0x08U   /**< Period register          */
#define TIMER_REG_INTCTRL           0x0CU   /**< Interrupt control        */
#define TIMER_REG_INTFLG            0x10U   /**< Interrupt flag           */

#define TIMER_GCTRL_ENABLE          (1U << 0)
#define TIMER_GCTRL_ONESHOT         (1U << 1)
#define TIMER_GCTRL_RESET           (1U << 15)
#define TIMER_INTCTRL_PRD_INT_ENA   (1U << 0)

#define TIMER_REG(base, off)   (*(volatile uint32*)((base) + (off)))

/* Timer clock: assume 200 MHz VCLK with /1 prescale */
#define TIMER_CLK_HZ                200000000U

/* Radar front-end control registers */
#define RADAR_REG_FRAME_TRIGGER     0x0400U
#define RADAR_REG_CHIRP_STATUS      0x0404U
#define RADAR_FRAME_TRIG_START      0x00000001U

/* ---- Internal context ---- */
typedef struct {
    bool                    initialized;
    uint32                  state;
    mmwave_frame_config_t   config;
    uint32                  frames_completed;
    uint32                  chirps_in_current_frame;
    uint32                  frames_remaining;
    uint32                  timer_base;
    uint32                  frame_start_timestamp;
} frame_sched_ctx_t;

static frame_sched_ctx_t g_fsched;

/* ---- SPI helper for radar trigger (simplified) ---- */

static int32 radar_spi_write(uint16 reg_addr, uint32 value)
{
    /* Inline the SPI write for frame trigger - same pattern as config mgr */
    uint8 tx_buf[6];
    tx_buf[0] = 0x80U | (uint8)((reg_addr >> 8) & 0x7FU);
    tx_buf[1] = (uint8)(reg_addr & 0xFFU);
    tx_buf[2] = (uint8)((value >> 24) & 0xFFU);
    tx_buf[3] = (uint8)((value >> 16) & 0xFFU);
    tx_buf[4] = (uint8)((value >> 8) & 0xFFU);
    tx_buf[5] = (uint8)(value & 0xFFU);

    /* Use direct SPI calls from hal_spi */
    extern int32 hal_spi_chip_select(uint32 instance, uint32 cs_num, bool active);
    extern int32 hal_spi_transfer(uint32 instance, const uint8* tx_buf,
                                  uint8* rx_buf, uint32 len);

    hal_spi_chip_select(0U, 0U, true);
    int32 ret = hal_spi_transfer(0U, tx_buf, NULL, 6U);
    hal_spi_chip_select(0U, 0U, false);
    return ret;
}

/* ---- Timer ISR: triggers a new frame ---- */

static void frame_timer_isr(void* arg)
{
    frame_sched_ctx_t* ctx = (frame_sched_ctx_t*)arg;

    if (ctx->state != FRAME_SCHED_RUNNING) {
        return;
    }

    /* Clear timer interrupt flag */
    TIMER_REG(ctx->timer_base, TIMER_REG_INTFLG) = 1U;

    /* Record frame start timestamp from timer counter */
    ctx->frame_start_timestamp = TIMER_REG(ctx->timer_base, TIMER_REG_CNT);

    /* Reset chirp counter for new frame */
    ctx->chirps_in_current_frame = 0U;

    /* Trigger the radar front-end to start a new frame */
    radar_spi_write(RADAR_REG_FRAME_TRIGGER, RADAR_FRAME_TRIG_START);

    /* Update remaining count if not infinite */
    if (ctx->config.num_frames != 0U) {
        if (ctx->frames_remaining > 0U) {
            ctx->frames_remaining--;
        }

        /* Stop if all frames completed */
        if (ctx->frames_remaining == 0U) {
            /* Disable the timer */
            TIMER_REG(ctx->timer_base, TIMER_REG_GCTRL) &= ~TIMER_GCTRL_ENABLE;
            ctx->state = FRAME_SCHED_IDLE;
        }
    }

    ctx->frames_completed++;
}

/* ---- Public API ---- */

int32 frame_scheduler_init(frame_sched_handle_t* handle)
{
    if (handle == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    memset(&g_fsched, 0, sizeof(frame_sched_ctx_t));
    g_fsched.timer_base = TIMER0_BASE_ADDR;
    g_fsched.state = FRAME_SCHED_IDLE;

    /* Reset the timer */
    TIMER_REG(g_fsched.timer_base, TIMER_REG_GCTRL) = TIMER_GCTRL_RESET;
    volatile uint32 delay = 10U;
    while (delay-- > 0U) { /* spin */ }
    TIMER_REG(g_fsched.timer_base, TIMER_REG_GCTRL) = 0U;

    /* Register timer ISR */
    hal_interrupt_register(IRQ_TIMER0, frame_timer_isr, &g_fsched);

    g_fsched.initialized = true;
    *handle = (frame_sched_handle_t)&g_fsched;

    return MMWAVE_OK;
}

int32 frame_scheduler_start(frame_sched_handle_t handle,
                            const mmwave_frame_config_t* config)
{
    frame_sched_ctx_t* ctx = (frame_sched_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (config == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }
    if (ctx->state == FRAME_SCHED_RUNNING) {
        return MMWAVE_ERR_ALREADY_RUNNING;
    }

    /* Store configuration */
    memcpy(&ctx->config, config, sizeof(mmwave_frame_config_t));
    ctx->frames_completed       = 0U;
    ctx->chirps_in_current_frame = 0U;
    ctx->frames_remaining       = config->num_frames;  /* 0 = infinite */

    /*
     * Compute timer period from frame periodicity.
     * period_ticks = frame_periodicity_ms * 1e-3 * TIMER_CLK_HZ
     */
    float32 period_sec = config->frame_periodicity_ms / 1000.0f;
    uint32 period_ticks = (uint32)(period_sec * (float32)TIMER_CLK_HZ);

    if (period_ticks == 0U) {
        period_ticks = 1U;  /* Minimum period */
    }

    /* Configure the timer */
    uint32 timer_base = ctx->timer_base;

    TIMER_REG(timer_base, TIMER_REG_GCTRL) = 0U;           /* Stop timer */
    TIMER_REG(timer_base, TIMER_REG_CNT)   = 0U;           /* Clear counter */
    TIMER_REG(timer_base, TIMER_REG_PRD)   = period_ticks;  /* Set period */

    /* Enable period interrupt */
    TIMER_REG(timer_base, TIMER_REG_INTCTRL) = TIMER_INTCTRL_PRD_INT_ENA;
    TIMER_REG(timer_base, TIMER_REG_INTFLG)  = 1U;  /* Clear pending */

    hal_interrupt_enable(IRQ_TIMER0);

    /* Start the timer in continuous mode */
    TIMER_REG(timer_base, TIMER_REG_GCTRL) = TIMER_GCTRL_ENABLE;

    ctx->state = FRAME_SCHED_RUNNING;

    /* Trigger the first frame immediately */
    frame_timer_isr(ctx);

    return MMWAVE_OK;
}

int32 frame_scheduler_stop(frame_sched_handle_t handle)
{
    frame_sched_ctx_t* ctx = (frame_sched_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }

    /* Disable timer */
    TIMER_REG(ctx->timer_base, TIMER_REG_GCTRL) = 0U;
    TIMER_REG(ctx->timer_base, TIMER_REG_INTCTRL) = 0U;

    hal_interrupt_disable(IRQ_TIMER0);

    ctx->state = FRAME_SCHED_IDLE;

    return MMWAVE_OK;
}

int32 frame_scheduler_get_stats(frame_sched_handle_t handle,
                                frame_sched_stats_t* stats)
{
    frame_sched_ctx_t* ctx = (frame_sched_ctx_t*)handle;

    if (ctx == NULL || !ctx->initialized) {
        return MMWAVE_ERR_NOT_INITIALIZED;
    }
    if (stats == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    stats->state                  = ctx->state;
    stats->frames_completed       = ctx->frames_completed;
    stats->chirps_in_current_frame = ctx->chirps_in_current_frame;
    stats->frames_remaining       = ctx->frames_remaining;
    stats->frame_start_timestamp  = ctx->frame_start_timestamp;

    return MMWAVE_OK;
}

int32 frame_scheduler_deinit(frame_sched_handle_t handle)
{
    frame_sched_ctx_t* ctx = (frame_sched_ctx_t*)handle;

    if (ctx == NULL) {
        return MMWAVE_ERR_NULL_POINTER;
    }

    if (ctx->initialized) {
        /* Ensure timer is stopped */
        if (ctx->state == FRAME_SCHED_RUNNING) {
            frame_scheduler_stop(handle);
        }

        hal_interrupt_unregister(IRQ_TIMER0);

        /* Reset the timer hardware */
        TIMER_REG(ctx->timer_base, TIMER_REG_GCTRL) = TIMER_GCTRL_RESET;

        memset(ctx, 0, sizeof(frame_sched_ctx_t));
    }

    return MMWAVE_OK;
}
