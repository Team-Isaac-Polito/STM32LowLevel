# CAN Bus Testing Tool

PC-side CAN bus testing and debugging tool for the STM32LowLevel firmware.
Provides both an **interactive CLI** and a **web dashboard** for sending
commands and monitoring CAN bus traffic in real time.

## Hardware

- **USB2CAN adapter**: Innomaker USB2CAN MS124 ([Part-DB #233](https://part-db.teamisaac.it/en/part/233/info))
- **Driver**: `gs_usb` — uses the WinUSB class driver on Windows, native kernel driver on Linux
- **Bitrate**: 1 Mbit/s arbitration / 2 Mbit/s data (CAN FD with BRS, Extended 29-bit identifiers)

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

### Windows (WinUSB)

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

#### Windows ARM64 Quirk

The standard `libusb` wheel ships only x64 binaries.
On ARM64 Windows you must build `libusb` from source:

1. **Install Visual Studio 2022 or newer** with the *ARM64* build tools
   (C++ Desktop workload + ARM64 component).

2. **Add `msbuild` to your PATH** if it's not already available in your terminal:
   ```
   C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin
   ```
   Make sure to confirm the exact path on your system, as it may vary based on the Visual Studio version and edition. Run to confirm:
   ```powershell
   msbuild --version
   ```

3. **Clone and build libusb**:
   ```powershell
   # Running from the root of your disk (e.g. C:\)
   git clone https://github.com/libusb/libusb.git
   cd libusb
   cd msvc
   # Open the VS solution and build the ARM64-Release target, or:
   msbuild libusb_dll.vcxproj /p:Configuration=Release /p:Platform=ARM64 
   ```

4. **Copy the DLL** next to your Python interpreter:
   ```powershell
   # Adjust Python path as needed for your Python version and installation location
   copy C:\libusb\build\v145\ARM64\Release\dll\libusb-1.0.dll "$env:LOCALAPPDATA\Programs\Python\Python313-arm64\" 
   ```
   If you are using `uv` or you want to keep everything local in your `venv`, run this instead:
   ```powershell
   # Adjust the path to your virtual environment's directory
   copy C:\libusb\build\v145\ARM64\Release\dll\libusb-1.0.dll "{path-to-your-venv}"
   ```
   If you want to make the DLL available system-wide in case you still have issues locating the dll in your environment, you can copy it to System32:
   ```powershell
   # Use with caution — It may cause potential conflicts with your existing libraries and require admin permissions
   copy C:\libusb\build\v145\ARM64\Release\dll\libusb-1.0.dll "C:\Windows\System32"
   ```

5. The CAN tester's `__init__.py` automatically detects ARM64 Windows and
   patches `pyusb` to load `libusb-1.0.dll` from that location — no
   manual environment variables needed.

6. Then follow the same WinUSB driver steps described in the **Windows (WinUSB)** upper setup section above.

---

## Ixxat USB-to-CAN/FD Standard setup (Issue #67)

The Ixxat adapter is **not** a `gs_usb` device. Do not use the Innomaker
WinUSB/`gs_usb` driver setup above for this adapter.

### Windows (Ixxat VCI)

1. Install the compatible **HMS Ixxat VCI V4** Windows driver for your exact
   USB-to-CAN/FD Standard model from the official HMS/Ixxat download pages.
   Follow the manufacturer's installation and restart instructions.
2. Plug in the adapter and confirm it appears correctly in Windows Device
   Manager or the HMS VCI diagnostic utilities. Check its CAN channel numbering.
3. Install the Python dependencies as shown in the Installation section.
4. Connect the adapter to a powered, correctly terminated CAN FD test bus,
   using the module's specified CAN wiring. Do not attach motion-capable
   hardware until the passive receive check has been approved.
5. Start the CAN tester with the Ixxat backend:

```powershell
# Passive feedback monitoring from the interactive terminal
python -m tools.can_tester -i ixxat -c 0 -b 1000000 -D 2000000

# Alternative: browser dashboard, http://localhost:8080
python -m tools.can_tester.web_dashboard -i ixxat -c 0 -b 1000000 -D 2000000 --debug
```

The `ixxat` backend is Windows-specific and needs a working VCI
installation. The channel is **zero-based and numeric**, typically `0`
for the first CAN controller. The CAN tester normalizes this CLI value
to an integer for the Ixxat backend. Do not assume every USB product
or driver revision is compatible with the installed `python-can`
version: check the VCI diagnostic utilities and the supported Ixxat
backend documentation if bus creation fails.

### Linux (Ixxat via SocketCAN)

A compatible Linux kernel or vendor driver must first expose the Ixxat
CAN FD channel as a network interface (for example `can0`). Support
depends on the **exact Ixxat device and driver version**. The
`python-can` Windows `ixxat` backend is not a replacement for a
Linux driver. If your adapter is not recognized as a CAN interface,
verify official Linux support before proceeding.

```bash
ip -details link show
# Only when the correct CAN network interface is present:
sudo ip link set can0 down
sudo ip link set can0 up type can bitrate 1000000 dbitrate 2000000 fd on

python -m tools.can_tester -i socketcan -c can0
python -m tools.can_tester.web_dashboard -i socketcan -c can0 --debug
```

### macOS

No native macOS driver/backend has been validated for this exact Ixxat
adapter. The adapter cannot be assumed to work through `gs_usb` or
the Windows-only `ixxat` backend. Use a supported Windows/Linux test
host until a working macOS driver is confirmed.

### Verification checklist

- [ ] Driver or SocketCAN interface recognizes the exact Ixxat adapter.
- [ ] CLI starts at **1 Mbps nominal / 2 Mbps data** bitrate, CAN FD enabled.
- [ ] Passively receive STM32 feedback using 29-bit extended IDs and
      CAN FD frames with BRS. Confirm using independent CAN diagnostics.
- [ ] Confirm transmitted command frames preserve their extended IDs,
      CAN FD flag, BRS and data bytes. Compare against known-good traffic.
- [ ] Verify a safe command with an STM32 module and inspect the
      corresponding feedback (hardware supervision required).
- [ ] Verify monitoring and safe commands through the web dashboard.
- [ ] Test automated motion sequences only with the team's established
      bench safety measures, a suitable stop mechanism and cleared workspace.
- [ ] Record OS, driver version, `python-can` version, adapter firmware,
      interface/channel, and any unresolved limitations.

Without the physical Ixxat adapter and STM32 test setup, software-only
unit tests cannot establish hardware acceptance.

### Hardware-independent checks

```bash
python -m unittest tools.can_tester.test_connection -v
python -m tools.can_tester.test_dry
python -m tools.can_tester.web_dashboard --demo
```

The new `test_connection.py` checks interface-specific arguments and
that the existing sender constructs CAN FD, BRS-enabled extended frames.
These tests do not open the physical Ixxat device.

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

# Debug mode — print received CAN messages to terminal
# Useful when the dashboard display isn't working, to verify the backend
# is receiving messages from the adapter:
python -m tools.can_tester.web_dashboard -i gs_usb -c 0 --debug
```

Open `http://localhost:8080` in a browser. Features:

- Real-time message feed via SSE (auto-reconnecting)
- Filter by subsystem (arm, traction, joint, feedback)
- Module-aware command panel — shows arm commands for MOD1, joint commands for MOD2/MOD3
- Target module selector with CAN address display
- Command descriptions with value ranges
- Emergency stop button
- Collapsible message statistics

**Debugging:** If the dashboard shows no messages, use `--debug` to print received
CAN messages to the terminal. You can also check `http://localhost:8080/status`
to see the last received message. This helps distinguish between hardware issues
(backend receives nothing) and display issues (backend receives but browser doesn't show).

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
