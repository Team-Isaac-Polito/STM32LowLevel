# Getting Started — STM32LowLevel

This document explains how to **set up your development environment**, **configure**, and **build** the STM32LowLevel firmware on Windows using **VS Code**, **CMake**, **Ninja**, and the **Arm GNU Toolchain**.

---

## Prerequisites

You need four tools on your system before you can build:

| Tool | Minimum version | Purpose |
|---|---|---|
| [CMake](https://cmake.org/download/) | 3.22 | Build system generator |
| [Ninja](https://github.com/ninja-build/ninja/releases) | any recent | Fast build backend (required by `CMakePresets.json`) |
| [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) | 13.x or later | `arm-none-eabi-gcc` cross-compiler |
| [Visual Studio Code](https://code.visualstudio.com/) | any | Editor |

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
- **Cortex-Debug** (`marus25.cortex-debug`) — on-chip debugging (for later, once flashing is set up)

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
cmake --build build/Debug
```

The output `.elf` is at `build/Debug/STM32LowLevel.elf`.  
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
cmake --build build/Debug
```

---

## Notes

- Reconfigure (re-run `cmake --preset ...`) whenever you switch modules. The build directory is shared — forgetting to reconfigure builds the last-configured module silently.
