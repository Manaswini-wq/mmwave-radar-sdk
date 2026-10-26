# Firmware-Hardware Interface Specification

This document describes the low-level interfaces between the mmWave Radar SDK firmware and the radar sensor hardware.

---

## 1. SPI Interface

The host processor communicates with the radar front-end ASIC via SPI.

| Parameter | Value |
|-----------|-------|
| Clock speed | 10 MHz (default), up to 25 MHz |
| Mode | CPOL = 0, CPHA = 0 (SPI Mode 0) |
| Word size | 16-bit |
| Byte order | MSB first |
| CS assertion | Active low, must remain asserted for entire transaction |

### CS Timing

```
         ___      _________________________      ___
CS  ____/   \____/                         \____/   \____
              |<-- t_setup -->|       |<-- t_hold -->|
                 >= 50 ns                  >= 50 ns
```

### Register Access Protocol

| Byte | Direction | Content |
|------|-----------|---------|
| 0-1 | MOSI | Address (15-bit) + R/W bit (MSB) |
| 2-3 | MOSI/MISO | Write data / Read data |

---

## 2. UART Debug Console

The SDK outputs diagnostic and trace messages on a dedicated UART port.

| Parameter | Value |
|-----------|-------|
| Baud rate | 921600 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Instance | UART0 |

### Message Format

```
[TIMESTAMP_MS] [LEVEL] MODULE: message\r\n
```

Levels: `DBG`, `INF`, `WRN`, `ERR`, `FTL`

---

## 3. GPIO Assignments

| GPIO Pin | Direction | Function |
|----------|-----------|----------|
| GPIO_0 | Output | Frame start indicator |
| GPIO_1 | Output | Chirp active pulse |
| GPIO_2 | Output | DMA transfer complete |
| GPIO_3 | Input | Radar front-end interrupt (active low) |
| GPIO_4 | Output | Error indicator LED |
| GPIO_5 | Input | External trigger input |

---

## 4. DMA Channel Mapping

| Channel | Source | Destination | Priority | Trigger |
|---------|--------|-------------|----------|---------|
| 0 | ADC output FIFO | L3 RAM ping buffer | High (0) | ADC data ready |
| 1 | L3 RAM | Result buffer | Medium (1) | Software trigger |
| 2 | UART TX FIFO | --- | Low (2) | UART TX empty |

### Ping-Pong Buffer Layout (L3 RAM)

```
0x5100_0000  +------------------+
             |  Ping buffer     |  64 KB
0x5101_0000  +------------------+
             |  Pong buffer     |  64 KB
0x5102_0000  +------------------+
             |  Result scratch  |  32 KB
0x5102_8000  +------------------+
```

---

## 5. Interrupt Priority Table

| Priority | IRQ Name | Source | Handler |
|----------|----------|--------|---------|
| 0 (highest) | `IRQ_FRAME_DONE` | Frame scheduler | `sdk_frame_done_isr` |
| 1 | `IRQ_CHIRP_ERR` | RF front-end | `sdk_error_isr` |
| 2 | `IRQ_DMA_CH0` | DMA channel 0 | `data_acq_dma_isr` |
| 3 | `IRQ_DMA_CH1` | DMA channel 1 | `data_acq_dma_isr` |
| 4 | `IRQ_ADC_DATA_RDY` | ADC | `data_acq_adc_isr` |
| 5 | `IRQ_UART_RX` | UART | `debug_uart_rx_isr` |
| 6 | `IRQ_TEMP_ALERT` | Temperature sensor | `cal_temp_isr` |

All ISRs must complete within 10 us to avoid frame timing violations.

---

## 6. Memory-Mapped Register Access

Radar front-end registers are memory-mapped in the MSS (Master Sub-System) address space.

### Base Addresses

| Peripheral | Base Address | Size |
|------------|-------------|------|
| RF front-end control | `0x0200_0000` | 4 KB |
| ADC buffer control | `0x0210_0000` | 1 KB |
| Chirp sequencer | `0x0220_0000` | 2 KB |
| Calibration engine | `0x0230_0000` | 1 KB |
| DMA controller | `0x0240_0000` | 4 KB |

### Access Patterns

```c
/* Direct 32-bit register write */
#define REG_WRITE32(addr, val)  (*(volatile uint32*)(addr) = (val))
#define REG_READ32(addr)        (*(volatile uint32*)(addr))

/* Read-modify-write with mask */
#define REG_RMW32(addr, mask, val) do {         \
    uint32 tmp = REG_READ32(addr);              \
    tmp = (tmp & ~(mask)) | ((val) & (mask));   \
    REG_WRITE32(addr, tmp);                     \
} while (0)
```

All register accesses must be 32-bit aligned. Use memory barriers (`__DSB()`) after configuration writes that affect DMA or interrupt state.
