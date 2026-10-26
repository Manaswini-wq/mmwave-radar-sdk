#!/usr/bin/env python3
"""
data_visualizer.py - Radar data visualization tool.

Reads binary radar data files or streams data from a UART port and provides:
  - Range profile plot
  - Range-Doppler map (2-D heatmap)
  - Detection points overlay
  - Frame-by-frame playback

Usage:
    python data_visualizer.py --file capture.bin --mode range
    python data_visualizer.py --port COM4 --baud 921600 --mode range-doppler
"""

import argparse
import struct
import sys
import time

import numpy as np

try:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
except ImportError:
    plt = None

try:
    import serial
except ImportError:
    serial = None

# Frame header magic word (matches SDK binary format)
MAGIC_WORD = b"\x02\x01\x04\x03\x06\x05\x08\x07"


def read_frames_from_file(path: str, num_chirps: int, num_samples: int,
                           num_rx: int):
    """Yield (frame_number, adc_data) tuples from a binary file."""
    frame_size = num_chirps * num_samples * num_rx * 4  # int16 I + int16 Q
    frame_num = 0
    with open(path, "rb") as f:
        while True:
            raw = f.read(frame_size)
            if len(raw) < frame_size:
                break
            samples = np.frombuffer(raw, dtype=np.int16)
            iq = samples[0::2] + 1j * samples[1::2]
            data = iq.reshape((num_chirps, num_rx, num_samples))
            yield frame_num, data
            frame_num += 1


def open_uart_stream(port: str, baud: int):
    """Open a UART serial port and return the connection."""
    if serial is None:
        sys.exit("ERROR: pyserial is required for UART streaming. pip install pyserial")
    return serial.Serial(port, baud, timeout=1)


def read_uart_frame(ser, num_chirps: int, num_samples: int, num_rx: int):
    """Read one frame from the UART stream (blocking)."""
    # Synchronise on magic word
    buf = ser.read(len(MAGIC_WORD))
    while buf != MAGIC_WORD:
        buf = buf[1:] + ser.read(1)
    frame_size = num_chirps * num_samples * num_rx * 4
    raw = ser.read(frame_size)
    samples = np.frombuffer(raw, dtype=np.int16)
    iq = samples[0::2] + 1j * samples[1::2]
    return iq.reshape((num_chirps, num_rx, num_samples))


def plot_range_profile(data_2d: np.ndarray, ax):
    """Plot the range FFT magnitude for the first chirp / first RX."""
    chirp0 = data_2d[0, 0, :]
    spectrum = np.abs(np.fft.fft(chirp0))
    ax.clear()
    ax.plot(spectrum[:len(spectrum) // 2])
    ax.set_xlabel("Range bin")
    ax.set_ylabel("Magnitude")
    ax.set_title("Range Profile")


def plot_range_doppler(data_3d: np.ndarray, ax):
    """Plot the range-Doppler heatmap across all chirps for first RX."""
    rd_map = np.fft.fftshift(
        np.abs(np.fft.fft2(data_3d[:, 0, :])),
        axes=0,
    )
    ax.clear()
    ax.imshow(rd_map, aspect="auto", origin="lower", cmap="jet")
    ax.set_xlabel("Range bin")
    ax.set_ylabel("Doppler bin")
    ax.set_title("Range-Doppler Map")


def run_file_playback(args):
    """Frame-by-frame playback from a binary file."""
    if plt is None:
        sys.exit("ERROR: matplotlib is required.  pip install matplotlib")

    fig, ax = plt.subplots()
    frames = list(read_frames_from_file(args.file, args.chirps,
                                         args.samples, args.rx))
    if not frames:
        sys.exit("No frames found in file.")

    def update(idx):
        _, data = frames[idx % len(frames)]
        if args.mode == "range":
            plot_range_profile(data, ax)
        else:
            plot_range_doppler(data, ax)

    ani = FuncAnimation(fig, update, frames=len(frames), interval=100,
                        repeat=True)
    plt.tight_layout()
    plt.show()


def run_uart_stream(args):
    """Real-time streaming from UART."""
    if plt is None:
        sys.exit("ERROR: matplotlib is required.  pip install matplotlib")

    ser = open_uart_stream(args.port, args.baud)
    fig, ax = plt.subplots()

    def update(_frame_idx):
        data = read_uart_frame(ser, args.chirps, args.samples, args.rx)
        if args.mode == "range":
            plot_range_profile(data[np.newaxis, :, :] if data.ndim == 2
                               else data, ax)
        else:
            plot_range_doppler(data, ax)

    ani = FuncAnimation(fig, update, interval=50)
    plt.tight_layout()
    plt.show()
    ser.close()


def main():
    parser = argparse.ArgumentParser(description="mmWave Radar Data Visualizer")
    parser.add_argument("--file", "-f", help="Binary radar data file")
    parser.add_argument("--port", help="UART serial port (e.g. COM4, /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=921600, help="UART baud rate")
    parser.add_argument("--mode", choices=["range", "range-doppler"],
                        default="range", help="Visualization mode")
    parser.add_argument("--chirps", type=int, default=128, help="Chirps per frame")
    parser.add_argument("--samples", type=int, default=256, help="ADC samples per chirp")
    parser.add_argument("--rx", type=int, default=4, help="Number of RX channels")
    args = parser.parse_args()

    if args.file:
        run_file_playback(args)
    elif args.port:
        run_uart_stream(args)
    else:
        parser.error("Provide either --file or --port")


if __name__ == "__main__":
    main()
