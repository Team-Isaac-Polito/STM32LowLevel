#include "Battery.h"

#include "stm32g4xx_ll_adc.h"
#include "definitions.h"

static constexpr uint32_t ADC_TIMEOUT_LOOPS =
    34000U * 10U;                                //< Timeout for ADC end-of-conversion poll (roughly 10 ms at 170 MHz)
static constexpr float ADC_VREF = 3.3f;          //< ADC reference voltage in volts (connected to VDD, 3.3 V)
static constexpr float ADC_FULL_SCALE = 4095.0f; //< ADC full-scale count for 12-bit resolution (2^12 - 1)

void Battery::begin()
{
    // MX_ADC1_Init() has already enabled the internal regulator and set up
    // the channel. Run the self-calibration sequence, then enable ADC1.
    LL_ADC_StartCalibration(ADC1, LL_ADC_SINGLE_ENDED);
    uint32_t count = ADC_TIMEOUT_LOOPS;
    while (LL_ADC_IsCalibrationOnGoing(ADC1))
        if (--count == 0U)
            return;
    // ADEN must not be set during the 4 ADC clock cycles immediately after ADCAL is cleared.
    volatile uint32_t delayCalib = (LL_ADC_DELAY_CALIB_ENABLE_ADC_CYCLES >> 1U);
    while (delayCalib != 0U)
        delayCalib--;
    LL_ADC_Enable(ADC1);
    count = ADC_TIMEOUT_LOOPS;
    while (!LL_ADC_IsActiveFlag_ADRDY(ADC1))
        if (--count == 0U)
            return;
}

float Battery::readVoltage()
{
    // Trigger a single conversion
    LL_ADC_REG_StartConversion(ADC1);

    // Wait for end of conversion (EOC flag)
    uint32_t count = ADC_TIMEOUT_LOOPS;
    while (!LL_ADC_IsActiveFlag_EOC(ADC1))
        if (--count == 0U)
            return -1.0f;

    // Read result (clears EOC flag automatically in single-conversion mode)
    uint32_t raw = LL_ADC_REG_ReadConversionData12(ADC1);

    // Convert: Vadc = raw * Vref / 4095; Vbat = Vadc * BAT_SCALE_FACTOR
    float vadc = (static_cast<float>(raw) / ADC_FULL_SCALE) * ADC_VREF;
    return vadc * BAT_SCALE_FACTOR;
}

float Battery::chargePercent()
{
    float v = readVoltage();
    if (v < 0.0f)
        return -1.0f;

    float pct = (v - BAT_LOW) / (BAT_NOM - BAT_LOW) * 100.0f;
    if (pct < 0.0f)
        pct = 0.0f;
    if (pct > 100.0f)
        pct = 100.0f;
    return pct;
}

bool Battery::charged()
{
    float v = readVoltage();
    return (v >= 0.0f) && (v > BAT_LOW);
}
