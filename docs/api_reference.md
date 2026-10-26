# mmWave Radar SDK - API Reference

All public functions are declared in `sdk/app_api/mmwave_api.h` and `sdk/app_api/data_acquisition_api.h`. Return values are `int32` where `MMWAVE_OK` (0) indicates success and negative values are error codes defined in `mmwave_error_codes.h`.

---

## Lifecycle Functions

### `mmwave_init`

```c
int32 mmwave_init(mmwave_handle_t* handle, const mmwave_config_t* config);
```

| Direction | Parameter | Description |
|-----------|-----------|-------------|
| [out] | `handle` | Pointer that receives the SDK handle on success |
| [in] | `config` | Configuration struct; pass `NULL` for defaults |

**Returns:** `MMWAVE_OK`, `MMWAVE_ERR_NULL_POINTER`, `MMWAVE_ERR_ALREADY_INITIALIZED`, or a `MMWAVE_ERR_CFG_*` code.

**Example:**
```c
mmwave_handle_t h;
int32 ret = mmwave_init(&h, NULL);  /* use default config */
```

---

### `mmwave_open`

```c
int32 mmwave_open(mmwave_handle_t handle);
```

| Direction | Parameter | Description |
|-----------|-----------|-------------|
| [in] | `handle` | SDK handle from `mmwave_init` |

**Returns:** `MMWAVE_OK`, `MMWAVE_ERR_NOT_INITIALIZED`, `MMWAVE_ERR_ALREADY_OPEN`, or `MMWAVE_ERR_HW_INIT_FAILED`.

---

### `mmwave_close`

```c
int32 mmwave_close(mmwave_handle_t handle);
```

Stops the sensor (if running) and releases hardware resources.

**Returns:** `MMWAVE_OK` or `MMWAVE_ERR_NOT_OPEN`.

---

### `mmwave_deinit`

```c
int32 mmwave_deinit(mmwave_handle_t handle);
```

Closes (if open) and frees all SDK resources. The handle is invalid after this call.

**Returns:** `MMWAVE_OK` or `MMWAVE_ERR_NOT_INITIALIZED`.

---

## Configuration Functions

### `mmwave_config_chirp`

```c
int32 mmwave_config_chirp(mmwave_handle_t handle, const mmwave_chirp_profile_t* profile);
```

| Direction | Parameter | Description |
|-----------|-----------|-------------|
| [in] | `handle` | SDK handle |
| [in] | `profile` | Chirp profile (frequency, slope, samples, etc.) |

Validates bandwidth against platform limit (`MMWAVE_MAX_BANDWIDTH_GHZ`). ADC samples must be in `[1, 1024]`.

**Returns:** `MMWAVE_OK`, `MMWAVE_ERR_CFG_BANDWIDTH_EXCEED`, `MMWAVE_ERR_CFG_INVALID_SAMPLES`.

---

### `mmwave_config_frame`

```c
int32 mmwave_config_frame(mmwave_handle_t handle, const mmwave_frame_config_t* config);
```

| Direction | Parameter | Description |
|-----------|-----------|-------------|
| [in] | `handle` | SDK handle |
| [in] | `config` | Frame parameters (chirps per frame, periodicity, channel masks) |

`chirps_per_frame` must be in `[1, MMWAVE_MAX_CHIRPS_PER_FRAME]`.

**Returns:** `MMWAVE_OK` or `MMWAVE_ERR_CFG_INVALID_SAMPLES`.

---

### `mmwave_calibrate`

```c
int32 mmwave_calibrate(mmwave_handle_t handle, const mmwave_cal_config_t* config);
```

Runs TX power, RX gain/phase, and LO leakage calibrations as enabled in `config`. Pass `NULL` to use the calibration settings from `mmwave_init`.

**Returns:** `MMWAVE_OK` or `MMWAVE_ERR_CAL_*` codes.

---

## Runtime Functions

### `mmwave_start`

```c
int32 mmwave_start(mmwave_handle_t handle);
```

Starts the RF front-end and frame scheduler. Automatically runs calibration if enabled and not yet performed.

**Returns:** `MMWAVE_OK`, `MMWAVE_ERR_NOT_OPEN`, `MMWAVE_ERR_ALREADY_RUNNING`.

---

### `mmwave_stop`

```c
int32 mmwave_stop(mmwave_handle_t handle);
```

**Returns:** `MMWAVE_OK` or `MMWAVE_ERR_NOT_RUNNING`.

---

### `mmwave_get_frame`

```c
int32 mmwave_get_frame(mmwave_handle_t handle, mmwave_frame_t* frame, uint32 timeout_ms);
```

| Direction | Parameter | Description |
|-----------|-----------|-------------|
| [in] | `handle` | SDK handle |
| [out] | `frame` | Populated with frame metadata and ADC data pointer |
| [in] | `timeout_ms` | Timeout; use `MMWAVE_WAIT_FOREVER` for blocking |

**Returns:** `MMWAVE_OK`, `MMWAVE_ERR_TIMEOUT`, `MMWAVE_ERR_NOT_RUNNING`.

**Example:**
```c
mmwave_frame_t frame;
if (mmwave_get_frame(h, &frame, 1000) == MMWAVE_OK) {
    process(frame.adc_data, frame.adc_data_size);
}
```

---

## Callback Registration

### `mmwave_register_frame_callback`

```c
int32 mmwave_register_frame_callback(mmwave_handle_t handle,
                                      mmwave_frame_callback_t callback,
                                      void* user_data);
```

Called from ISR context when a new frame is available.

---

### `mmwave_register_error_callback`

```c
int32 mmwave_register_error_callback(mmwave_handle_t handle,
                                      mmwave_error_callback_t callback,
                                      void* user_data);
```

Called when an asynchronous hardware error occurs (chirp error, RF fault).

---

## Status

### `mmwave_get_status`

```c
int32 mmwave_get_status(mmwave_handle_t handle, mmwave_status_t* status);
```

Returns current state flags, temperature, frame count, and error count.

---

## Data Types

### `mmwave_chirp_profile_t`

| Field | Type | Description |
|-------|------|-------------|
| `profile_id` | `uint32` | 0-3 |
| `start_freq_ghz` | `float32` | 76.0 - 81.0 |
| `freq_slope_mhz_us` | `float32` | Chirp slope |
| `idle_time_us` | `float32` | Inter-chirp idle |
| `adc_start_time_us` | `float32` | ADC start offset |
| `ramp_end_time_us` | `float32` | Ramp duration |
| `adc_samples` | `uint16` | 1 - 1024 |
| `sample_rate_ksps` | `uint16` | ADC rate |
| `rx_gain_db` | `uint8` | RX gain |
| `tx_power_backoff_db` | `uint8` | TX backoff |

### `mmwave_frame_config_t`

| Field | Type | Description |
|-------|------|-------------|
| `chirps_per_frame` | `uint16` | 1 - 256 |
| `num_frames` | `uint16` | 0 = infinite |
| `frame_periodicity_ms` | `float32` | Frame period |
| `tx_channel_mask` | `uint8` | TX enable bitmask |
| `rx_channel_mask` | `uint8` | RX enable bitmask |
| `profile_id` | `uint8` | Active profile |

### `mmwave_frame_t`

| Field | Type | Description |
|-------|------|-------------|
| `frame_number` | `uint32` | Monotonic counter |
| `timestamp_us` | `uint32` | Frame start time |
| `num_chirps` | `uint16` | Chirps in frame |
| `num_rx_channels` | `uint16` | Active RX count |
| `num_adc_samples` | `uint16` | Samples per chirp |
| `adc_data` | `int16*` | Data buffer pointer |
| `adc_data_size` | `uint32` | Payload size (bytes) |
| `crc` | `uint32` | End-to-end CRC |
