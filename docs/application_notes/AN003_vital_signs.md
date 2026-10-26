# AN003 - Vital Signs Monitoring with mmWave Radar

## 1. Introduction

This application note describes contactless vital signs monitoring (breathing rate and heart rate) using the mmWave Radar SDK. The radar measures sub-millimetre chest-wall displacement at ranges up to 2 m.

## 2. System Overview

```
Radar  -->  Range FFT  -->  Phase Extraction  -->  Bandpass Filter
    -->  Breathing Rate (0.1-0.5 Hz)
    -->  Heart Rate (0.8-2.0 Hz)
```

## 3. Recommended Configuration

| Parameter | Value |
|-----------|-------|
| Start frequency | 77 GHz |
| Bandwidth | 3.5 GHz |
| Chirps per frame | 64 |
| Frame rate | 20 fps |
| ADC samples | 256 |
| Max range | 2.0 m |

```c
mmwave_config_t cfg = MMWAVE_DEFAULT_CONFIG;
cfg.chirp_profile.freq_slope_mhz_us = 87.5f;
cfg.chirp_profile.adc_samples       = 256;
cfg.frame_config.chirps_per_frame   = 64;
cfg.frame_config.frame_periodicity_ms = 50.0f;
cfg.frame_config.tx_channel_mask    = 0x01;   /* single TX for phase coherence */
cfg.frame_config.rx_channel_mask    = 0x01;   /* single RX */
```

## 4. Signal Processing

1. **Range FFT** - Identify the range bin corresponding to the subject's chest.
2. **Phase extraction** - Compute `atan2(Im, Re)` at the target range bin across frames.
3. **Phase unwrapping** - Remove 2-pi discontinuities.
4. **Bandpass filtering** - Separate breathing (0.1-0.5 Hz) and cardiac (0.8-2.0 Hz) components using 4th-order Butterworth IIR filters.
5. **Rate estimation** - Apply FFT or autocorrelation on a 15-second sliding window to extract dominant frequency.

## 5. Displacement Sensitivity

Chest-wall displacement from phase:

```
displacement = (lambda / (4 * pi)) * delta_phase
```

At 77 GHz, lambda = 3.9 mm, giving ~0.31 mm per radian of phase change. Typical breathing displacement is 1-12 mm; heartbeat displacement is 0.1-0.5 mm.

## 6. Performance

| Metric | Typical |
|--------|---------|
| Breathing rate accuracy | +/- 1 bpm |
| Heart rate accuracy | +/- 3 bpm |
| Max subject range | 2 m |
| Update rate | Every 2 s |

## 7. Limitations

- Subject must remain relatively still during measurement.
- Multiple subjects require spatial separation via beamforming.
- Accuracy degrades if subject is moving (walking, exercising).

## 8. References

- TI Application Report SWRA583 - Vital Signs with mmWave
- mmWave Radar SDK API Reference (`docs/api_reference.md`)
