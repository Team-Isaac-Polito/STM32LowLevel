/**
 * @file IMU.h
 * @brief LSM6DSL 6-axis IMU driver for STM32G474 (I2C1, LL polling).
 *
 * Shared I2C1 bus with AS5048B (address 0x40). This driver uses address 0x6A.
 * I2C1 is initialised by MX_I2C1_Init() (400 kHz, PA15 SCL / PB7 SDA).
 *
 * Supports two orientation modes:
 *   - Accelerometer-only: call update(), then getPitch() / getRoll()
 *   - Complementary filter: call updateFused(), then getFusedPitch() / getFusedRoll()
 */

#ifndef IMU_H
#define IMU_H

#include <cstdint>
#include <cstdbool>

static constexpr uint8_t LSM6DSL_ADDR           = 0x6AU; //< Default 7-bit I2C address (SA0=GND)

// Register addresses (LSM6DSL datasheet Table 16)
static constexpr uint8_t LSM6DSL_WHO_AM_I        = 0x0FU; //< Device ID register (expected: 0x6A)
static constexpr uint8_t LSM6DSL_CTRL1_XL        = 0x10U; //< Accelerometer control 1
static constexpr uint8_t LSM6DSL_CTRL2_G         = 0x11U; //< Gyroscope control 2
static constexpr uint8_t LSM6DSL_OUTX_L_G        = 0x22U; //< Gyroscope X-axis output LSB
static constexpr uint8_t LSM6DSL_OUTX_L_XL       = 0x28U; //< Accelerometer X-axis output LSB

// Sensor sensitivity
static constexpr float LSM6DSL_SENSITIVITY_ACCEL = 0.061f;       //< mg/LSB at ±2 g FS
static constexpr float LSM6DSL_SENSITIVITY_GYRO  = 0.004375f;    //< dps/LSB at 125 dps FS
static constexpr float DEG_TO_RAD_F              = 0.017453293f; //< π / 180

// Calibration sample count
static constexpr int CALIBRATION_DATA_SIZE = 1000;

// Complementary filter default blending coefficient
static constexpr float COMPLEMENTARY_FILTER_ALPHA = 0.98f;

/**
 * @brief Raw 3-axis sensor reading.
 */
struct SensorData {
    int16_t x;
    int16_t y;
    int16_t z;
};

class IMU {
public:
    /**
     * @brief Configure and activate the LSM6DSL. Call once after MX_I2C1_Init().
     * @param addr 7-bit I2C address (default 0x6A).
     */
    void begin(uint8_t addr = LSM6DSL_ADDR);

    /**
     * @brief Read WHO_AM_I register and verify device identity.
     * @return true if the register returns 0x6A.
     */
    bool checkID();

    /**
     * @brief Enable the accelerometer (ODR = 104 Hz, FS = ±2 g, BW = 50 Hz).
     */
    void enableAccel();

    /**
     * @brief Enable the gyroscope (ODR = 104 Hz, FS = 125 dps).
     */
    void enableGyro();

    /**
     * @brief Collect calibration offsets for the accelerometer.
     *
     * Averages CALIBRATION_DATA_SIZE samples. Assumes the sensor is flat and
     * stationary (gravity on Z-axis). Must be called after enableAccel().
     */
    void calibrateAccel();

    /**
     * @brief Collect calibration offsets for the gyroscope.
     *
     * Averages CALIBRATION_DATA_SIZE samples at rest. Must be called after enableGyro().
     */
    void calibrateGyro();

    /**
     * @brief Read calibrated accelerometer data.
     * @param[out] data Raw counts with offsets subtracted.
     */
    void readAccel(SensorData &data);

    /**
     * @brief Read calibrated gyroscope data.
     * @param[out] data Raw counts with offsets subtracted.
     */
    void readGyro(SensorData &data);

    /**
     * @brief Compute accel-only pitch and roll. Call before getPitch() / getRoll().
     */
    void update();

    /**
     * @brief Get the last accel-only pitch angle (radians).
     */
    float getPitch();

    /**
     * @brief Get the last accel-only roll angle (radians).
     */
    float getRoll();

    /**
     * @brief Set the complementary filter blending coefficient.
     * @param alpha Blend factor (0–1). Higher = more gyro trust.
     */
    void setAlpha(float alpha);

    /**
     * @brief Fuse accelerometer and gyroscope with complementary filter.
     *
     * Must call after both enableAccel(), calibrateAccel(), enableGyro(), calibrateGyro().
     * Uses HAL_GetTick() for dt estimation.
     */
    void updateFused();

    /**
     * @brief Get the complementary-filter pitch angle (radians).
     */
    float getFusedPitch();

    /**
     * @brief Get the complementary-filter roll angle (radians).
     */
    float getFusedRoll();

private:
    uint8_t _addr = LSM6DSL_ADDR;  //< I2C address of the sensor

    int16_t _offsetAccelX = 0;  //< Accelerometer X-axis offset (raw counts)
    int16_t _offsetAccelY = 0;  //< Accelerometer Y-axis offset (raw counts)
    int16_t _offsetAccelZ = 0;  //< Accelerometer Z-axis offset (raw counts)
    int16_t _offsetGyroX  = 0;  //< Gyroscope X-axis offset (raw counts)
    int16_t _offsetGyroY  = 0;  //< Gyroscope Y-axis offset (raw counts)
    int16_t _offsetGyroZ  = 0;  //< Gyroscope Z-axis offset (raw counts)

    float _cachedPitch = 0.0f;  //< Last computed accel-only pitch (radians)
    float _cachedRoll  = 0.0f;  //< Last computed accel-only roll (radians)

    float _alpha            = COMPLEMENTARY_FILTER_ALPHA; //< Complementary filter blending coefficient
    float _fusedPitch       = 0.0f;  //< Last computed fused pitch (radians)
    float _fusedRoll        = 0.0f;  //< Last computed fused roll (radians)
    bool  _fusedInitialized = false; //< Indicates if the fused angles have been initialized
    uint32_t _lastFusedTick = 0U;    //< Timestamp of the last fused update (ms)

    /**
     * @brief Write one byte to register @p reg.
     */
    bool writeRegister(uint8_t reg, uint8_t value);

    /**
     * @brief Read one byte from register @p reg.
     */
    bool readRegister(uint8_t reg, uint8_t &out);

    /**
     * @brief Read @p length consecutive bytes starting at @p reg.
     */
    bool readRegisters(uint8_t reg, uint8_t *buffer, uint8_t length);

    /**
     * @brief Send device address + register byte; leaves bus in TC state for restart.
     */
    bool i2cWriteByte(uint8_t reg);

    /**
     * @brief Repeated-start read of @p len bytes. Must follow i2cWriteByte().
     */
    bool i2cReadBytes(uint8_t *buf, uint8_t len);

    /**
     * @brief Write @p reg and @p val in a single I2C write transaction.
     */
    bool i2cWriteRegByte(uint8_t reg, uint8_t val);
};

#endif // IMU_H
