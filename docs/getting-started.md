# Getting Started — STM32LowLevel

This document explains how to **set up your development environment**, **configure**, and **build** the STM32LowLevel firmware on Windows using **VS Code**, **CMake**, **Ninja**, and the **Arm GNU Toolchain**.

---

## Prerequisites

You need four tools on your system before you can build. STM32CubeMX is optional and only needed for peripheral configuration.

| Tool | Minimum version | Purpose |
|---|---|---|
| [CMake](https://cmake.org/download/) | 3.22 | Build system generator |
| [Ninja](https://github.com/ninja-build/ninja/releases) | any recent | Fast build backend (required by `CMakePresets.json`) |
| [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) | 13.x or later | `arm-none-eabi-gcc` cross-compiler |
| [Visual Studio Code](https://code.visualstudio.com/) | any | Editor |
| [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html) | any | *(Optional)* View/edit `.ioc` peripheral config |

---

## 1. Install CMake

1. Download the Windows installer from [cmake.org/download](https://cmake.org/download/).
2. During installation, select **"Add CMake to the system PATH for all users"**.
3. Verify:
   ```
   cmake --version
   ```

---

## 2. Install Ninja

Ninja is not included with CMake on Windows. Install it via **Chocolatey** (recommended) or manually.

#### Option A — Chocolatey

Open an **Administrator** PowerShell:
```powershell
choco install ninja
```

#### Option B — Manual

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
   
   Choose the `arm-none-eabi` variant (bare-metal target), installer `.exe`.

2. Run the installer. At the last step, check **"Add path to environment variable"**.

3. Verify:
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

Open VS Code and install:

- **CMake Tools** (`ms-vscode.cmake-tools`) — configure and build from the sidebar
- **C/C++** (`ms-vscode.cpptools`) — IntelliSense and navigation

---

## 5. Clone and Open the Project

```bash
git clone https://github.com/Team-Isaac-Polito/STM32LowLevel.git
cd STM32LowLevel
code .
```

Open the **inner** `STM32LowLevel/` subfolder as your working directory for all CMake commands:

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

```bash
# Debug target workflow
cmake --workflow --preset MK2_MOD1-flash

# Release target workflow
cmake --workflow --preset MK2_MOD1-release-flash
```

**Note:** The workflow presets can still be used without the hardware connected. If the flashing step fails, the workflow will stop after the build step, leaving you with a compiled binary in `build/debug/MK2_MOD1/` or `build/release/MK2_MOD1/` that you can flash manually later.

### Option B — Manual One-Line Build (Compile Only)

To compile the project binary without launching the flashing routine:

```bash
# First time (configure + build)
cmake --preset MK2_MOD1 && cmake --build --preset MK2_MOD1
# Subsequent debug builds (just build)
cmake --build --preset MK2_MOD1

# Release target compilation (configure + build)
cmake --preset MK2_MOD1-release && cmake --build --preset MK2_MOD1-release
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

## 7. Flashing & WSL Setup

Flashing is handled automatically via a custom script wrapper called by the `flash` target or workflow presets. On Windows hosts, the script automatically attempts a **WSL Fallback** if native Windows binaries for `dfu-util` are missing.

### WSL Passthrough Prerequisites

If you are running on Windows ARM64 (other Windows architectures can also use this setup) and your target `dfu-util` utility resides only inside WSL, you must pass the physical micro-controller across the virtualization layer using `usbipd`:

1. Boot the board into **DFU Bootloader Mode** using the switch.
2. Open an **Administrator PowerShell** on Windows and attach the device:
```powershell
usbipd list                        # Find the Bus ID for "DFU in FS Mode" (typically 0483:df11)
usbipd bind --busid <ID>           # Bind the device to WSL (First time only)
usbipd attach --wsl --busid <ID>   # Route the hardware into WSL instance
```

*See our outline documentation [Step 3: Setting Up USBIPD in PowerShell](https://docs.teamisaac.it/doc/kernel-and-usbipd-B5cYVhJ1Gv) for detailed setup instructions.*

### Critical WSL Default Distro Quirk

When executing the scripted fallback from Windows, the script boots into your system's **Default WSL Distribution**. If you have multiple distributions (or use Docker Desktop), this can cause a `dfu-util: command not found` error even if it works in your favorite terminal.

Ensure your preferred Linux distribution (where `dfu-util` is installed) is explicitly configured as your Windows default:

```powershell
# Check current defaults (marked with an asterisk *)
wsl -l -v

# Set your primary development distro as default
wsl --set-default <Your-Distro-Name>  # e.g., wsl --set-default Ubuntu

```

Once `usbipd` is attached and your default distro is correct, running `cmake --workflow --preset MK2_MOD1-flash` will open an interactive prompt window, cleanly bridging your Windows build tree to your WSL flashing environment.

### Manual Flashing Procedure

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

### STM32CubeMX — viewing the .ioc file

The project includes a `STM32LowLevel.ioc` file that describes all peripheral configurations (GPIO, USART, CAN, DMA, clocks, etc.). You don't need CubeMX to **build** or **flash** the firmware, but you do need it if you want to **view or modify** the peripheral setup and regenerate the LL/HAL init code.

1. Download and install **STM32CubeMX** from the [ST website](https://www.st.com/en/development-tools/stm32cubemx.html).

2. Open CubeMX and use **File → Load Project** and select the `STM32LowLevel.ioc` file, or double-click the `.ioc` file directly from Windows Explorer.

3. To regenerate code after making changes, click the **Generate Code** button (gear icon in the toolbar).

> **Note:** Code generation will overwrite auto-generated files such as `Core/Src/gpio.c` and `Core/Src/main.c`. Any changes made **outside** the `/* USER CODE BEGIN / END */` markers will be lost. The project is already fully configured; this step is only needed if you change peripheral assignments.

### Local GitHub Actions Testing with act

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

### Style Fix Guide

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