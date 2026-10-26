# AN001 - People Counting with mmWave Radar

## 1. Introduction

This application note describes how to implement a real-time people counting system using the mmWave Radar SDK. The solution detects and tracks individuals in an indoor environment using point-cloud clustering.

## 2. System Overview

```
Radar Sensor  -->  Range-FFT  -->  CFAR Detection  -->  Point Cloud
    -->  Clustering (DBSCAN)  -->  Tracking (EKF)  -->  Count Output
```

### Key Parameters

| Parameter | Recommended Value |
|-----------|------------------|
| Start frequency | 77 GHz |
| Bandwidth | 3.5 GHz |
| Chirps per frame | 128 |
| Frame rate | 10 fps |
| TX channels | 3 (MIMO) |
| RX channels | 4 |
| Max range | 6 m |
| Range resolution | ~4.3 cm |

## 3. Configuration

```c
mmwave_config_t cfg = MMWAVE_DEFAULT_CONFIG;
cfg.chirp_profile.freq_slope_mhz_us = 87.5f;
cfg.chirp_profile.ramp_end_time_us  = 40.0f;  /* BW = 3.5 GHz */
cfg.chirp_profile.adc_samples       = 256;
cfg.frame_config.chirps_per_frame   = 128;
cfg.frame_config.frame_periodicity_ms = 100.0f;
cfg.frame_config.tx_channel_mask    = 0x07;    /* 3 TX for azimuth+elevation */
cfg.frame_config.rx_channel_mask    = 0x0F;    /* 4 RX */
```

## 4. Signal Processing Pipeline

1. **Range FFT** - 256-point FFT on each chirp with Hann window.
2. **Doppler FFT** - 128-point FFT across chirps to extract velocity.
3. **CFAR Detection** - CA-CFAR with 8 training cells, 2 guard cells, threshold scale 4.0.
4. **Angle Estimation** - Capon beamforming on the 12 virtual antenna array.
5. **Clustering** - DBSCAN with epsilon = 0.5 m, min_points = 3.
6. **Tracking** - Extended Kalman Filter per cluster with state = [x, y, vx, vy].

## 5. Counting Logic

- Each active track with lifetime > 5 frames counts as one person.
- Tracks with zero velocity for > 30 frames are marked as stationary but still counted.
- Tracks that exit the field of view are removed after a 10-frame grace period.

## 6. Performance

| Metric | Typical |
|--------|---------|
| Accuracy (1-5 people) | > 95% |
| Latency | < 100 ms |
| Max detectable people | 20 |
| Processing load (R5F) | ~40% |

## 7. Example Code

See `examples/people_counting/` for the complete application. Run with:

```bash
make PLATFORM=iwr6843 APP=people_counting
```

## 8. References

- TI Application Report SWRA581 - People Counting with mmWave
- mmWave Radar SDK API Reference (`docs/api_reference.md`)
