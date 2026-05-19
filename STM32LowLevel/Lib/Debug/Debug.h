/**
 * @file Debug.h
 * @brief Serial debug logger for STM32LowLevel (USB CDC backend).
 *
 * All debug output goes through USB CDC. The CDC TX ring buffer (1024 bytes)
 * preserves recent messages even if the host hasn't connected yet.
 *
 * Usage:
 *   Debug.setLevel(Level::LOG_INFO);
 *   Debug.log(Level::LOG_INFO, "voltage: %.2f V\n", voltage);
 *   printf("also routes through USB CDC\n");  // via _write retarget
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <cstdint>
#include <cstdarg>

/**
 * @brief Verbosity levels (ordered: LOG_OFF < LOG_WARN < LOG_INFO < LOG_DEBUG).
 *
 * Only messages whose level is <= the active level are printed.
 */
enum class Level : uint8_t
{
    LogOff = 0,
    LogWarn = 1,
    LogInfo = 2,
    LogDebug = 3,
};

/**
 * @brief Serial debug logger backed by USB CDC.
 *
 * Call Debug.setLevel() once at startup, then use Debug.log() throughout.
 * printf() is also routed through USB CDC via the _write syscall retarget.
 */
class SerialDebug
{
  public:
    /**
     * @brief Set the active log level. Messages above this level are suppressed.
     * @param lvl New active level (Level::LOG_OFF suppresses all output).
     */
    void setLevel(Level lvl);

    /**
     * @brief Write a formatted message via USB CDC if lvl <= active level.
     * @param lvl Verbosity of this message.
     * @param fmt printf-style format string.
     * @param ... Format arguments.
     */
    void log(Level lvl, const char* fmt, ...);

    /**
     * @brief va_list variant of log().
     * @param lvl  Verbosity of this message.
     * @param fmt  printf-style format string.
     * @param args Pre-initialized va_list.
     */
    void vlog(Level lvl, const char* fmt, va_list args);

    /**
     * @brief Transmit a single character over USB CDC (non-blocking, buffered).
     * @param ch Character to send.
     */
    void putchar(char ch);
};

extern SerialDebug debug;

#endif // DEBUG_H
