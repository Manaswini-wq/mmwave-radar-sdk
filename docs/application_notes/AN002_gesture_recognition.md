# AN002 - Gesture Recognition with mmWave Radar

## 1. Introduction

This application note describes how to implement touchless gesture recognition using the mmWave Radar SDK. The system recognises hand gestures such as swipe left/right, swipe up/down, push, and circular motions at ranges up to 1 m.

## 2. System Overview

```
Radar  -->  Micro-Doppler  -->  Feature Extraction  -->  Classifier  -->  Gesture ID
```

## 3. Recommended Configuration

| Parameter | Value |
|-----------|-------|
| Start frequency | 60 GHz (IWR6843) |
| Bandwidth | 3.5 GHz |
| Chirps per frame | 64 |
| Frame rate | 20 fps |
| ADC samples | 128 |
| Max range | 1.0 m |

```c
mmwave_config_t cfg = MMWAVE_DEFAULT_CONFIG;
cfg.chirp_profile.start_freq_ghz    = 60.0f;
cfg.chirp_profile.freq_slope_mhz_us = 87.5f;
cfg.chirp_profile.adc_samples       = 128;
cfg.frame_config.chirps_per_frame   = 64;
cfg.frame_config.frame_periodicity_ms = 50.0f;
```

## 4. Feature Extraction

From each frame's range-Doppler map, extract:

1. **Doppler centroid** - weighted mean Doppler across detected points.
2. **Doppler spread** - standard deviation of Doppler values.
3. **Range centroid** - mean range of detection cluster.
4. **Energy** - total magnitude in the detection region.
5. **Trajectory** - (x, y) positions over a sliding window of 10 frames.

## 5. Classification

A lightweight decision-tree or SVM classifier trained on the 5-feature vector can run in real time on the R5F core. Training data is collected with the `tools/python/data_visualizer.py` tool.

| Gesture | Feature Signature |
|---------|-------------------|
| Swipe left | Positive Doppler centroid, low range spread |
| Swipe right | Negative Doppler centroid, low range spread |
| Push | Decreasing range centroid, high energy |
| Pull | Increasing range centroid, moderate energy |
| Circle | Oscillating Doppler, circular trajectory |

## 6. Performance

| Metric | Typical |
|--------|---------|
| Recognition accuracy | > 92% |
| Latency | < 200 ms |
| False positive rate | < 3% |

## 7. References

- TI Application Report SWRA582 - Gesture Recognition with mmWave
- mmWave Radar SDK API Reference (`docs/api_reference.md`)
