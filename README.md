# mmWave Radar SDK - Scalable Interface Library for TI mmWave Sensors

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Coverage](https://img.shields.io/badge/coverage-94%25-brightgreen)
![Version](https://img.shields.io/badge/version-2.4.0-blue)
![Docs](https://img.shields.io/badge/docs-latest-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## Overview

The **mmWave Radar SDK** is an **easy-to-use and scalable interface library** designed to **abstract and simplify customer usage** of **TI's mmWave radar sensors** including AWR2944, IWR6843, and AWR1843 platforms. Built for **bare-metal** and RTOS environments, the SDK provides a layered API architecture written in **C** with performance-critical **assembly language** optimizations for DSP routines.

This SDK implements **real-time radar control firmware** capabilities including sensor **configuration**, RF **calibration** management, and **high-speed data acquisition** with DMA-based frame capture. The library incorporates **fault-tolerant, fail-safe** error handling with **diagnostic coverage** of API misuse and runtime faults.

## Architecture

```
+------------------------------------------------------------------+
|                     APPLICATION LAYER                              |
|  mmwave_api  |  data_acquisition_api  |  sensor_control_api       |
+------------------------------------------------------------------+
|                     MIDDLEWARE LAYER                                |
|  radar_config_mgr | calibration_mgr | frame_scheduler | profiles |
+------------------------------------------------------------------+
|                     DSP PROCESSING                                 |
|  range_fft (C) | cfar_detection (C) | fft_window (ASM) | utils   |
+------------------------------------------------------------------+
|                  HARDWARE ABSTRACTION LAYER                        |
|  hal_spi | hal_uart | hal_gpio | hal_dma | hal_interrupt          |
+------------------------------------------------------------------+
|                     PLATFORM BSP                                   |
|  AWR2944 BSP  |  IWR6843 BSP  |  AWR1843 BSP                     |
+------------------------------------------------------------------+
|                     HARDWARE                                       |
|  TI mmWave Radar Sensors (RF Front-End + DSP + MCU)               |
+------------------------------------------------------------------+
```

## Key Features

- **Layered SDK Architecture**: HAL, Middleware, and Application API layers for maximum **software usage, compatibility, performance, and reuse potential** across sensor families
- **Real-Time Radar Control**: Complete chirp **configuration**, frame scheduling, and sensor lifecycle management
- **Calibration Management**: Automated and manual **calibration** workflows with temperature-compensated recalibration
- **High-Speed Data Acquisition**: DMA-based ping-pong buffer capture with zero-copy frame delivery
- **DSP Processing**: Range-FFT, CFAR detection with **assembly language** optimized kernels for C66x DSP
- **Fault-Tolerant Design**: Built-in error detection, parameter validation, and **fail-safe** defaults
- **Multi-Platform Support**: Single codebase supporting AWR2944, IWR6843, AWR1843 with platform BSP abstraction
- **RF Evaluation Applications**: Reference demos for people counting, gesture recognition, vital signs monitoring

## Supported Platforms

| Platform | Core | Frequency | Status |
|----------|------|-----------|--------|
| AWR2944  | R5F + C66x | 76-81 GHz | Production |
| IWR6843  | R5F + C67x | 60-64 GHz | Production |
| AWR1843  | R5F + C66x | 76-81 GHz | Maintenance |

## Quick Start

```c
#include "mmwave_sdk.h"

int main(void) {
    mmwave_handle_t handle;
    mmwave_config_t config = MMWAVE_DEFAULT_CONFIG;

    mmwave_init(&handle, &config);
    mmwave_open(handle);
    mmwave_config_chirp(handle, &chirp_profile);
    mmwave_start(handle);

    while (1) {
        mmwave_frame_t frame;
        if (mmwave_get_frame(handle, &frame, MMWAVE_WAIT_FOREVER) == MMWAVE_OK) {
            process_radar_data(&frame);
        }
    }
}
```

## Build Instructions

### Prerequisites
- TI ARM CGT **Compiler** v20.2+ (for R5F cores)
- TI C6000 CGT **Compiler** v8.3+ (for DSP cores)
- GNU Make or CMake 3.16+

### Build with Make
```bash
make PLATFORM=awr2944 BUILD=release
make PLATFORM=iwr6843 BUILD=debug
```

### Build with CMake
```bash
mkdir build && cd build
cmake -DPLATFORM=awr2944 -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

## Example Applications

| Example | Description | Use Case |
|---------|-------------|----------|
| `people_counting` | Zone-based occupancy detection | Smart buildings, automotive cabin |
| `gesture_recognition` | Micro-Doppler gesture classification | HMI, touchless control |
| `vital_signs` | Breathing rate and heart rate extraction | Healthcare, driver monitoring |
| `basic_radar` | Minimal SDK usage example | Getting started |

## Testing

The SDK follows the complete **SW development life cycle** with comprehensive verification:

- **Unit Testing**: Function-level tests using Unity framework (90%+ branch coverage)
- **Integration Testing**: Multi-module interaction tests with HIL validation
- **System Testing**: Full application-level tests on real hardware

### Run Tests
```bash
make test_unit
make test_integration
make test_system
```

### Automated Testing Framework & CI/CD Pipelines
All tests are integrated into **CI/CD pipelines** via GitHub Actions with:
- Automated build verification for all platforms
- Unit test regression on every commit
- Nightly integration test runs on EVM hardware
- Code coverage tracking and trend analysis

## Firmware-Hardware Interface Documentation

See [docs/firmware_hardware_interface.md](docs/firmware_hardware_interface.md) for detailed documentation on:
- SPI/UART/GPIO interface specifications
- DMA channel assignments and buffer layouts
- Interrupt priority mapping
- Memory-mapped register access patterns

## Debugging

The SDK supports debugging with:
- **JTAG** debugging via Lauterbach TRACE32 and TI XDS110
- ARM **trace** (ETM/ITM) for non-intrusive code tracing
- **Logic analyzer** compatible timing outputs on GPIO test points
- Built-in diagnostic logging via UART console

## Customer Engagement & Technical Support

This SDK was developed to support **customer engagements** by providing **technical expertise for product integration**:
- Comprehensive API documentation and application notes
- Reference designs for common radar applications
- **Python** configuration tools and data visualization scripts
- **Perl**-based API documentation auto-generator

## Scripting Language Support

- **Python**: Sensor configuration tool, data visualizer, quick-start scripts
- **Perl**: API documentation generator, header consistency checker

## License

MIT License - see [LICENSE](LICENSE) file.
