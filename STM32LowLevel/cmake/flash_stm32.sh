#!/bin/bash

# Helper Bash script for WSL and Linux. Does the diffing and .dfu upload.
# Called by CMake flash target — Ninja-safe (no interactive stdin required)   
# Parameters:                                                                 
#   %1 = binary file path (absolute, Windows-style)                           
#   %2 = --wsl-fallback   (internal: re-launched interactively for Y/N prompt)

BIN_FILE=$1
# Gets the path of the build starting from the binary path given as command argument (./build/debug/MK2_MOD1/STM32LowLevel.bin -> ./build/debug/MK2_MOD1/ but gets the absolute path)
BUILD_PATH=$(dirname "$(realpath $1)")
DFU_VID_PID="0483:df11"
DFU_ADDR="0x08000000"
# Gets the path of the script location from this (./cmake/flash_stm32.sh -> ./cmake/ but gets the absolute path)
PYTHON_SCRIPT_PATH=$(dirname "$(realpath $0)")

if [ -z "$BIN_FILE" ]; then
    printf "[flash] ERROR: No binary file provided.\n"
    exit 1
fi

if ! command -v dfu-util >/dev/null 2>&1; then
    printf "[flash] dfu-util not found on this system.\n\n"
    printf "[flash] Install it from the official https://sourceforge.net/projects/dfu-util/ page.\n\n"
    printf "[flash] After installing, run the workflow again:\n"
    printf  "     cmake --workflow --preset MK2_MOD1-flash\n\n"
    exit 1
fi

if [ -f "$BUILD_PATH/output.dfu" ]; then
    rm "$BUILD_PATH/output.dfu"
fi

printf "[flash] Found dfu-util. Dumping firmware...\n\n"

# Dumps the first bank (256KB in size)
dfu-util -d $DFU_VID_PID -a 0 --dfuse-address $DFU_ADDR:262144 -U "$BUILD_PATH/dump.bin"
if [ -f "$BUILD_PATH/output.dfu" ]; then
    printf "[flash] Obtained last pushed firmware, diffing and flashing...\n\n"
    python "$PYTHON_SCRIPT_PATH/diff.py" "$BUILD_PATH/dump.bin" "$BIN_FILE"
    dfu-util -d $DFU_VID_PID -a 0 -D "$BUILD_PATH/output.dfu"
else
    echo "[flash] Unable to obtain the dumped firmware, attempting to send full binary...\n\n"
    dfu-util -d $DFU_VID_PID -a 0 --dfuse-address $DFU_ADDR -D "$BIN_FILE"
fi

exit $?