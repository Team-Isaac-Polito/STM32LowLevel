setlocal enabledelayedexpansion

:: Flash STM32 via dfu-util with WSL fallback
:: Called by CMake flash target — Ninja-safe (no interactive stdin required)
::
:: Parameters:
::   %1 = binary file path (absolute, Windows-style)
::   %2 = --wsl-fallback   (internal: re-launched interactively for Y/N prompt)

if "%~1"=="" (
    echo [flash] ERROR: No binary file provided.
    exit /b 1
)
set BIN_FILE=%~1
set DFU_VID_PID=0483:df11
set DFU_ADDR=0x08000000
set PYTHON_SCRIPT_PATH=%~dp0
:: Gets the path for the bin file (e.s.: C:\STM32LowLevel\STM32LowLevel\build\debug\MK2_MOD1\STM32LowLevel.bin
:: gives C:\STM32LowLevel\STM32LowLevel\build\debug\MK2_MOD1\)
for %%i in ("%BIN_FILE%") do set BUILD_PATH=%%~dpi
:: ── Internal re-launch path: interactive WSL Y/N prompt ──────────────────────
if "%~2"=="--wsl-fallback" goto :wsl_interactive

:: ── Primary path (called by Ninja/CMake) ─────────────────────────────────────
echo.
echo [flash] Target binary : %BIN_FILE%
echo.

echo [flash] Looking for native dfu-util...
where dfu-util >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [flash] Found dfu-util. Dumping firmware...
    echo.
    :: `dfu-util` will refuse to dump the firmware if the file already exists
    if exist "%BUILD_PATH%\dump.bin" (
        del "%BUILD_PATH%\dump.bin"
    )
    :: Dumps the first bank (256KB in size)
    dfu-util -d %DFU_VID_PID% -a 0 --dfuse-address %DFU_ADDR%:262144 -U "%BUILD_PATH%\dump.bin"
    if exist "%BUILD_PATH%\dump.bin" (
        echo [flash] Obtained last pushed firmware, diffing and flashing...
        echo.
        python "%PYTHON_SCRIPT_PATH%\diff.py" "%BUILD_PATH%\dump.bin" "%BIN_FILE%"
        dfu-util -d %DFU_VID_PID% -a 0 -D "%BUILD_PATH%\output.dfu"
    ) else (
        echo [flash] Unable to obtain the dumped firmware, attempting to send full binary...
        echo.
        dfu-util -d %DFU_VID_PID% -a 0 --dfuse-address %DFU_ADDR% -D "%BIN_FILE%"
    )
    :: %ERRORLEVEL% doesn't get evaluated again inside an if() statement, !<variable>! gets always evaluated at runtime.
    :: For example: dfu-util doesn't flash successfully -> %ERRORLEVEL% EQU 0, whereas !ERRORLEVEL! EQU 1 !!!! 
    exit /b !ERRORLEVEL!
)

:: dfu-util not found — print install instructions
echo [flash] dfu-util not found on this system.
echo.
echo [flash] Install it from the official https://sourceforge.net/projects/dfu-util/ page.
echo.
echo [flash] After installing, run the workflow again:
echo     cmake --workflow --preset MK2_MOD1-flash
echo.
echo [flash] --- WSL fallback ---
echo [flash] If you have dfu-util in WSL and the device attached via usbipd,
echo [flash] a new window will open to ask whether to proceed.
echo.
echo [flash] Required usbipd commands before proceeding with WSL flashing (Admin PowerShell):
echo     usbipd list
echo     usbipd bind   --busid ^<BUSID^>
echo     usbipd attach --wsl --busid ^<BUSID^>
echo.

:: Re-launch this script in a new interactive console window for the Y/N prompt.
:: This is necessary because Ninja redirects stdin, making choice.exe fail.
:: start /wait opens a new cmd window (with a real TTY) and waits for it to exit.
start "STM32 Flash via WSL" /wait cmd.exe /c ""%~f0" "%BIN_FILE%" --wsl-fallback"
exit /b %ERRORLEVEL%


:: ── WSL interactive path (runs in its own console window) ────────────────────
:wsl_interactive
echo.
echo [flash] ================================================================
echo [flash]  WSL Flash Fallback
echo [flash]  Make sure the USB device is attached to WSL via usbipd first.
echo [flash] ================================================================
echo.

choice /C YN /N /M "[flash] Proceed with WSL flashing? [Y/N]: "
if %ERRORLEVEL% NEQ 1 goto :wsl_cancelled

echo.
echo [flash] Converting path for WSL...

:: Convert Windows path to WSL /mnt/<drive>/... path
set WSL_PATH=%BIN_FILE%
set WSL_PATH=%WSL_PATH:\=/%
set WSL_PATH=%WSL_PATH:A:=/mnt/a%
set WSL_PATH=%WSL_PATH:B:=/mnt/b%
set WSL_PATH=%WSL_PATH:C:=/mnt/c%
set WSL_PATH=%WSL_PATH:D:=/mnt/d%
set WSL_PATH=%WSL_PATH:E:=/mnt/e%
set WSL_PATH=%WSL_PATH:F:=/mnt/f%
set WSL_PATH=%WSL_PATH:G:=/mnt/g%
set WSL_PATH=%WSL_PATH:H:=/mnt/h%
set WSL_PATH=%WSL_PATH:I:=/mnt/i%
set WSL_PATH=%WSL_PATH:J:=/mnt/j%
set WSL_PATH=%WSL_PATH:K:=/mnt/k%
set WSL_PATH=%WSL_PATH:L:=/mnt/l%
set WSL_PATH=%WSL_PATH:M:=/mnt/m%
set WSL_PATH=%WSL_PATH:N:=/mnt/n%
set WSL_PATH=%WSL_PATH:O:=/mnt/o%
set WSL_PATH=%WSL_PATH:P:=/mnt/p%
set WSL_PATH=%WSL_PATH:Q:=/mnt/q%
set WSL_PATH=%WSL_PATH:R:=/mnt/r%
set WSL_PATH=%WSL_PATH:S:=/mnt/s%
set WSL_PATH=%WSL_PATH:T:=/mnt/t%
set WSL_PATH=%WSL_PATH:U:=/mnt/u%
set WSL_PATH=%WSL_PATH:V:=/mnt/v%
set WSL_PATH=%WSL_PATH:W:=/mnt/w%
set WSL_PATH=%WSL_PATH:X:=/mnt/x%
set WSL_PATH=%WSL_PATH:Y:=/mnt/y%
set WSL_PATH=%WSL_PATH:Z:=/mnt/z%

set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_PATH%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:\=/%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:A:=/mnt/a%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:B:=/mnt/b%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:C:=/mnt/c%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:D:=/mnt/d%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:E:=/mnt/e%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:F:=/mnt/f%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:G:=/mnt/g%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:H:=/mnt/h%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:I:=/mnt/i%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:J:=/mnt/j%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:K:=/mnt/k%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:L:=/mnt/l%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:M:=/mnt/m%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:N:=/mnt/n%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:O:=/mnt/o%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:P:=/mnt/p%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:Q:=/mnt/q%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:R:=/mnt/r%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:S:=/mnt/s%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:T:=/mnt/t%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:U:=/mnt/u%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:V:=/mnt/v%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:W:=/mnt/w%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:X:=/mnt/x%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:Y:=/mnt/y%
set PYTHON_SCRIPT_WSL_PATH=%PYTHON_SCRIPT_WSL_PATH:Z:=/mnt/z%


echo [flash] WSL path: %WSL_PATH%
echo [flash] Python script WSL path: %PYTHON_SCRIPT_WSL_PATH%
echo.
wsl -- bash -lc "'%PYTHON_SCRIPT_WSL_PATH%/flash_stm32.sh' %WSL_PATH%"
set EXIT_CODE=%ERRORLEVEL%
echo.
if %EXIT_CODE% EQU 0 (
    echo [flash] Flash complete.
) else (
    echo [flash] dfu-util exited with code %EXIT_CODE%.
    echo [flash] Is the board in DFU mode? Is dfu-util installed in WSL?
)
echo.
pause
exit /b %EXIT_CODE%

:wsl_cancelled
echo.
echo [flash] Cancelled. Binary is at: %BIN_FILE%
echo.
pause
exit /b 0