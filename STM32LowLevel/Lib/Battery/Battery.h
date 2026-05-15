/**
 * @file Battery.h
 * @brief Battery voltage monitor for STM32G474 (ADC1 IN1, PA0 — VBAT_SENSE).
 *
 * ADC1 is initialised by MX_ADC1_Init() (LL, 12-bit, software-triggered,
 * single conversion). This driver triggers conversions and reads results via
 * LL registers directly, without HAL.
 *
 * Voltage divider: R_TOP = 100 kΩ, R_BOT = 27 kΩ.
 * Vref = 3.3 V, ADC full-scale = 4095 counts.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <cstdint>
#include <cstdbool>

class Battery
{
  public:
    /**
     * @brief Calibrate and enable ADC1. Call once after MX_ADC1_Init().
     */
    void begin();

    /**
     * @brief Trigger a single ADC conversion and return the battery voltage.
     *
     * Applies the voltage divider scale factor (BAT_SCALE_FACTOR ≈ 4.704).
     *
     * @return Battery voltage in volts, or -1.0f on ADC timeout.
     */
    float readVoltage();

    /**
     * @brief Estimate remaining charge as a percentage.
     *
     * Linearly maps [BAT_LOW, BAT_NOM] to [0%, 100%], clamped.
     *
     * @return Charge percentage (0–100), or -1.0f on ADC timeout.
     */
    float chargePercent();

    /**
     * @brief Check whether the battery voltage is above the low-voltage threshold.
     * @return true if voltage > BAT_LOW.
     */
    bool charged();
};

#endif // BATTERY_H
