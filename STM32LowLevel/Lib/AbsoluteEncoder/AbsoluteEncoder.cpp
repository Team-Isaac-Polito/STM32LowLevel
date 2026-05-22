#include "AbsoluteEncoder.h"

#include "stm32g4xx_ll_i2c.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"
#include "stm32g4xx_ll_gpio.h"

// Timeout in busy-wait loops (~10 ms at 170 MHz)
static constexpr uint32_t I2C_TIMEOUT_LOOPS = 340000U;

/**
 * @brief Recover a stuck I2C bus by toggling SCL clock line.
 * Standard I2C bus recovery: clock SCL to let stuck devices finish.
 */
static void i2cBusRecovery(void)
{
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_15, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_15, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_15, LL_GPIO_SPEED_FREQ_HIGH);

    for (int i = 0; i < 9; i++)
    {
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_15);
        for (volatile uint32_t d = 0; d < 1000; d++);
        LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_15);
        for (volatile uint32_t d = 0; d < 1000; d++);
        if (LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_7))
            break;
    }

    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_15);
    for (volatile uint32_t d = 0; d < 1000; d++);
    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_7);
    for (volatile uint32_t d = 0; d < 1000; d++);

    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_15, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_15, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_15, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_15, LL_GPIO_AF_4);

    for (volatile uint32_t d = 0; d < 5000; d++);
}

static inline bool waitSet(volatile uint32_t* reg, uint32_t mask)
{
    uint32_t count = I2C_TIMEOUT_LOOPS;
    while (!(*reg & mask))
        if (--count == 0U)
            return false;
    return true;
}

static inline bool waitClear(volatile uint32_t* reg, uint32_t mask)
{
    uint32_t count = I2C_TIMEOUT_LOOPS;
    while (*reg & mask)
        if (--count == 0U)
            return false;
    return true;
}

static inline void i2cClearErrors(void)
{
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF |
                I2C_ICR_ARLOCF | I2C_ICR_OVRCF | I2C_ICR_PECCF |
                I2C_ICR_TIMOUTCF | I2C_ICR_ALERTCF;
}

/**
 * @brief Robust I2C write-then-read (repeated start) for encoder.
 */
static bool i2cReadReg(uint8_t devAddr, uint8_t reg, uint8_t* buf, uint8_t len)
{
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
    {
        i2cBusRecovery();
        if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
            return false;
    }

    i2cClearErrors();

    // Phase 1: Write register address (SOFTEND)
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(devAddr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          1U,
                          LL_I2C_MODE_SOFTEND,
                          LL_I2C_GENERATE_START_WRITE);

    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
    {
        i2cBusRecovery();
        return false;
    }

    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        LL_I2C_GenerateStopCondition(I2C1);
        waitSet(&I2C1->ISR, I2C_ISR_STOPF);
        I2C1->ICR = I2C_ICR_STOPCF;
        return false;
    }

    I2C1->TXDR = reg;

    if (!waitSet(&I2C1->ISR, I2C_ISR_TC))
    {
        i2cBusRecovery();
        return false;
    }

    // Phase 2: Read data (AUTOEND)
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(devAddr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          len,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_READ);

    for (uint8_t i = 0U; i < len; i++)
    {
        if (!waitSet(&I2C1->ISR, I2C_ISR_RXNE))
        {
            i2cBusRecovery();
            return false;
        }
        buf[i] = static_cast<uint8_t>(I2C1->RXDR);
    }

    if (!waitSet(&I2C1->ISR, I2C_ISR_STOPF))
    {
        i2cBusRecovery();
        return false;
    }

    I2C1->ICR = I2C_ICR_STOPCF;
    return true;
}

/**
 * @brief Robust I2C write register + value for encoder.
 */
static bool i2cWriteReg(uint8_t devAddr, uint8_t reg, uint8_t val)
{
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
    {
        i2cBusRecovery();
        if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
            return false;
    }

    i2cClearErrors();

    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(devAddr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          2U,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
    {
        i2cBusRecovery();
        return false;
    }

    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        LL_I2C_GenerateStopCondition(I2C1);
        waitSet(&I2C1->ISR, I2C_ISR_STOPF);
        I2C1->ICR = I2C_ICR_STOPCF;
        return false;
    }

    I2C1->TXDR = reg;

    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
    {
        i2cBusRecovery();
        return false;
    }

    I2C1->TXDR = val;

    if (!waitSet(&I2C1->ISR, I2C_ISR_STOPF))
    {
        i2cBusRecovery();
        return false;
    }

    I2C1->ICR = I2C_ICR_STOPCF;
    return true;
}

// ======================== Public Methods ========================

AbsoluteEncoder::AbsoluteEncoder(uint8_t addr)
    : _addr(addr)
{}

void AbsoluteEncoder::setZero()
{
    writeReg(AS5048B_ZEROMSB_REG, 0x00U);
    writeReg(AS5048B_ZEROLSB_REG, 0x00U);

    uint16_t raw = 0;
    if (!readReg16(AS5048B_ANGLMSB_REG, raw))
        return;

    writeReg(AS5048B_ZEROMSB_REG, static_cast<uint8_t>(raw >> 6));
    writeReg(AS5048B_ZEROLSB_REG, static_cast<uint8_t>(raw & 0x3FU));
}

float AbsoluteEncoder::readAngle()
{
    const uint8_t maxRetries = 5;
    uint16_t raw = 0;

    for (uint8_t retry = 0; retry < maxRetries; retry++)
    {
        if (readReg16(AS5048B_ANGLMSB_REG, raw))
        {
            // Clockwise: invert the raw value
            raw = static_cast<uint16_t>((AS5048B_RESOLUTION - 1U) - raw);

            // Convert to degrees in [0, 360)
            float angle = (static_cast<float>(raw) / static_cast<float>(AS5048B_RESOLUTION)) * 360.0f;

            // Remap to (-180, 180]
            if (angle > 180.0f)
                angle -= 360.0f;

            return angle;
        }
    }

    return ENCODER_READ_ERROR;
}

float AbsoluteEncoder::readRaw()
{
    uint16_t raw = 0;
    if (!readReg16(AS5048B_ANGLMSB_REG, raw))
        return ENCODER_READ_ERROR;
    return static_cast<float>(raw);
}

bool AbsoluteEncoder::readReg8(uint8_t reg, uint8_t& out)
{
    return i2cReadReg(_addr, reg, &out, 1U);
}

bool AbsoluteEncoder::readReg16(uint8_t reg, uint16_t& out)
{
    uint8_t buf[2] = {};
    if (!i2cReadReg(_addr, reg, buf, 2U))
        return false;

    out = static_cast<uint16_t>(static_cast<uint16_t>(buf[0]) << 6) | static_cast<uint16_t>(buf[1] & 0x3FU);
    return true;
}

bool AbsoluteEncoder::writeReg(uint8_t reg, uint8_t val)
{
    return i2cWriteReg(_addr, reg, val);
}
