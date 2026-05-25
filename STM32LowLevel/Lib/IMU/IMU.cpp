#include "IMU.h"

#include <cmath>
#include <cstring>

#include "stm32g4xx_ll_i2c.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_hal.h"

// Timeout in busy-wait loops (~10 ms at 170 MHz)
static constexpr uint32_t I2C_TIMEOUT_LOOPS = 340000U;

/**
 * @brief Recover a stuck I2C bus by toggling SCL clock line.
 *
 * If SDA is held low by a device, we clock SCL to let the device
 * finish its transfer. This is the standard I2C bus recovery procedure.
 */
static void i2cBusRecovery(void)
{
    // Configure PA15 (SCL) as GPIO output open-drain
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_15, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_15, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_15, LL_GPIO_SPEED_FREQ_HIGH);

    // Toggle SCL up to 9 times to clock out any stuck device
    for (int i = 0; i < 9; i++)
    {
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_15);
        for (volatile uint32_t d = 0; d < 1000; d++)
            ;
        LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_15);
        for (volatile uint32_t d = 0; d < 1000; d++)
            ;

        // Check if SDA (PB7) has been released
        if (LL_GPIO_IsInputPinSet(GPIOB, LL_GPIO_PIN_7))
            break;
    }

    // Generate a STOP condition: SCL high, then SDA high
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_15);
    for (volatile uint32_t d = 0; d < 1000; d++)
        ;
    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_7);
    for (volatile uint32_t d = 0; d < 1000; d++)
        ;

    // Reconfigure PA15 back to I2C alternate function
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_15, LL_GPIO_MODE_ALTERNATE);
    LL_GPIO_SetPinOutputType(GPIOA, LL_GPIO_PIN_15, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinSpeed(GPIOA, LL_GPIO_PIN_15, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_15, LL_GPIO_AF_4);

    // Small delay for bus to settle
    for (volatile uint32_t d = 0; d < 5000; d++)
        ;
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
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF | I2C_ICR_PECCF |
                I2C_ICR_TIMOUTCF | I2C_ICR_ALERTCF;
}

/**
 * @brief Robust I2C write-then-read (repeated start).
 * Mimics Arduino Wire: beginTransmission(addr); write(reg); endTransmission(false); requestFrom(addr, len);
 * On any error, performs bus recovery.
 */
static bool i2cReadReg(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
    {
        i2cBusRecovery();
        if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
            return false;
    }

    i2cClearErrors();

    // Phase 1: Write register address (SOFTEND = no STOP, repeated start follows)
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(addr) << 1U,
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

    // Phase 2: Read data (AUTOEND = generates STOP after last byte)
    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(addr) << 1U,
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
 * @brief Robust I2C write register + value.
 */
static bool i2cWriteReg(uint8_t addr, uint8_t reg, uint8_t val)
{
    if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
    {
        i2cBusRecovery();
        if (!waitClear(&I2C1->ISR, I2C_ISR_BUSY))
            return false;
    }

    i2cClearErrors();

    LL_I2C_HandleTransfer(I2C1,
                          static_cast<uint32_t>(addr) << 1U,
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

bool IMU::writeRegister(uint8_t reg, uint8_t value)
{
    return i2cWriteReg(_addr, reg, value);
}

bool IMU::readRegister(uint8_t reg, uint8_t& out)
{
    return i2cReadReg(_addr, reg, &out, 1U);
}

bool IMU::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length)
{
    return i2cReadReg(_addr, reg, buffer, length);
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
    return (id == _addr);
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
