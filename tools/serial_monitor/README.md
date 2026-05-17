# Serial Monitor

A simple serial monitor for STM32LowLevel debug output. Connects to a USB CDC COM port (or UART-to-serial adapter) and prints debug messages with optional timestamps and file logging.

**Note:** All the commands below should be run from the `STM32LowLevel/` repo root.

## Installation

```bash
pip install -r tools/serial_monitor/requirements.txt
```

## Usage

```bash
# Auto-detect port and monitor
python -m tools.serial_monitor

# Explicit port
python -m tools.serial_monitor COM5

# With timestamps
python -m tools.serial_monitor COM5 --timestamp

# Log to file
python -m tools.serial_monitor COM5 --log debug_output.txt

# List available ports
python -m tools.serial_monitor --list
```

## Options

| Flag | Description |
|---|---|
| `port` | COM port (e.g. `COM5`). Auto-detected if omitted. |
| `-b`, `--baudrate` | Baud rate (default: 115200) |
| `-t`, `--timestamp` | Prefix each line with `[HH:MM:SS.mmm]` |
| `-l`, `--log FILE` | Append output to file |
| `--list` | List available serial ports and exit |

Press `Ctrl+C` to stop.

## Auto-detection priority

1. STM32 USB CDC (VID 0x0483)
2. Known USB-to-serial adapters (FTDI, CH340, CP210x)
3. Any USB serial port
