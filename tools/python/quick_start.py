#!/usr/bin/env python3
"""
quick_start.py - Guided quick-start script for new mmWave Radar SDK customers.

Steps:
  1. Detect a connected EVM via USB/UART.
  2. Flash the default firmware binary.
  3. Configure the sensor for basic ranging (short-range, 30 fps).
  4. Capture and display the first few frames.
  5. Print a summary of what was observed.

Usage:
    python quick_start.py
    python quick_start.py --port COM4 --firmware mmwave_demo.bin
"""

import argparse
import sys
import time

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None

import numpy as np


def detect_evm():
    """Auto-detect an mmWave EVM connected via USB-UART."""
    if serial is None:
        sys.exit("ERROR: pyserial is required.  pip install pyserial")

    print("Scanning for connected mmWave EVM boards...")
    candidates = []
    for port in serial.tools.list_ports.comports():
        desc = (port.description or "").lower()
        if any(kw in desc for kw in ("xds110", "mmwave", "ti", "uart")):
            candidates.append(port.device)

    if not candidates:
        print("No EVM detected automatically.")
        port = input("Enter the serial port manually (e.g. COM4): ").strip()
        return port
    if len(candidates) == 1:
        print(f"Found EVM on {candidates[0]}")
        return candidates[0]

    print("Multiple ports found:")
    for i, p in enumerate(candidates):
        print(f"  [{i}] {p}")
    idx = int(input("Select port index: ").strip())
    return candidates[idx]


def flash_firmware(port: str, firmware_path: str):
    """Flash the default firmware to the EVM over UART boot-loader."""
    print(f"Flashing {firmware_path} to {port} ...")
    # In a real implementation this would use the TI UniFlash protocol.
    # Here we simulate the handshake for demonstration purposes.
    try:
        with serial.Serial(port, 115200, timeout=5) as ser:
            ser.write(b"BOOT\r\n")
            time.sleep(0.5)
            with open(firmware_path, "rb") as fw:
                data = fw.read()
            ser.write(data)
            time.sleep(1.0)
            resp = ser.read(64)
            if b"OK" in resp or b"ACK" in resp:
                print("Firmware flash successful.")
            else:
                print("WARNING: No ACK received; firmware may not have been applied.")
    except Exception as exc:
        print(f"Flash failed: {exc}")
        print("You can flash manually using UniFlash and then re-run this script.")


def configure_basic_ranging(ser):
    """Send a minimal CLI configuration for short-range 30 fps operation."""
    commands = [
        "sensorStop",
        "flushCfg",
        "dfeDataOutputMode 1",
        "channelCfg 15 7 0",
        "adcCfg 2 1",
        "adcbufCfg -1 0 1 1 1",
        "profileCfg 0 77 7 6 40 0 0 70 1 256 10000 0 0 30",
        "chirpCfg 0 0 0 0 0 0 0 1",
        "frameCfg 0 0 128 0 33.33 1 0",
        "sensorStart",
    ]
    for cmd in commands:
        ser.write((cmd + "\n").encode())
        time.sleep(0.05)
    time.sleep(0.5)
    print("Basic ranging configuration applied.")


def capture_frames(ser, num_frames: int = 5):
    """Capture a few frames and print summary statistics."""
    print(f"Capturing {num_frames} frames ...")
    for i in range(num_frames):
        raw = ser.read(256 * 128 * 4)  # approximate frame size
        if len(raw) == 0:
            print(f"  Frame {i}: timeout (no data received)")
            continue
        samples = np.frombuffer(raw[:len(raw) - len(raw) % 2], dtype=np.int16)
        rms = np.sqrt(np.mean(samples.astype(np.float64) ** 2))
        peak = np.max(np.abs(samples))
        print(f"  Frame {i}: {len(samples)} samples, RMS={rms:.1f}, Peak={peak}")
    print("Capture complete.")


def main():
    parser = argparse.ArgumentParser(description="mmWave Radar Quick Start")
    parser.add_argument("--port", help="Serial port (auto-detect if omitted)")
    parser.add_argument("--firmware", default="mmwave_demo.bin",
                        help="Firmware binary to flash")
    parser.add_argument("--skip-flash", action="store_true",
                        help="Skip firmware flashing step")
    args = parser.parse_args()

    port = args.port or detect_evm()

    if not args.skip_flash:
        flash_firmware(port, args.firmware)

    print("Opening sensor control port ...")
    try:
        with serial.Serial(port, 115200, timeout=2) as ser:
            configure_basic_ranging(ser)
            capture_frames(ser)
    except Exception as exc:
        sys.exit(f"Serial error: {exc}")

    print("\nQuick start complete!  Next steps:")
    print("  - Review docs/user_guide.md for detailed configuration")
    print("  - Use tools/python/data_visualizer.py to see range profiles")
    print("  - See examples/basic_radar/ for a full embedded application")


if __name__ == "__main__":
    main()
