#!/usr/bin/env python3
"""
STM32LowLevel Serial Monitor — core module.

Monitors a USB CDC COM port (or UART-to-serial adapter) and prints
debug output with optional timestamps and file logging.
"""

import sys
import argparse
import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("pyserial not found. Install it with: pip install -r tools/serial_monitor/requirements.txt")
    sys.exit(1)


def list_ports():
    """List all available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print("Available serial ports:")
    for p in ports:
        desc = f"  {p.device:<10} {p.description}"
        if p.vid and p.pid:
            desc += f"  (VID={p.vid:04X} PID={p.pid:04X})"
        print(desc)


def auto_detect():
    """Try to find the STM32 USB CDC port or a common USB serial adapter."""
    ports = serial.tools.list_ports.comports()

    # Priority 1: STM32 Virtual COM Port (VID 0483 = STMicroelectronics)
    for p in ports:
        if p.vid == 0x0483:
            return p.device

    # Priority 2: Common USB-to-serial adapters (FTDI, CH340, CP210x)
    known_vids = {0x0403, 0x1A86, 0x10C4, 0x067B}
    for p in ports:
        if p.vid in known_vids:
            return p.device

    # Priority 3: Any USB serial port
    for p in ports:
        if "USB" in p.description.upper() or "SERIAL" in p.description.upper():
            return p.device

    return None


def monitor(port, baudrate=115200, timestamp=False, logfile=None):
    """Open the serial port and print incoming data."""
    try:
        ser = serial.Serial(port, baudrate, timeout=0.1)
    except serial.SerialException as e:
        print(f"Error opening {port}: {e}")
        sys.exit(1)

    print(f"Connected to {port} at {baudrate} baud. Press Ctrl+C to stop.")
    if logfile:
        print(f"Logging to: {logfile}")
    print("-" * 60)

    log_fp = None
    if logfile:
        log_fp = open(logfile, "a", encoding="utf-8", errors="replace")
        log_fp.write(
            f"\n--- Session started at {datetime.datetime.now().isoformat()} ---\n"
        )

    buf = b""
    try:
        while True:
            data = ser.read(256)
            if data:
                buf += data
                # Process complete lines
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    try:
                        text = line.decode("utf-8", errors="replace").rstrip("\r")
                    except Exception:
                        text = repr(line)

                    if timestamp:
                        ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
                        output = f"[{ts}] {text}"
                    else:
                        output = text

                    print(output)
                    if log_fp:
                        log_fp.write(output + "\n")
                        log_fp.flush()

    except KeyboardInterrupt:
        print("\n" + "-" * 60)
        print("Disconnected.")
    finally:
        ser.close()
        if log_fp:
            log_fp.write(
                f"--- Session ended at {datetime.datetime.now().isoformat()} ---\n"
            )
            log_fp.close()


def main():
    parser = argparse.ArgumentParser(
        description="STM32LowLevel Serial Monitor",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "port", nargs="?", help="COM port (e.g. COM5). Auto-detected if omitted."
    )
    parser.add_argument(
        "-b", "--baudrate", type=int, default=115200, help="Baud rate (default: 115200)"
    )
    parser.add_argument(
        "-t", "--timestamp", action="store_true",
        help="Prefix each line with timestamp",
    )
    parser.add_argument(
        "-l", "--log", metavar="FILE", help="Also log output to file",
    )
    parser.add_argument(
        "--list", action="store_true",
        help="List available serial ports and exit",
    )

    args = parser.parse_args()

    if args.list:
        list_ports()
        return

    port = args.port
    if not port:
        port = auto_detect()
        if port:
            print(f"Auto-detected port: {port}")
        else:
            print("No serial port auto-detected. Use --list to see available ports.")
            print("Specify one explicitly: python -m tools.serial_monitor COM5")
            sys.exit(1)

    monitor(port, args.baudrate, args.timestamp, args.log)


if __name__ == "__main__":
    main()
