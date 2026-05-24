# Getting Started — STM32LowLevel

This document explains how to **set up your development environment**, **configure**, and **build** the STM32LowLevel firmware on Windows using **VS Code**, **CMake**, **Ninja**, and the **Arm GNU Toolchain**.

---

## Prerequisites

You need four tools on your system before you can build. STM32CubeMX is optional and only needed for peripheral configuration.

| Tool | Minimum version | Purpose |
|---|---|---|
| [CMake](https://cmake.org/download/) | 3.25 | Build system generator |
| [Ninja](https://github.com/ninja-build/ninja/releases) | any recent | Fast build backend (required by `CMakePresets.json`) |
| [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) | 13.x or later | `arm-none-eabi-gcc` cross-compiler |
| [Visual Studio Code](https://code.visualstudio.com/) | any | Editor |

---

## 1. Install CMake

#### Option A — Windows Installer (Recommended)

Open a PowerShell and run:
```powershell
winget install Kitware.CMake
```

#### Option B — Manual Installation

1. Download the Windows installer from [cmake.org/download](https://cmake.org/download/).
2. During installation, select **"Add CMake to the system PATH for all users"**.
3. Verify:
   ```
   cmake --version
   ```

---

## 2. Install Ninja

Ninja is not included with CMake on Windows. Install it via **Chocolatey** (recommended) or manually.

#### Option A — Windows Installer (Recommended)

Open a PowerShell:
```powershell
winget install Ninja-build.Ninja
```

#### Option B — Chocolatey

If you have Chocolatey installed, open an **Administrator** PowerShell:
```powershell
choco install ninja
```

#### Option C — Manual

1. Download the `ninja-win.zip` from [github.com/ninja-build/ninja/releases](https://github.com/ninja-build/ninja/releases).
2. Extract `ninja.exe` to a folder, e.g. `C:\tools\ninja\`.
3. Add that folder to your **system PATH** (System Properties → Environment Variables → Path → New).

Verify:
```
ninja --version
```

---

## 3. Install the Arm GNU Toolchain

1. Download the **Windows (mingw-w64-i686) hosted** release from:
   [developer.arm.com/downloads/-/arm-gnu-toolchain-downloads](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
   
   Choose the `arm-none-eabi` variant (AArch32 bare-metal target), installer `.msi`.

2. Run the installer.

3. Add the toolchain's `bin` directory to your system PATH. By default, it is likely installed in:
   ```
   C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin
   ```

4. Verify:
   ```
   arm-none-eabi-gcc --version
   ```

> **STM32CubeIDE users:** If you already have STM32CubeIDE installed, a compatible toolchain is bundled inside:
> ```
> C:\ST\STM32CubeIDE_x.x.x\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.x.x.x...\tools\bin\
> ```
> You can add that folder to your PATH instead of installing the toolchain separately.

---

## 4. Install VS Code Extensions

Open VS Code and install the extensions below:

- **CMake Tools** (`ms-vscode.cmake-tools`) — configure and build from the sidebar
- **C/C++** (`ms-vscode.cpptools`) — IntelliSense and navigation

---

## 5. Clone and Open the Project

```powershell
git clone https://github.com/Team-Isaac-Polito/STM32LowLevel.git
cd STM32LowLevel
code .
```

Open the `STM32LowLevel/STM32LowLevel` subfolder as your working directory for all CMake commands:

```
STM32LowLevel/          ← repo root
└── STM32LowLevel/      ← CMakeLists.txt lives here — work from here
    ├── CMakeLists.txt
    ├── CMakePresets.json
    └── ...
```

---

## 6. Build & Flash Workflows

All commands run from inside `STM32LowLevel/STM32LowLevel/`.

### Option A — The Single-Command Workflow (Recommended with Hardware)

If your flashing environment is configured (either native Windows `dfu-util` or WSL + `usbipd` with `dfu-util` installed on the WSL side), you can execute the entire configuration, compilation, and flashing sequence with a single command utilizing CMake Workflows (requires CMake 3.25+):

```powershell
# Debug target workflow
cmake --workflow --preset MK2_MOD1-flash

# Release target workflow
cmake --workflow --preset MK2_MOD1-release-flash
```

**Note:** The workflow presets can still be used without the hardware connected. If the flashing step fails, the workflow will stop after the build step, leaving you with a compiled binary in `build/debug/MK2_MOD1/` or `build/release/MK2_MOD1/` that you can flash manually later. The detailed flashing instructions as well as the prerequisites are in Section 7 below.

### Option B — Build Presets (Compile Only)

To compile the project binary without launching the flashing routine:

```powershell
# First time (configure + build)
cmake --preset MK2_MOD1
cmake --build --preset MK2_MOD1
# Subsequent debug builds (just build)
cmake --build --preset MK2_MOD1

# Release target compilation (configure + build)
cmake --preset MK2_MOD1-release 
cmake --build --preset MK2_MOD1-release
# Subsequent release builds (just build)
cmake --build --preset MK2_MOD1-release
```

### Available Presets

The project supports the following target presets across debug, release, and automated workflow configurations:

| Target Module | Debug Preset (With Logs) | Release Preset (Optimized) | Workflow (Debug + Flash) | Workflow (Release + Flash) |
| --- | --- | --- | --- | --- |
| **Head (MOD1)** | `MK2_MOD1` | `MK2_MOD1-release` | `MK2_MOD1-flash` | `MK2_MOD1-release-flash` |
| **Middle (MOD2)** | `MK2_MOD2` | `MK2_MOD2-release` | `MK2_MOD2-flash` | `MK2_MOD2-release-flash` |
| **Tail (MOD3)** | `MK2_MOD3` | `MK2_MOD3-release` | `MK2_MOD3-flash` | `MK2_MOD3-release-flash` |

---

## 7. Flashing

Flashing is handled automatically via a custom script wrapper called by the `flash` target or workflow presets. On Windows hosts, the script automatically attempts a **WSL Fallback** if native Windows binaries for `dfu-util` are missing.

### Native Windows Flashing
If you have `dfu-util` installed on the Windows side and your board is connected, the workflow will use the native Windows binary to flash directly without needing WSL.

#### Installing dfu-util on Windows
1. Download the latest Windows binary package from [dfu-util.sourceforge.net](https://sourceforge.net/projects/dfu-util/) official page.
2. Extract the contents to a folder or the root directory, e.g., `C:\Program Files\dfu-util-0.11-binaries` or `C:\dfu-util-0.11-binaries`.
3. Add the binary directory of your Windows architecture (e.g., `C:\dfu-util-0.11-binaries\win32` or `C:\dfu-util-0.11-binaries\win64`) to your system PATH.
4. Verify:
   ```powershell
   dfu-util --version
   ```
5. Connect your board in DFU mode and run the flash workflow. The script will detect the Windows binary and use it to flash directly.
   ```powershell
   cmake --workflow --preset MK2_MOD1-flash
   ```

#### Critical WinUSB Driver Quirk (Zadig)
Windows treats STM32 devices in DFU mode as generic USB devices. `dfu-util` requires a specific driver (WinUSB) to talk to the device, but Windows usually assigns it the default "DFU in FS Mode" driver, which `libusb` (the library `dfu-util` uses) cannot access directly.

1. Boot the board into **DFU Bootloader Mode** using the switch.
2. Download Zadig: Get it from [zadig.akeo.ie](https://zadig.akeo.ie/).
3. Run Zadig as Administrator, and from the dropdown, select the device that corresponds to the STM32 board in DFU mode (e.g., "DFU in FS Mode").
4. You will see the Current Driver on the left (e.g., **None**) with the USB ID 0483:DF11 and the New Driver on the right. Ensure the Target is WinUSB (v6.x.x.x).
5. Click **Install Driver** and then verify that the Current Driver changes to WinUSB.
6. Rerun the flash workflow:
   ```powershell
   cmake --workflow --preset MK2_MOD1-flash
   ```

#### Manual Driver Path (If Zadig Fails)
Zadig is essentially an old-school Win32/x64 application. If you run it on Windows ARM64, it operates through an emulation layer. While basic applications run fine, **Zadig attempts to install kernel-mode drivers**.

Kernel drivers are architecture-specific. You have to bypass Zadig's x64-only driver library. WinUSB is a built-in, architecture-native driver provided by Microsoft. Because it is native to Windows, the ARM64 version of Windows has its own perfectly compatible version of `winusb.sys`.

1. In **Device Manager**, right-click the **"DFU in FS Mode"** device.
2. Select **Update driver**.
3. Select **"Browse my computer for drivers"**.
4. Select **"Let me pick from a list of available drivers on my computer"**.
5. Look for **"Universal Serial Bus devices"**.
6. Look for **"WinUSB Device"** on the left. After clicking, select the **WinUSB Device** on the right and click Next. Windows will force the driver change, bypassing Zadig's installer issues.
7. Rerun the flash workflow:
   ```powershell
   cmake --workflow --preset MK2_MOD1-flash
   ```

### WSL Passthrough

If you are running on Windows ARM64 (other Windows architectures can also use this setup) and your target `dfu-util` utility resides only inside WSL, you can completely ignore the Windows driver architecture by tricking the USB device into thinking it is connected to a Linux kernel. To achieve that, you must pass the physical micro-controller across the virtualization layer using `usbipd`:

1. Boot the board into **DFU Bootloader Mode** using the switch.
2. Install `usbipd` on Windows:
```powershell
winget install usbipd
```
3. Open an **Administrator PowerShell** on Windows and attach the device:
```powershell
usbipd list                        # Find the Bus ID for "DFU in FS Mode" (typically 0483:df11)
usbipd bind --busid <ID>           # Bind the device to WSL (First time only)
usbipd attach --wsl --busid <ID>   # Route the hardware into WSL instance
```

### Manual Flashing Procedure (WSL or Native)

Put your STM32G474RET6 board into DFU bootloader mode. Then, inside `STM32LowLevel/STM32LowLevel/`, run these commands sequentially:

```bash
# 1. Convert your compiled ELF to a raw uncompressed binary file
arm-none-eabi-objcopy -O binary build/debug/MK2_MOD1/STM32LowLevel.elf build/debug/MK2_MOD1/STM32LowLevel.bin

# 2. Flash the raw binary directly to the MCU internal flash memory
dfu-util -d 0483:df11 -a 0 --dfuse-address 0x08000000 -D build/debug/MK2_MOD1/STM32LowLevel.bin
```
where:
   - `0483:df11` (Vendor ID : Product ID): hardcoded USB identifier for the factory bootloader programmed by STMicroelectronics. This is the default DFU mode that all STM32G4 series chips enter when BOOT0 is tied high.
   - `-a 0` (Alternate Setting 0): selects the first (and in this case, only) memory interface exposed by the bootloader, which maps directly to the main internal flash array.
   - `0x08000000` (Memory Target Address): the base physical memory address where the internal flash begins on the ARM Cortex-M architecture. This tells `dfu-util` exactly where to write the raw binary payload since it contains no header information.
   - `build/debug/MK2_MOD1/STM32LowLevel.bin`: the path to the raw binary file generated from the compiled ELF firmware. Change `debug` to `release` if you want to flash the optimized build. Make sure to adjust the path if you are flashing a different module (MK2_MOD2 or MK2_MOD3).

---

## 8. USB CDC Debug Output

The firmware routes all debug output (`debug.log()`) to a USB CDC (Communications Device Class) Virtual COM Port. No external adapter is needed — just connect the board's USB FS port to your PC.

For detailed usage of the serial monitor tool, see [tools/serial_monitor/README.md](../tools/serial_monitor/README.md).

### First Connection Behavior

When the board powers on or resets:

1. The USB CDC port enumerates as a Virtual COM Port on the host PC
2. The firmware waits for the host DTR signal before proceeding with motor initialization
3. If no USB connection is detected, the firmware proceeds after a timeout

---

## Notes

- Reconfigure (re-run `cmake --preset ...`) whenever you switch modules. The build directory is module-specific — forgetting to reconfigure builds the wrong module silently.
- The USB CDC port replaces the old UART5 debug output. No UART-to-serial adapter is needed for basic debug output.

---

## Optional Sections

### 1. STM32CubeMX — viewing the .ioc file

The project includes a `STM32LowLevel.ioc` file that describes all peripheral configurations (GPIO, USART, CAN, DMA, clocks, etc.). You don't need CubeMX to **build** or **flash** the firmware, but you do need it if you want to **view or modify** the peripheral setup and regenerate the LL/HAL init code.

1. Download and install **STM32CubeMX** from the [ST website](https://www.st.com/en/development-tools/stm32cubemx.html).

2. Open CubeMX and use **File → Load Project** and select the `STM32LowLevel.ioc` file, or double-click the `.ioc` file directly from Windows Explorer.

3. To regenerate code after making changes, click the **Generate Code** button (gear icon in the toolbar).

> **Note:** Code generation will overwrite auto-generated files such as `Core/Src/gpio.c` and `Core/Src/main.c`. Any changes made **outside** the `/* USER CODE BEGIN / END */` markers will be lost. The project is already fully configured; this step is only needed if you change peripheral assignments.

### 2. Local GitHub Actions Testing with act

This section is for **contributors** who want to verify CI workflows locally before pushing.

You can run the CI workflows locally using [act](https://github.com/nektos/act) on WSL. This is useful to verify that builds and style checks pass before opening a PR.

#### Install act on WSL (Ubuntu)

```bash
curl -s https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash
```

Verify:
```bash
act --version
```

#### Running Workflows

From the `STM32LowLevel/` directory (where `.github/` lives):

**List available workflows:**
```bash
act --list
```

**Build workflow** (runs the 3-module matrix build):
```bash
act push
```

This triggers the `build.yml` workflow only (which has `on: push`). All three modules (MK2_MOD1, MK2_MOD2, MK2_MOD3) are built in both debug and release configurations.

**Style workflow** (clang-format + clang-tidy):
```bash
act pull_request
```

This triggers the `style.yml` and `build.yml` workflows (both of which have `on: pull_request`).

**Run a specific job only:**
```bash
act --job build
act --job build --matrix preset:MK2_MOD1
act --job clang-format
```

#### How It Works

`act` uses Docker containers to replicate the GitHub Actions runner environment locally. The first run is slower because it pulls the runner image. Subsequent runs are faster as the image is cached.

> **Tip:** If you only changed code in one module, you don't need to run the full matrix. Use the one-line CMake build commands from Section 6 instead. Use `act` when you want to verify CI will pass before pushing or opening a PR.

### 3. Style Fix Guide

#### Installation

If clang-format-19 is not installed:

```bash
sudo apt update
sudo apt install -y clang-format-19
```

#### Verify Style Checks

To verify that all files pass the style checks:

```bash
# Test that all files pass clang-format checks (same as GitHub workflow)
find Core/Src Lib USB_Device -name "*.cpp" -o -name "*.h" -o -name "*.tpp" | xargs clang-format-19 --dry-run --Werror

# If no output, all files pass style checks
# If output shows errors, files need formatting
```

#### Quick Auto-Fix (Recommended)

To automatically fix all style issues in the project according to the style workflow:

```bash
# Navigate to STM32LowLevel directory
cd STM32LowLevel

# Format ALL files that are checked by the style workflow
find Core/Src Lib USB_Device -name "*.cpp" -o -name "*.h" -o -name "*.tpp" | xargs clang-format-19 -i -style=file
```

#### Manual Check for Specific Files

To check if a specific file needs formatting:

```bash
# Check specific file
clang-format-19 -style=file Core/Src/main.cpp | diff -u Core/Src/main.cpp -

# If no output, file is properly formatted
# If output shows differences, file needs formatting
```

#### GitHub Style Workflow

The project uses two style checks:

1. **clang-format**: Checks code formatting (runs on all .cpp, .h, .tpp files)
2. **clang-tidy**: Checks naming conventions (runs only on .cpp files)

Both checks are run on pull requests to main branch.