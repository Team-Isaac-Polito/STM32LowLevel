/**
 * @file AbsoluteEncoder.h
 * @brief AS5048B magnetic absolute encoder driver for STM32G474 (I2C1, LL polling).
 *
 * Reads the 14-bit angle register pair (0xFE / 0xFF) from an AS5048B sensor.
 * I2C1 is initialised by MX_I2C1_Init() (400 kHz, PA15 SCL / PB7 SDA).
 *
 * Default sensor address: 0x40 (A1 = A2 = GND).
 */

#ifndef ABSOLUTE_ENCODER_H
#define ABSOLUTE_ENCODER_H

#include <cstdint>

static constexpr uint8_t  AS5048B_DEFAULT_ADDR = 0x40U;     /**< 7-bit I2C address (A1=A2=GND) */
static constexpr uint8_t  AS5048B_ANGLMSB_REG  = 0xFEU;     /**< Angle MSB register (bits [13:6]) */
static constexpr uint8_t  AS5048B_ANGLLSB_REG  = 0xFFU;     /**< Angle LSB register (bits  [5:0]) */
static constexpr uint8_t  AS5048B_ZEROMSB_REG  = 0x16U;     /**< Zero-position MSB register */
static constexpr uint8_t  AS5048B_ZEROLSB_REG  = 0x17U;     /**< Zero-position LSB register */
static constexpr uint16_t AS5048B_RESOLUTION   = 16384U;    /**< Full-scale 14-bit count (2^14) */
static constexpr float    ENCODER_READ_ERROR    = -1000.0f; /**< Sentinel returned on I2C failure */

class AbsoluteEncoder {
public:
    /**
     * @brief Construct the encoder driver.
     * @param addr 7-bit I2C address of the AS5048B.
     */
    explicit AbsoluteEncoder(uint8_t addr = AS5048B_DEFAULT_ADDR);

    /**
     * @brief Store the current position as the zero reference.
     *
     * Writes the current angle into the AS5048B zero-position registers
     * so subsequent reads return 0° at this mechanical position.
     */
    void setZero();

    /**
     * @brief Read the absolute angle in degrees, range (-180, 180].
     * Clockwise positive 
     *
     * @return Angle in degrees, or ENCODER_READ_ERROR on I2C failure.
     */
    float readAngle();

    /**
     * @brief Read the raw 14-bit encoder count (0–16383).
     * @return Raw count, or ENCODER_READ_ERROR on I2C failure.
     */
    float readRaw();

private:
    uint8_t _addr;  //< I2C address of the AS5048B sensor

    /**
     * @brief Read one byte from register @p reg.
     */
    bool readReg8(uint8_t reg, uint8_t &out);

    /**
     * @brief Read two consecutive bytes at @p reg and assemble a 14-bit value.
     */
    bool readReg16(uint8_t reg, uint16_t &out);

    /**
     * @brief Write one byte to register @p reg.
     */
    bool writeReg(uint8_t reg, uint8_t val);

    /**
     * @brief Send device address + register byte; leaves bus in TC state for restart.
     */
    bool i2cWriteByte(uint8_t devAddr, uint8_t reg);

    /**
     * @brief Repeated-start read of @p len bytes. Must follow i2cWriteByte().
     */
    bool i2cReadBytes(uint8_t devAddr, uint8_t *buf, uint8_t len);

    /**
     * @brief Write @p reg and @p val in a single I2C write transaction.
     */
    bool i2cWriteRegByte(uint8_t devAddr, uint8_t reg, uint8_t val);
};

#endif // ABSOLUTE_ENCODER_H
