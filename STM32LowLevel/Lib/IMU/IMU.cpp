#include "IMU.h"

#include <cmath>
#include <cstring>

#include "stm32g4xx_ll_i2c.h"
#include "stm32g4xx_hal.h"

// Timeout in busy-wait loops (~10 ms at 170 MHz, 34000 loops/ms)
static constexpr uint32_t I2C_TIMEOUT_LOOPS = 34000U * 10U;

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

bool IMU::i2cWriteByte(uint8_t reg)
{
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
        return false;
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(_addr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          1U,
                          LL_I2C_MODE_SOFTEND,
                          LL_I2C_GENERATE_START_WRITE);
    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
        return false;
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        return false;
    }
    I2C1->TXDR = reg;
    if (!waitSet(&I2C1->ISR, I2C_ISR_TC))
        return false;
    return true;
}

bool IMU::i2cReadBytes(uint8_t* buf, uint8_t len)
{
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(_addr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          len,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_READ);
    for (uint8_t i = 0U; i < len; i++)
    {
        if (!waitSet(&I2C1->ISR, I2C_ISR_RXNE))
            return false;
        if (I2C1->ISR & I2C_ISR_NACKF)
        {
            I2C1->ICR = I2C_ICR_NACKCF;
            return false;
        }
        buf[i] = static_cast<uint8_t>(I2C1->RXDR);
    }
    if (!waitSet(&I2C1->ISR, I2C_ISR_STOPF))
        return false;
    I2C1->ICR = I2C_ICR_STOPCF;
    return true;
}

bool IMU::i2cWriteRegByte(uint8_t reg, uint8_t val)
{
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
        return false;
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(_addr) << 1U,
                          LL_I2C_ADDRSLAVE_7BIT,
                          2U,
                          LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);
    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
        return false;
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        return false;
    }
    I2C1->TXDR = reg;
    if (!waitSet(&I2C1->ISR, I2C_ISR_TXIS))
        return false;
    if (I2C1->ISR & I2C_ISR_NACKF)
    {
        I2C1->ICR = I2C_ICR_NACKCF;
        return false;
    }
    I2C1->TXDR = val;
    if (!waitSet(&I2C1->ISR, I2C_ISR_STOPF))
        return false;
    I2C1->ICR = I2C_ICR_STOPCF;
    return true;
}

bool IMU::writeRegister(uint8_t reg, uint8_t value)
{
    return i2cWriteRegByte(reg, value);
}

bool IMU::readRegister(uint8_t reg, uint8_t& out)
{
    if (!i2cWriteByte(reg))
        return false;
    return i2cReadBytes(&out, 1U);
}

bool IMU::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length)
{
    if (!i2cWriteByte(reg))
        return false;
    return i2cReadBytes(buffer, length);
}

void IMU::begin(uint8_t addr)
{
    _addr = addr;
    // MX_I2C1_Init() already called from main.c; no further init needed.
}

bool IMU::checkID()
{
    uint8_t id = 0;
    if (!readRegister(LSM6DSL_WHO_AM_I, id))
        return false;
    return (id == 0x6AU);
}

void IMU::enableAccel()
{
    // CTRL1_XL: ODR=104 Hz, FS=±2 g, BW=50 Hz → 0x43
    writeRegister(LSM6DSL_CTRL1_XL, 0x43U);
}

void IMU::calibrateAccel()
{
    int32_t sumX = 0, sumY = 0, sumZ = 0;
    uint8_t buf[6];
    for (int i = 0; i < CALIBRATION_DATA_SIZE; i++)
    {
        readRegisters(LSM6DSL_OUTX_L_XL, buf, 6U);
        sumX += static_cast<int16_t>(static_cast<uint16_t>(buf[1]) << 8 | buf[0]);
        sumY += static_cast<int16_t>(static_cast<uint16_t>(buf[3]) << 8 | buf[2]);
        sumZ += static_cast<int16_t>(static_cast<uint16_t>(buf[5]) << 8 | buf[4]);
    }
    _offsetAccelX = static_cast<int16_t>(sumX / CALIBRATION_DATA_SIZE);
    _offsetAccelY = static_cast<int16_t>(sumY / CALIBRATION_DATA_SIZE);
    _offsetAccelZ = static_cast<int16_t>(sumZ / CALIBRATION_DATA_SIZE) - 16384; // Z = +1g when flat
}

void IMU::readAccel(SensorData& data)
{
    uint8_t buf[6];
    readRegisters(LSM6DSL_OUTX_L_XL, buf, 6U);
    data.x = static_cast<int16_t>(static_cast<uint16_t>(buf[1]) << 8 | buf[0]) - _offsetAccelX;
    data.y = static_cast<int16_t>(static_cast<uint16_t>(buf[3]) << 8 | buf[2]) - _offsetAccelY;
    data.z = static_cast<int16_t>(static_cast<uint16_t>(buf[5]) << 8 | buf[4]) - _offsetAccelZ;
}

void IMU::enableGyro()
{
    // CTRL2_G: ODR=104 Hz, FS=125 dps → 0x42
    writeRegister(LSM6DSL_CTRL2_G, 0x42U);
}

void IMU::calibrateGyro()
{
    int32_t sumX = 0, sumY = 0, sumZ = 0;
    uint8_t buf[6];
    for (int i = 0; i < CALIBRATION_DATA_SIZE; i++)
    {
        readRegisters(LSM6DSL_OUTX_L_G, buf, 6U);
        sumX += static_cast<int16_t>(static_cast<uint16_t>(buf[1]) << 8 | buf[0]);
        sumY += static_cast<int16_t>(static_cast<uint16_t>(buf[3]) << 8 | buf[2]);
        sumZ += static_cast<int16_t>(static_cast<uint16_t>(buf[5]) << 8 | buf[4]);
    }
    _offsetGyroX = static_cast<int16_t>(sumX / CALIBRATION_DATA_SIZE);
    _offsetGyroY = static_cast<int16_t>(sumY / CALIBRATION_DATA_SIZE);
    _offsetGyroZ = static_cast<int16_t>(sumZ / CALIBRATION_DATA_SIZE);
}

void IMU::readGyro(SensorData& data)
{
    uint8_t buf[6];
    readRegisters(LSM6DSL_OUTX_L_G, buf, 6U);
    data.x = static_cast<int16_t>(static_cast<uint16_t>(buf[1]) << 8 | buf[0]) - _offsetGyroX;
    data.y = static_cast<int16_t>(static_cast<uint16_t>(buf[3]) << 8 | buf[2]) - _offsetGyroY;
    data.z = static_cast<int16_t>(static_cast<uint16_t>(buf[5]) << 8 | buf[4]) - _offsetGyroZ;
}

void IMU::update()
{
    SensorData accel;
    readAccel(accel);

    float ax = accel.x * LSM6DSL_SENSITIVITY_ACCEL * 0.001f;
    float ay = accel.y * LSM6DSL_SENSITIVITY_ACCEL * 0.001f;
    float az = accel.z * LSM6DSL_SENSITIVITY_ACCEL * 0.001f;

    _cachedPitch = atan2f(ay, sqrtf(ax * ax + az * az));
    _cachedRoll = atan2f(ax, sqrtf(ay * ay + az * az));
}

float IMU::getPitch()
{
    return _cachedPitch;
}
float IMU::getRoll()
{
    return _cachedRoll;
}

void IMU::setAlpha(float alpha)
{
    _alpha = alpha;
}

void IMU::updateFused()
{
    SensorData accel, gyro;
    readAccel(accel);
    readGyro(gyro);

    float ax = accel.x * LSM6DSL_SENSITIVITY_ACCEL * 0.001f;
    float ay = accel.y * LSM6DSL_SENSITIVITY_ACCEL * 0.001f;
    float az = accel.z * LSM6DSL_SENSITIVITY_ACCEL * 0.001f;

    float accelPitch = atan2f(ax, sqrtf(ay * ay + az * az));
    float accelRoll = atan2f(ay, sqrtf(ax * ax + az * az));

    float gyroPitchRate = gyro.x * LSM6DSL_SENSITIVITY_GYRO * DEG_TO_RAD_F;
    float gyroRollRate = gyro.y * LSM6DSL_SENSITIVITY_GYRO * DEG_TO_RAD_F;

    uint32_t now = HAL_GetTick();

    if (!_fusedInitialized)
    {
        _fusedPitch = accelPitch;
        _fusedRoll = accelRoll;
        _lastFusedTick = now;
        _fusedInitialized = true;
        return;
    }

    float dt = static_cast<float>(now - _lastFusedTick) * 0.001f; // ms → s
    _lastFusedTick = now;

    if (dt <= 0.0f || dt > 1.0f)
        dt = 0.04f;

    _fusedPitch = _alpha * (_fusedPitch + gyroPitchRate * dt) + (1.0f - _alpha) * accelPitch;
    _fusedRoll = _alpha * (_fusedRoll + gyroRollRate * dt) + (1.0f - _alpha) * accelRoll;
}

float IMU::getFusedPitch()
{
    return _fusedPitch;
}
float IMU::getFusedRoll()
{
    return _fusedRoll;
}
