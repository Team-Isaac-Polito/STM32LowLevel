#include "AbsoluteEncoder.h"

#include "stm32g4xx_ll_i2c.h"
#include "stm32g4xx_ll_rcc.h"
#include "stm32g4xx_ll_bus.h"

// I2C timeout in milliseconds (polling loops)
static constexpr uint32_t I2C_TIMEOUT_MS = 10U;

// Rough busy-wait loop count per millisecond at 170 MHz (conservative)
// 170e6 / 1000 / 5 (cycles per iteration) = 34000
static constexpr uint32_t I2C_LOOPS_PER_MS = 34000U;

AbsoluteEncoder::AbsoluteEncoder(uint8_t addr)
    : _addr(addr)
{}

void AbsoluteEncoder::setZero()
{
    // Write 0 first, then read current angle and write it as the new zero.
    //   1. zeroRegW(0)
    //   2. newZero = readReg16(ANGLMSB_REG)
    //   3. zeroRegW(newZero)
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
    uint16_t raw = 0;
    if (!readReg16(AS5048B_ANGLMSB_REG, raw))
        return ENCODER_READ_ERROR;

    // Clockwise: invert the raw value
    raw = static_cast<uint16_t>((AS5048B_RESOLUTION - 1U) - raw);

    // Convert to degrees in [0, 360)
    float angle = (static_cast<float>(raw) / static_cast<float>(AS5048B_RESOLUTION)) * 360.0f;

    // Remap to (-180, 180]
    if (angle > 180.0f)
        angle -= 360.0f;

    return angle;
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
    if (!i2cWriteByte(_addr, reg))
        return false;
    return i2cReadBytes(_addr, &out, 1U);
}

bool AbsoluteEncoder::readReg16(uint8_t reg, uint16_t& out)
{
    // Write register pointer then read two consecutive bytes.
    // Frame: [addr W][reg] RESTART [addr R][MSB][LSB]
    // AS5048B: angle MSB register is 0xFE, LSB is 0xFF (sequential).
    // readArray[0] = MSB (bits[13:6]), readArray[1] = LSB (bits[5:0])
    // Combined: (MSB << 6) | (LSB & 0x3F)  — identical to ams_as5048b
    if (!i2cWriteByte(_addr, reg))
        return false;

    uint8_t buf[2] = {};
    if (!i2cReadBytes(_addr, buf, 2U))
        return false;

    out = static_cast<uint16_t>(static_cast<uint16_t>(buf[0]) << 6) | static_cast<uint16_t>(buf[1] & 0x3FU);
    return true;
}

bool AbsoluteEncoder::writeReg(uint8_t reg, uint8_t val)
{
    return i2cWriteRegByte(_addr, reg, val);
}

static inline bool waitFlag(volatile uint32_t* reg, uint32_t mask, uint32_t polarity)
{
    uint32_t count = I2C_TIMEOUT_MS * I2C_LOOPS_PER_MS;
    while (((*reg & mask) == polarity) == false)
        if (--count == 0U)
            return false;
    return true;
}

static inline bool waitSet(volatile uint32_t* reg, uint32_t mask)
{
    uint32_t count = I2C_TIMEOUT_MS * I2C_LOOPS_PER_MS;
    while (!(*reg & mask))
        if (--count == 0U)
            return false;
    return true;
}

static inline bool waitClear(volatile uint32_t* reg, uint32_t mask)
{
    uint32_t count = I2C_TIMEOUT_MS * I2C_LOOPS_PER_MS;
    while (*reg & mask)
        if (--count == 0U)
            return false;
    return true;
}

bool AbsoluteEncoder::i2cWriteByte(uint8_t devAddr, uint8_t reg)
{
    // Wait until bus is not busy
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
        return false;

    // Configure transfer: 7-bit address, 1 byte, software end (we'll do restart)
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(devAddr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          1U,
                          LL_I2C_MODE_SOFTEND,
                          LL_I2C_GENERATE_START_WRITE);

    // Wait for TXIS (transmit register empty)
    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
        return false;
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        return false;
    }

    // Write register byte
    I2C1->TXDR = reg;

    // Wait for TC (transfer complete — SOFTEND mode)
    if (!waitSet(&I2C1->ISR, I2C_ISR_TC))
        return false;

    return true;
}

bool AbsoluteEncoder::i2cReadBytes(uint8_t devAddr, uint8_t* buf, uint8_t len)
{
    // Issue repeated start read transfer (AUTOEND so STOP is generated automatically)
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(devAddr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          len,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_READ);

    for (uint8_t i = 0U; i < len; i++)
    {
        // Wait for RXNE (receive data register not empty)
        if (!waitSet(&I2C1->ISR, I2C_ISR_RXNE))
            return false;
        if (I2C1->ISR & I2C_ISR_NACKF)
        {
            I2C1->ICR = I2C_ICR_NACKCF;
            return false;
        }
        buf[i] = static_cast<uint8_t>(I2C1->RXDR);
    }

    // Wait for STOPF (AUTOEND generates STOP automatically)
    if (!waitSet(&I2C1->ISR, I2C_ISR_STOPF))
        return false;
    I2C1->ICR = I2C_ICR_STOPCF; // Clear STOPF flag

    return true;
}

bool AbsoluteEncoder::i2cWriteRegByte(uint8_t devAddr, uint8_t reg, uint8_t val)
{
    // Wait until bus is not busy
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
        return false;

    // 2 bytes: register address + value, AUTOEND → auto STOP
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(devAddr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          2U,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);

    // Send register address
    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
        return false;
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        return false;
    }
    I2C1->TXDR = reg;

    // Send value
    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
        return false;
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        return false;
    }
    I2C1->TXDR = val;

    // Wait for STOPF
    if (!waitSet(&I2C1->ISR, I2C_ISR_STOPF))
        return false;
    I2C1->ICR = I2C_ICR_STOPCF;

    return true;
}
