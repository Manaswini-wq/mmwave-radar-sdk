# mmWave Radar SDK - User Guide

## 1. Overview

The mmWave Radar SDK provides an easy-to-use, scalable interface library for TI's mmWave radar sensors (AWR2944, IWR6843, AWR1843). It abstracts hardware details behind a clean C API so customers can focus on application-level signal processing.

## 2. Prerequisites

| Item | Requirement |
|------|-------------|
| Toolchain | TI ARM CGT v20.x or GCC arm-none-eabi 12+ |
| Host OS | Windows 10+, Ubuntu 20.04+ |
| Hardware | mmWave EVM (e.g. IWR6843ISK) + USB cable |
| Debug probe | XDS110 (on-board) or XDS200 |
| Python (tools) | 3.8+ with pip |

## 3. Installation

```bash
git clone https://github.com/ti/mmwave-radar-sdk.git
cd mmwave-radar-sdk
pip install -r tools/python/requirements.txt
```

## 4. Build

### 4.1 Using Make

```bash
make PLATFORM=iwr6843          # Release build
make PLATFORM=iwr6843 DEBUG=1  # Debug build with symbols
make test                      # Run unit tests on host
```

### 4.2 Using CMake

```bash
cmake -B build -DPLATFORM=IWR6843
cmake --build build
ctest --test-dir build
```

## 5. Project Structure

```
mmwave-radar-sdk/
  sdk/
    app_api/       Top-level API (mmwave_api.h/c)
    include/       Public headers (types, config, error codes)
    hal/           Hardware abstraction layer
  examples/
    basic_radar/   Minimal ranging example
  tests/
    unit/          Unity-based unit tests
    integration/   SDK lifecycle tests
    system/        Application-level tests
  tools/
    python/        Config tool, visualizer, quick start
    perl/          Doc generator, header checker
  docs/            Documentation
```

## 6. Configuration

Sensor configuration can be set programmatically or via a YAML file:

```c
mmwave_config_t cfg = MMWAVE_DEFAULT_CONFIG;
cfg.chirp_profile.start_freq_ghz = 77.0f;
cfg.frame_config.chirps_per_frame = 128;

mmwave_handle_t handle;
mmwave_init(&handle, &cfg);
```

Or generate a C header from YAML:

```bash
python tools/python/sensor_config_tool.py -i my_config.yaml -o mmwave_user_cfg.h
```

## 7. Running the Examples

```bash
cd examples/basic_radar
make PLATFORM=iwr6843
# Flash via UniFlash or the quick-start script:
python tools/python/quick_start.py --port COM4
```

## 8. Debugging

### 8.1 JTAG (XDS110)

1. Connect the XDS110 USB to your host.
2. Open Code Composer Studio (CCS) or OpenOCD.
3. Load the `.out` ELF built with `DEBUG=1`.
4. Set breakpoints in `mmwave_api.c` or ISR handlers.

### 8.2 UART Trace

The SDK outputs diagnostic messages at 921600 baud on UART instance 0. Use any serial terminal or `tools/python/data_visualizer.py --port COMx`.

### 8.3 Logic Analyzer

Monitor the following signals for timing analysis:

| Signal | GPIO | Purpose |
|--------|------|---------|
| Frame start | GPIO_0 | Toggled at frame boundary |
| Chirp active | GPIO_1 | High during RF ramp |
| DMA done | GPIO_2 | Pulse on buffer swap |

## 9. Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `MMWAVE_ERR_PLL_LOCK_FAILED` | Power supply droop | Check 1.0 V rail ripple |
| `MMWAVE_ERR_DATA_CRC_MISMATCH` | EMI on SPI lines | Reduce SPI clock or add shielding |
| `MMWAVE_ERR_TIMEOUT` | Firmware not flashed | Re-flash with UniFlash |
| Frame drops | Processing > frame period | Optimise DSP or reduce chirps |

## 10. Next Steps

- [API Reference](api_reference.md) — full function signatures and examples
- [Porting Guide](porting_guide.md) — adapt the SDK to a new platform
- [Application Notes](application_notes/) — people counting, gestures, vital signs
