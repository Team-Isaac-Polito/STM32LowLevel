/**
 * @file debug.cpp
 * @brief USB CDC backend for the debug logging library.
 *
 * All debug output goes through USB CDC. Data is written into a 1024-byte
 * ring buffer in usbd_cdc_if.c; when full, oldest data is overwritten so
 * the most recent messages are always preserved. This means the serial
 * monitor will see the latest boot messages even if it connects after the
 * board has been running.
 *
 * _write syscall is overridden here (strong symbol) so that printf() and any
 * other newlib I/O function routes through USB CDC automatically.
 */

#include "Debug.h"

#include <cstdio>
#include <cstdarg>

// USB CDC interface
#include "usbd_cdc_if.h"

static Level sLevel = Level::LogDebug; //< Active verbosity level

static const char* levelPrefix(Level level)
{
    switch (level)
    {
        case Level::LogDebug:
            return "[DBG] ";
        case Level::LogInfo:
            return "[INF] ";
        case Level::LogWarn:
            return "[WRN] ";
        default:
            return "";
    }
}

void SerialDebug::putchar(char ch)
{
    uint8_t chU8 = static_cast<uint8_t>(ch);
    CDC_Transmit(&chU8, 1U);
}

void SerialDebug::setLevel(Level lvl)
{
    sLevel = lvl;
}

void SerialDebug::vlog(Level lvl, const char* fmt, va_list args)
{
    if (sLevel == Level::LogOff || lvl > sLevel)
        return;

    // Print level prefix
    const char* prefix = levelPrefix(lvl);
    while (*prefix)
        debug.putchar(*prefix++);

    // Format the message into a local buffer to avoid repeated putchar calls
    char buf[128];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);

    // Transmit the formatted string; clamp to buffer size on overflow
    int send = (n > 0 && n < (int)sizeof(buf)) ? n : (int)sizeof(buf) - 1;
    for (int i = 0; i < send; ++i)
        debug.putchar(buf[i]);
}

void SerialDebug::log(Level lvl, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vlog(lvl, fmt, args);
    va_end(args);
}

// _write syscall retarget — strong override of the CubeMX weak stub.
extern "C" int write(int file, char* ptr, int len)
{
    (void)file;
    for (int i = 0; i < len; ++i)
        debug.putchar(ptr[i]);
    return len;
}

// Creation of the default instance
SerialDebug debug;
