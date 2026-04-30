/**
 * @file Debug.h
 * @brief Serial debug logger for STM32LowLevel (UART5 LL backend).
 *
 * UART5 (PC12 TX / PD2 RX, 115200 baud) is the dedicated debug console.
 *
 * Usage:
 *   Debug.setLevel(Level::LOG_OFF);
 *   Debug.log(Level::LOG_INFO, "voltage: %.2f V\n", voltage);
 *   printf("also routes through UART5\n");  // via _write retarget
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
enum class Level : uint8_t {
    LOG_OFF   = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3,
};

/**
 * @brief Serial debug logger backed by UART5 (PC12 TX / PD2 RX, 115200 baud).
 *
 * Call Debug.setLevel() once at startup, then use Debug.log() throughout.
 * printf() is also routed through UART5 via the _write syscall retarget.
 */
class SerialDebug {
public:
    /**
     * @brief Set the active log level. Messages above this level are suppressed.
     * @param lvl New active level (Level::LOG_OFF suppresses all output).
     */
    void setLevel(Level lvl);

    /**
     * @brief Write a formatted message to UART5 if lvl <= active level.
     * @param lvl Verbosity of this message.
     * @param fmt printf-style format string.
     * @param ... Format arguments.
     */
    void log(Level lvl, const char *fmt, ...);

    /**
     * @brief va_list variant of log().
     * @param lvl  Verbosity of this message.
     * @param fmt  printf-style format string.
     * @param args Pre-initialized va_list.
     */
    void vlog(Level lvl, const char *fmt, va_list args);

    /**
     * @brief Transmit a single character over UART5 (blocking).
     * @param ch Character to send.
     */
    void putchar(char ch);
};

extern SerialDebug Debug;

#endif // DEBUG_H
