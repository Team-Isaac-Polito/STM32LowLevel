#!/bin/bash

# Helper Bash script for WSL and Linux. Does the diffing and .dfu upload.
# Called by CMake flash target and `flash_stm32.cmd` when using WSL fallback — Ninja-safe (no interactive stdin required)   
# Parameters:                                                                 
#   %1 = binary file path (absolute, Unix-style)

BIN_FILE="$1"
# Gets the path of the build starting from the binary path given as command argument
BUILD_PATH="$(dirname "$(realpath "$1")")"
DFU_VID_PID="0483:df11"
DFU_ADDR="0x08000000"
# Gets the path of the script location from this
PYTHON_SCRIPT_PATH="$(dirname "$(realpath "$0")")"

if [ -z "$BIN_FILE" ]; then
    echo -e "[flash] ERROR: No binary file provided.\n"
    exit 1
fi

if ! command -v dfu-util >/dev/null 2>&1; then
    echo -e "[flash] dfu-util not found on this system.\n\n"
    echo -e "[flash] Install it from the official https://sourceforge.net/projects/dfu-util/ page.\n\n"
    echo -e "[flash] After installing, run the workflow again:\n"
    echo -e  "     cmake --workflow --preset MK2_MOD1-flash\n\n"
    exit 1
fi

if [ -f "$BUILD_PATH/dump.bin" ]; then
    rm -- "$BUILD_PATH/dump.bin"
fi

echo -e "[flash] Found dfu-util. Dumping firmware...\n\n"

# Dumps the first bank (256KB in size)
dfu-util -d "$DFU_VID_PID" -a 0 --dfuse-address "$DFU_ADDR:262144" -U "$BUILD_PATH/dump.bin"
if [ -f "$BUILD_PATH/output.dfu" ]; then
    echo -e "[flash] Obtained last pushed firmware, diffing and flashing...\n\n"
    python "$PYTHON_SCRIPT_PATH/diff.py" "$BUILD_PATH/dump.bin" "$BIN_FILE"
    dfu-util -d "$DFU_VID_PID" -a 0 -D "$BUILD_PATH/output.dfu"
else
    echo -e "[flash] Unable to obtain the dumped firmware, attempting to send full binary...\n\n"
    dfu-util -d "$DFU_VID_PID" -a 0 --dfuse-address "$DFU_ADDR" -D "$BIN_FILE"
fi

exit $?