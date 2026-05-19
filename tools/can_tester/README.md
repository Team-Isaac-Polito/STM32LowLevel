# CAN Bus Testing Tool

PC-side CAN bus testing and debugging tool for the STM32LowLevel firmware.
Provides both an **interactive CLI** and a **web dashboard** for sending
commands and monitoring CAN bus traffic in real time.

## Hardware

- **USB2CAN adapter**: Innomaker USB2CAN MS124 ([Part-DB #233](https://part-db.teamisaac.it/en/part/233/info))
- **Driver**: `gs_usb` — uses the WinUSB class driver on Windows, native kernel driver on Linux
- **Bitrate**: 1 Mbit/s arbitration / 2 Mbit/s data (CAN FD with BRS, Extended 29-bit identifiers)
- **Connector**: D-SUB 9-pin CAN. CAN-H on pin 7, CAN-L on pin 2, GND on pin 3

## Prerequisites

- Python ≥ 3.10
- The USB2CAN adapter plugged in via USB
- CAN bus wiring between the adapter and at least one STM32 module

## Installation

```bash
cd STM32LowLevel
pip install -r tools/can_tester/requirements.txt
```

> All commands below assume the working directory is `STM32LowLevel/` and
> `PYTHONPATH` includes `.` (or you use `python -m` from that directory).

---

## Platform Setup

### Linux (SocketCAN)

The `gs_usb` kernel module is loaded automatically when you plug in the adapter.

```bash
# Verify the device is detected
dmesg | grep gs_usb

# Bring up the CAN FD interface at 1 Mbit/s arbitration + 2 Mbit/s data
sudo ip link set can0 up type can bitrate 1000000 dbitrate 2000000 fd on

# Verify
ip -details link show can0
```

### Windows (x64)

If you have issues with the version of the driver that Windows automatically installs, you can manually switch to the WinUSB driver:

1. Plug in the USB2CAN adapter.
2. Open **Device Manager** → find the device under *Universal Serial Bus devices* or *Other devices*
   (it may appear as "USB2CAN" or "CAN").
3. Right-click → **Update driver** → **Browse my computer for drivers**
   → **Let me pick from a list** → select **Universal Serial Bus devices**
   → choose **WinUSB Device**
4. No additional driver download is needed — the `gs_usb` Python package
   talks to WinUSB directly via `pyusb` / `libusb`.

> **Reference**: The [Innomaker USB2CAN GitHub repo](https://github.com/INNO-MAKER/usb2can)
> has the official user manual, Windows software, and firmware notes.

### Windows ARM64 (e.g. Snapdragon laptops)

The standard `libusb` wheel ships only x64 binaries.
On ARM64 Windows you must build `libusb` from source:

1. **Install Visual Studio 2022** with the *ARM64* build tools
   (C++ Desktop workload + ARM64 component).

2. **Clone and build libusb**:
   ```powershell
   git clone https://github.com/libusb/libusb.git
   cd libusb
   # Open the VS solution and build the ARM64-Release target, or:
   msbuild msvc\libusb_static_2022.vcxproj /p:Configuration=Release /p:Platform=ARM64
   ```

3. **Copy the DLL** next to your Python interpreter:
   ```powershell
   copy ARM64\Release\dll\libusb-1.0.dll "$env:LOCALAPPDATA\Programs\Python\Python313-arm64\"
   ```

4. The CAN tester's `__init__.py` automatically detects ARM64 Windows and
   patches `pyusb` to load `libusb-1.0.dll` from that location — no
   manual environment variables needed.

5. Then follow the same WinUSB driver steps described above.

---

## Usage

### Web Dashboard

```bash
# Start with USB2CAN adapter (gs_usb)
python -m tools.can_tester.web_dashboard -i gs_usb -c 0 --port 8080

# Start with SocketCAN (Linux)
python -m tools.can_tester.web_dashboard -i socketcan -c can0

# Demo mode — no hardware required (virtual bus)
python -m tools.can_tester.web_dashboard --demo

# Custom port
python -m tools.can_tester.web_dashboard -i gs_usb -c 0 -p 9000
```

Open `http://localhost:8080` in a browser. Features:

- Real-time message feed via SSE (auto-reconnecting)
- Filter by subsystem (arm, traction, joint, feedback)
- Module-aware command panel — shows arm commands for MOD1, joint commands for MOD2/MOD3
- Target module selector with CAN address display
- Command descriptions with value ranges
- Emergency stop button
- Collapsible message statistics

### Interactive CLI

```bash
# Linux (SocketCAN)
python -m tools.can_tester -i socketcan -c can0

# Windows / Linux (gs_usb)
python -m tools.can_tester -i gs_usb -c 0

# Target a specific module
python -m tools.can_tester -i gs_usb -c 0 -m MK2_MOD2
```

#### CLI Commands

| Command | Description |
|---------|-------------|
| `send traction <left> <right>` | Set traction motor speeds (RPM) |
| `send arm_j2 <angle>` | Set arm elbow pitch (radians) |
| `send arm_j3 <angle>` | Set arm roll J3 (radians) |
| `send arm_j4 <angle>` | Set arm wrist pitch (radians) |
| `send arm_j5 <angle>` | Set arm wrist roll (radians) |
| `send arm_1a1b <θ> <φ>` | Set arm J1 differential (radians) |
| `send beak open\|close` | Control beak gripper |
| `send reset_arm` | Move arm to home position |
| `send reboot_arm` | Reboot arm Dynamixel motors |
| `send reboot_traction` | Reboot traction motors |
| `stop` | Emergency stop all motors |
| `monitor [filter]` | Start live monitoring (filters: `all`, `arm`, `traction`, `joint`, `feedback`) |
| `test <name>` | Run a test sequence (see `procedure.md`) |
| `status` | Show message statistics |

#### Test Sequences

| Name | Description |
|------|-------------|
| `traction` | Forward → stop → reverse → stop |
| `traction_diff` | Turn left → turn right → spin |
| `arm_init` | Reset arm to home position |
| `arm_joints` | Move each arm joint individually |
| `arm_beak` | Beak open/close cycle |
| `arm_reboot` | Reboot and re-initialize arm motors |
| `full` | Complete diagnostic sequence |

---

## Architecture

```
can_tester/
├── __init__.py          # ARM64 Windows libusb workaround
├── __main__.py          # CLI entry point
├── protocol.py          # CAN ID / MsgType definitions (synced with communication.h)
├── header_parser.py     # C preprocessor for communication.h / mod_config.h validation
├── codec.py             # Payload encode/decode (struct-based)
├── sender.py            # High-level message sender
├── monitor.py           # Real-time bus monitor with named filters
├── cli.py               # Interactive REPL
├── web_dashboard.py     # Flask web UI with SSE
├── test_sequences.py    # Pre-built test routines
├── test_dry.py          # Offline validation tests
├── requirements.txt     # Python dependencies
├── procedure.md         # Testing procedures and checklists
└── README.md            # This file
```

## CAN Protocol

See the [CAN bus protocol documentation](https://docs.teamisaac.it/doc/can-bus-protocol-t40e2NOEqp)
for the full message ID table.

Extended CAN identifiers (29-bit) are encoded as:
- **Byte 0** `[0:7]`: Source address (sender module ID)
- **Byte 1** `[8:15]`: Destination address
- **Byte 2** `[16:23]`: Message type (PDU Format)

| Address | Module | Subsystems |
|---------|--------|------------|
| `0x00`  | Central (Jetson / PC) | — |
| `0x21`  | MK2_MOD1 — Head | Traction + Robotic Arm (6-DOF + beak) |
| `0x22`  | MK2_MOD2 — Body | Traction + Inter-module Joint |
| `0x23`  | MK2_MOD3 — Tail | Traction + Inter-module Joint |

## References

- [CAN bus protocol](https://docs.teamisaac.it/doc/can-bus-protocol-t40e2NOEqp)
- [STM32LowLevel firmware](https://github.com/Team-Isaac-Polito/STM32LowLevel)
- [Innomaker USB2CAN repo](https://github.com/INNO-MAKER/usb2can) — user manual, Windows software, firmware
- [python-can documentation](https://python-can.readthedocs.io/)
- [Part-DB entry for USB2CAN MS124](https://part-db.teamisaac.it/en/part/233/info)
