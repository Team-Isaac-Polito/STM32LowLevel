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

## 6. Build

All commands run from inside `STM32LowLevel/STM32LowLevel/`.

#### Step 1 — Configure

Pass the module you want to build using `MODULE_DEFINE`:

| Module | `MODULE_DEFINE` | CAN ID |
|---|---|---|
| Head (ARM) | `MK2_MOD1` | `0x21` |
| Middle (JOINT) | `MK2_MOD2` | `0x22` |
| Tail (TRACTION) | `MK2_MOD3` | `0x23` |

```bash
cmake --preset Debug -DMODULE_DEFINE=MK2_MOD1
```

#### Step 2 — Build

```bash
cmake --build build/MK2_MOD1
```

The output `.elf` is at `build/MK2_MOD1/STM32LowLevel.elf`.  
Memory usage is printed at the end:

```
Memory region         Used Size  Region Size  %age Used
             RAM:        4752 B       128 KB      3.63%
           FLASH:       47356 B       512 KB      9.03%
```

#### Convenience — named presets

The three module configurations also have named presets in `CMakePresets.json`:

```bash
cmake --preset MK2_MOD1   # configures + selects module in one step
cmake --build build/MK2_MOD1
```

---

## 7. Flashing

**Note:** This flashing guide is written for the Linux/Ubuntu environment.

For WSL users, you must first attach the physical USB device to your WSL instance using `usbipd` from an Administrator PowerShell prompt before running the flashing commands:
```powershell
# In Windows PowerShell (Admin)
usbipd list                        # Find the Bus ID for "DFU in FS Mode" (typically 0483:df11)
usbipd bind --busid <ID>           # Bind the device to WSL
usbipd attach --wsl --busid <ID>   # Bind it to your active WSL instance
```

*See our outline documentation [Step 3: Setting Up USBIPD in PowerShell](https://docs.teamisaac.it/doc/kernel-and-usbipd-B5cYVhJ1Gv) for detailed setup instructions.*

### Flashing Procedure

Put your STM32G474RET6 board into DFU bootloader mode. Then, inside `STM32LowLevel/STM32LowLevel/`, run these commands sequentially:

```bash
# 1. Convert your compiled ELF to a raw uncompressed binary file
arm-none-eabi-objcopy -O binary build/MK2_MOD1/STM32LowLevel.elf build/MK2_MOD1/STM32LowLevel.bin

# 2. Flash the raw binary directly to the MCU internal flash memory
dfu-util -d 0483:df11 -a 0 --dfuse-address 0x08000000 -D build/MK2_MOD1/STM32LowLevel.bin
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

## (Optional) STM32CubeMX — viewing the .ioc file

The project includes a `STM32LowLevel.ioc` file that describes all peripheral configurations (GPIO, USART, CAN, DMA, clocks, etc.). You don't need CubeMX to **build** or **flash** the firmware, but you do need it if you want to **view or modify** the peripheral setup and regenerate the LL/HAL init code.

1. Download and install **STM32CubeMX** from the [ST website](https://www.st.com/en/development-tools/stm32cubemx.html).

2. Open CubeMX and use **File → Load Project** and select the `STM32LowLevel.ioc` file, or double-click the `.ioc` file directly from Windows Explorer.

3. To regenerate code after making changes, click the **Generate Code** button (gear icon in the toolbar).

> **Note:** Code generation will overwrite auto-generated files such as `Core/Src/gpio.c` and `Core/Src/main.c`. Any changes made **outside** the `/* USER CODE BEGIN / END */` markers will be lost. The project is already fully configured; this step is only needed if you change peripheral assignments.

---

## Notes

- Reconfigure (re-run `cmake --preset ...`) whenever you switch modules. The build directory is module-specific — forgetting to reconfigure builds the wrong module silently.
- The USB CDC port replaces the old UART5 debug output. No UART-to-serial adapter is needed for basic debug output.
