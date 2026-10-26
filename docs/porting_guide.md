# mmWave Radar SDK - Porting Guide

This guide describes how to port the mmWave Radar SDK to a new hardware platform or RTOS.

---

## 1. Overview

The SDK is organised in three layers:

```
Application API  (platform-independent)
    |
Middleware       (config mgr, calibration, frame scheduler)
    |
HAL              (platform-specific hardware drivers)
```

To port the SDK you need to:

1. Implement the HAL interfaces for your platform.
2. Provide a linker script with the required memory sections.
3. Write a thin Board Support Package (BSP) for clock and pin-mux.
4. Validate with the unit and integration test suites.

---

## 2. HAL Implementation

Each HAL header in `sdk/hal/` declares a small set of functions. Create a new source file for your platform (e.g. `hal/platform_myboard/`) and implement every function.

### 2.1 SPI (`hal_spi.h`)

```c
int32 hal_spi_init(uint32 instance, uint32 speed);
int32 hal_spi_transfer(uint32 instance, const uint8* tx, uint8* rx, uint32 len);
```

- CPOL = 0, CPHA = 0 (mode 0) by default.
- CS must be asserted/deasserted by the implementation.
- Target throughput: >= 10 MHz for radar register access.

### 2.2 UART (`hal_uart.h`)

```c
int32 hal_uart_init(uint32 instance, uint32 baud);
int32 hal_uart_write(uint32 instance, const uint8* data, uint32 len);
int32 hal_uart_read(uint32 instance, uint8* data, uint32 len, uint32 timeout_ms);
```

### 2.3 GPIO (`hal_gpio.h`)

```c
int32 hal_gpio_init(void);
int32 hal_gpio_set(uint32 pin, bool value);
bool  hal_gpio_get(uint32 pin);
```

### 2.4 DMA (`hal_dma.h`)

```c
int32 hal_dma_init(void);
int32 hal_dma_configure(uint32 channel, const hal_dma_config_t* cfg);
int32 hal_dma_start(uint32 channel);
int32 hal_dma_wait(uint32 channel, uint32 timeout_ms);
```

### 2.5 Interrupts (`hal_interrupt.h`)

```c
int32 hal_interrupt_init(void);
int32 hal_interrupt_register(uint32 irq, hal_isr_t handler, void* arg);
int32 hal_interrupt_unregister(uint32 irq);
int32 hal_interrupt_enable(uint32 irq);
int32 hal_interrupt_disable(uint32 irq);
```

---

## 3. Linker Script

The SDK requires the following memory sections:

| Section | Usage | Typical Size |
|---------|-------|-------------|
| `.text` | Code | 128 KB |
| `.data` / `.bss` | Global variables | 16 KB |
| `.l3ram` | ADC ping-pong buffers | 128 KB |
| `.hwi_stack` | ISR stack | 4 KB |

Ensure the ADC buffer section is placed in a region accessible by both the CPU and the DMA controller.

---

## 4. Board Support Package (BSP)

Create `bsp_myboard.c` with:

```c
void bsp_clock_init(void);     /* PLL, peripheral clocks */
void bsp_pinmux_init(void);    /* Pin multiplexing for SPI, UART, GPIO */
void bsp_power_init(void);     /* Regulator enables, power sequencing */
```

Call these from your `main()` before `mmwave_init()`.

---

## 5. RTOS Adaptation

The default SDK build is bare-metal. For RTOS support:

- Replace busy-wait loops in the HAL with semaphore-pend calls.
- Protect shared data with mutexes (e.g. the global SDK context).
- Provide `hal_time_get_us()` using the RTOS tick or a hardware timer.

---

## 6. Testing Your Port

```bash
# Build tests for your platform
make PLATFORM=myboard test

# Run on target via JTAG
openocd -f myboard.cfg -c "program build/test_mmwave_api.out verify reset exit"
```

1. All unit tests in `tests/unit/` must pass.
2. The integration test `test_sdk_full_flow` must complete the full lifecycle.
3. Measure frame-to-frame latency with `test_latency_benchmark` to ensure real-time performance.

---

## 7. Checklist

- [ ] All 5 HAL modules implemented and compiling
- [ ] Linker script places ADC buffers in DMA-accessible RAM
- [ ] BSP initialises clocks and pin-mux before SDK init
- [ ] Unit tests pass on host (mocked HAL) and target (real HAL)
- [ ] Integration test completes Init -> Start -> GetFrame -> Stop -> Deinit
- [ ] Latency benchmark meets real-time budget (< 40 ms per frame)
