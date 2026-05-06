/**
 * @file DynamixelLL.h
 * @brief Low-level Dynamixel Protocol 2.0 driver for STM32G474 (USART2/3 LL backend).
 *
 * e-manual for DYNAMIXEL protocol 2.0: https://emanual.robotis.com/docs/en/dxl/protocol2/
 * e-Manual for DYNAMIXEL XL430-W250: https://emanual.robotis.com/docs/en/dxl/x/xl430-w250/
 * 
 * Physical layer:
 *   - DXL Bus 1: USART2, PA2 TX (open-drain), PA1 DE, 4.5 Mbps half-duplex
 *   - DXL Bus 2: USART3, PB10 TX (open-drain), PB14 DE, 4.5 Mbps half-duplex
 */

#ifndef DYNAMIXEL_LL_H
#define DYNAMIXEL_LL_H

#include <cstdint>

#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_crc.h"
#include "stm32g4xx_hal.h"  // HAL_GetTick() for timeouts
#include "Debug.h"


// Instruction codes
#define DXL_INST_PING          0x01u  //< Instruction that checks whether the Packet has arrived at a device with the same ID as the specified packet ID
#define DXL_INST_READ          0x02u  //< Instruction to read data from the Device
#define DXL_INST_WRITE         0x03u  //< Instruction to write data to the Device
#define DXL_INST_FACTORY_RESET 0x06u  //< Instruction that resets the Control Table to its initial factory default settings
#define DXL_INST_REBOOT        0x08u  //< Instruction to reboot the Device
#define DXL_INST_SYNC_WRITE    0x83u  //< Instruction to write data to multiple devices with the same Address with the same length at once
#define DXL_INST_SYNC_READ     0x82u  //< Instruction to read data from multiple devices with the same Address with the same length at once
#define DXL_INST_BULK_WRITE    0x93u  //< Instruction to write data to multiple devices with different Addresses with different lengths at once
#define DXL_INST_BULK_READ     0x92u  //< Instruction to read data from multiple devices with different Addresses with different lengths at once
#define DXL_STATUS_INST        0x55u  //< Return packet sent following the execution of an Instruction Packet

#define DXL_BROADCAST_ID       0xFEu  //< Broadcast ID
#define DXL_RX_TIMEOUT_MS      10u    //< Receive timeout in milliseconds
#define DXL_MAX_PACKET_SIZE    128u   //< Maximum receive buffer size (bytes)


/**
 * @brief Decoded status packet returned by a Dynamixel servo.
 */
struct DxlStatusPacket {
    bool    valid;        //< True if packet is valid and CRC passed
    uint8_t id;           //< Servo ID that sent the response
    uint8_t error;        //< Error byte from status packet
    uint8_t data[8];      //< Parameter bytes (up to 8)
    uint8_t dataLength;   //< Number of parameter bytes received
};

/**
 * @brief Velocity profile types.
 */
enum class DxlVelocityProfile : uint8_t {
    PROFILE_NOT_USED = 0,
    RECTANGULAR      = 1,
    TRIANGULAR       = 2,
    TRAPEZOIDAL      = 3,
};

/**
 * @brief Decoded moving-status register.
 */
struct DxlMovingStatus {
    uint8_t             raw;            //< Raw register byte (address 123).
    DxlVelocityProfile  profileType;    //< Active velocity profile type (bits [5:4]).
    bool                followingError; //< True if position following error threshold exceeded (bit 3).
    bool                profileOngoing; //< True if the velocity profile is still being executed (bit 1).
    bool                inPosition;     //< True if goal position has been reached (bit 0).
};


/**
 * @brief Low-level Dynamixel Protocol 2.0 driver.
 *
 * One instance per servo.  Multiple servos on the same bus share the same
 * USART peripheral — pass the same `usart` pointer.
 *
 * Sync/bulk operations broadcast across all servos in the sync group.
 *
 * Usage:
 *   // After MX_USART2_UART_Init():
 *   DynamixelLL motor(USART2, SERVO_TRACTION_R_ID);
 *   motor.begin();
 *   motor.setTorqueEnable(true);
 *   motor.setGoalVelocity_RPM(10.0f);
 */
class DynamixelLL {
public:
    /**
     * @brief Construct a DynamixelLL instance.
     * @param usart   USART peripheral (USART2 for DXL Bus 1, USART3 for Bus 2).
     * @param servoID Target servo ID (0–253).
     */
    DynamixelLL(USART_TypeDef *usart, uint8_t servoID);
    ~DynamixelLL();

    DynamixelLL(const DynamixelLL &) = delete;
    DynamixelLL &operator=(const DynamixelLL &) = delete;

    /**
     * @brief Enable RS-485 DE hardware control (DEM bit) and flush USART RX.
     *
     * Must be called once after MX_USARTx_UART_Init().
     */
    void begin();

    /**
     * @brief Enable synchronous (broadcast) mode for a group of servos.
     *
     * After calling this, sync write/read methods broadcast a single packet to
     * all `motorIDs` simultaneously instead of addressing `_servoID` alone.
     *
     * @param motorIDs  Array of servo IDs to include in the sync group.
     * @param numMotors Number of entries in `motorIDs` (must be ≥ 2).
     */
    void enableSync(const uint8_t *motorIDs, uint8_t numMotors);

    /**
     * @brief Disable synchronous mode and release the motor-ID array.
     *
     * After calling this, all methods address `_servoID` directly again.
     */
    void disableSync();

    /**
     * @brief Set the operating mode (control table address 11).
     *
     * @param mode Operating mode: 0=Current, 1=Velocity, 3=Position,
     *             4=ExtendedPosition, 5=CurrentBasedPosition, 16=PWM.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setOperatingMode(uint8_t mode);

    /**
     * @brief Set the homing offset in raw encoder counts (control table address 20).
     *
     * @param offset Homing offset in raw units (−1,044,479 to 1,044,479).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setHomingOffset(int32_t offset);

    /**
     * @brief Set the homing offset converted from degrees.
     *
     * Converts `offsetAngle` to raw encoder units using RAD_TO_DXL and sets
     * control table address 20.
     *
     * @param offsetAngle Homing offset angle in degrees.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setHomingOffset_A(float offsetAngle);

    /**
     * @brief Set goal position in Position Control Mode (control table address 116).
     *
     * @param goalPosition Target position in raw units (0–4095 for one full turn).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setGoalPosition_PCM(uint16_t goalPosition);

    /**
     * @brief Set goal position in degrees for Position Control Mode.
     *
     * Converts `angleDegrees` to raw position units and writes address 116.
     *
     * @param angleDegrees Target angle in degrees (0.0–360.0).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setGoalPosition_A_PCM(float angleDegrees);

    /**
     * @brief Set goal position in Extended Position Control Mode (control table address 116).
     *
     * Supports multi-turn positioning beyond one revolution.
     *
     * @param extendedPosition Target position in raw encoder counts.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setGoalPosition_EPCM(int32_t extendedPosition);

    /**
     * @brief Enable or disable motor torque (control table address 64).
     *
     * @param enable True to enable torque output, false to disable.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setTorqueEnable(bool enable);

    /**
     * @brief Turn the servo status LED on or off (control table address 65).
     *
     * @param enable True to turn the LED on, false to turn it off.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setLED(bool enable);

    /**
     * @brief Set the status return level (control table address 68).
     *
     * @param level 0 = no response, 1 = respond to READ only, 2 = respond to all.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setStatusReturnLevel(uint8_t level);

    /**
     * @brief Change the servo ID (control table address 7).
     *
     * @note Torque must be disabled before changing the ID.
     * @param newID New servo ID (0–252; 253–255 are reserved).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setID(uint8_t newID);

    /**
     * @brief Set the baud rate index (control table address 8).
     *
     * @param baudRate Baud rate index (3 = 1 Mbps, 4 = 2 Mbps, 7 = 4.5 Mbps).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setBaudRate(uint8_t baudRate);

    /**
     * @brief Set the return delay time (control table address 9).
     *
     * Actual delay in microseconds = `delayTime` × 2.
     *
     * @param delayTime Delay in 2 µs units (0–254).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setReturnDelayTime(uint8_t delayTime);

    /**
     * @brief Configure drive mode bits (control table address 10).
     *
     * @param torqueOnByGoalUpdate True to auto-enable torque when a goal is updated.
     * @param timeBasedProfile     True to interpret profile values as time (ms) instead of velocity.
     * @param reverseMode          True to reverse the direction of rotation.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setDriveMode(bool torqueOnByGoalUpdate, bool timeBasedProfile, bool reverseMode);

    /**
     * @brief Set the profile velocity (control table address 112).
     *
     * Each unit corresponds to 0.229 RPM.  Set to 0 for maximum velocity.
     *
     * @param profileVelocity Profile velocity in raw units.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setProfileVelocity(uint32_t profileVelocity);

    /**
     * @brief Set the profile acceleration (control table address 108).
     *
     * Each unit corresponds to 214.577 Rev/min².  Set to 0 for maximum acceleration.
     *
     * @param profileAcceleration Profile acceleration in raw units.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setProfileAcceleration(uint32_t profileAcceleration);

    /**
     * @brief Set goal velocity in RPM (Velocity Control Mode, control table address 104).
     *
     * Positive values rotate CCW, negative values rotate CW.
     *
     * @param rpm Target velocity in revolutions per minute.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setGoalVelocity_RPM(float rpm);

    /**
     * @brief Set goal PWM duty cycle (PWM Control Mode, control table address 100).
     *
     * Each unit ≈ 0.113 %.  Valid range: −885 to +885.
     *
     * @param goalPWM PWM value (negative = reverse direction).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t setGoalPWM(int16_t goalPWM);

    /**
     * @brief Read present velocity in RPM (control table address 128).
     *
     * @param[out] rpm Present velocity in revolutions per minute.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t getPresentVelocity_RPM(float &rpm);

    /**
     * @brief Read present position in raw encoder counts (control table address 132).
     *
     * @param[out] presentPosition Present position value.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t getPresentPosition(int32_t &presentPosition);

    /**
     * @brief Read present load percentage (control table address 126).
     *
     * Range: −1000 to +1000, where each unit = 0.1 % of stall torque.
     * Positive = CCW direction, negative = CW direction.
     *
     * @param[out] currentLoad Present load value.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t getCurrentLoad(int16_t &currentLoad);

    /**
     * @brief Read the Moving Status register (control table address 123).
     *
     * @param[out] status Decoded moving status (profile type, following error, in-position flag).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t getMovingStatus(DxlMovingStatus &status);

    /**
     * @brief Read the Hardware Error Status register (control table address 70).
     *
     * @param[out] hwErrorStatus Bitmask of active hardware fault flags.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t getHardwareErrorStatus(uint8_t &hwErrorStatus);

    /**
     * @brief Read the present internal temperature in °C (control table address 146).
     *
     * @param[out] temperature Internal temperature in degrees Celsius.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t getPresentTemperature(uint8_t &temperature);

    /**
     * @brief Turn off the servo LED (control table address 65).
     */
    void ledOff();

    /**
     * @brief Print the last raw response bytes via the Debug library (LOG_DEBUG).
     */
    void printResponse();

    /**
     * @brief Ping the servo and retrieve its model number.
     *
     * @param[out] modelNumber 16-bit model number encoded in bits [15:0].
     * @return Error byte from status packet (0 = success).
     */
    uint8_t ping(uint32_t &modelNumber);

    /**
     * @brief Restore factory default settings.
     *
     * @param level Reset scope: 0x01 = all registers,
     *              0x02 = all except ID, 0xFF = all except ID and baud rate.
     * @return Error byte from status packet (0 = success).
     */
    uint8_t factoryReset(uint8_t level);

    /**
     * @brief Reboot the servo (triggers an internal hardware reset).
     *
     * @return Error byte from status packet (0 = success).
     */
    uint8_t reboot();

    /**
     * @brief Enable or disable verbose debug output via the Debug library.
     *
     * When enabled, raw packet bytes and status responses are logged to UART5
     * at Level::LOG_DEBUG.
     *
     * @param enable True to enable debug logging, false to disable.
     */
    void setDebug(bool enable);

    /**
     * @brief Read a register from multiple servos simultaneously (SYNC_READ).
     *
     * Broadcasts a SYNC_READ packet to all `ids` and collects one status
     * response per servo, matching each response to the correct index by ID.
     *
     * @tparam T         Result type matching the register width.
     * @param address    Control table register address to read.
     * @param dataLength Register width in bytes (should equal sizeof(T)).
     * @param ids        Array of servo IDs.
     * @param[out] values Array to receive one value per servo.
     * @param count      Number of servos.
     * @return 0 on full success, non-zero error byte if any response had an error.
     */
    template <typename T>
    uint8_t syncRead(uint16_t address, uint8_t dataLength,
                     const uint8_t *ids, T *values, uint8_t count);

    /**
     * @brief Write different values to different registers on multiple servos (BULK_WRITE).
     *
     * @param ids         Array of servo IDs.
     * @param addresses   Array of control table addresses, one per servo.
     * @param dataLengths Array of byte counts to write, one per servo.
     * @param values      Array of values to write, one per servo.
     * @param count       Number of servos.
     * @return True if the packet was sent successfully.
     */
    bool bulkWrite(const uint8_t *ids, uint16_t *addresses,
                   uint8_t *dataLengths, uint32_t *values, uint8_t count);

    /**
     * @brief Read different registers from different servos simultaneously (BULK_READ).
     *
     * @param ids         Array of servo IDs.
     * @param addresses   Array of control table addresses, one per servo.
     * @param dataLengths Array of byte counts to read, one per servo.
     * @param[out] values Array to receive one value per servo.
     * @param count       Number of servos.
     * @return 0 on full success, non-zero error byte if any response had an error.
     */
    uint8_t bulkRead(const uint8_t *ids, uint16_t *addresses,
                     uint8_t *dataLengths, uint32_t *values, uint8_t count);

    /** @brief Set operating mode for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setOperatingMode(const uint8_t (&modes)[N]);

    /** @brief Set homing offset (raw) for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setHomingOffset(const int32_t (&offsets)[N]);

    /** @brief Set homing offset (degrees) for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setHomingOffset_A(const float (&offsetAngles)[N]);

    /** @brief Set goal position (PCM, raw) for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setGoalPosition_PCM(const uint16_t (&goalPositions)[N]);

    /** @brief Set goal position (degrees, PCM) for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setGoalPosition_A_PCM(const float (&angleDegrees)[N]);

    /** @brief Set goal position (EPCM, raw) for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setGoalPosition_EPCM(const int32_t (&extendedPositions)[N]);

    /** @brief Enable or disable torque for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setTorqueEnable(const bool (&enable)[N]);

    /** @brief Set LED state for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setLED(const bool (&enable)[N]);

    /** @brief Set status return level for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setStatusReturnLevel(const uint8_t (&levels)[N]);

    /** @brief Set servo ID for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setID(const uint8_t (&newIDs)[N]);

    /** @brief Set baud rate index for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setBaudRate(const uint8_t (&baudRates)[N]);

    /** @brief Set return delay time for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setReturnDelayTime(const uint8_t (&delayTimes)[N]);

    /** @brief Configure drive mode for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setDriveMode(const bool (&torqueOnByGoalUpdate)[N],
                         const bool (&timeBasedProfile)[N],
                         const bool (&reverseMode)[N]);

    /** @brief Set profile velocity for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setProfileVelocity(const uint32_t (&profileVelocity)[N]);

    /** @brief Set profile acceleration for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setProfileAcceleration(const uint32_t (&profileAcceleration)[N]);

    /** @brief Set goal velocity (RPM) for each motor in the sync group. */
    template <uint8_t N>
    uint8_t setGoalVelocity_RPM(const float (&rpmValues)[N]);

    /** @brief Read present velocity (RPM) from each motor in the sync group. */
    template <uint8_t N>
    uint8_t getPresentVelocity_RPM(float (&rpms)[N]);

    /** @brief Read present position from each motor in the sync group. */
    template <uint8_t N>
    uint8_t getPresentPosition(int32_t (&presentPositions)[N]);

    /** @brief Read present load from each motor in the sync group. */
    template <uint8_t N>
    uint8_t getCurrentLoad(int16_t (&currentLoad)[N]);

    /** @brief Read moving status from each motor in the sync group. */
    template <uint8_t N>
    uint8_t getMovingStatus(DxlMovingStatus (&status)[N]);

    /**
     * @brief Register a callback invoked on every DXL TX (after TC) and every
     *        valid RX (after CRC pass).  Pass nullptr to disable.
     *
     * Called from a blocking main-loop context — keep it short (e.g. GPIO toggle).
     */
    static void setActivityCallback(void (*cb)(void)) { _activityCb = cb; }

private:
    USART_TypeDef *_usart;    //< USART peripheral
    uint8_t        _servoID;  //< Target servo ID

    bool     _sync      = false;    //< Sync-broadcast mode active
    uint8_t  _numMotors = 1;        //< Number of motors in sync group
    uint8_t *_motorIDs  = nullptr;  //< Motor IDs for sync group

    bool    _debug = false;   //< True if verbose debug logging is enabled.
    uint8_t _error = 0;       //< Last error byte received from a status packet.

    static void (*_activityCb)(void);  //< Optional TX/RX activity hook (shared across all instances).

    /**
     * @brief Transmit a raw byte packet over the USART (blocking, LL).
     *
     * Waits for the TC (Transmission Complete) flag before returning so that
     * the RS-485 DE line is de-asserted before the servo replies.
     *
     * @param packet Pointer to packet bytes.
     * @param length Number of bytes to transmit.
     * @return True on success, false if the USART is not ready.
     */
    bool sendPacket(const uint8_t *packet, uint16_t length);

    /**
     * @brief Receive a Dynamixel status packet with timeout (blocking, LL).
     *
     * Searches for the 4-byte header (0xFF 0xFF 0xFD 0x00), assembles the
     * full packet, verifies the CRC-16-IBM, and discards echo bytes that
     * appear in half-duplex mode (byte 7 ≠ 0x55 → recurse).
     *
     * @return Decoded DxlStatusPacket; `valid` is false on timeout or CRC error.
     */
    DxlStatusPacket receivePacket();

    /**
     * @brief Compute CRC-16-IBM using the STM32G474 hardware CRC unit.
     *
     * The CRC unit must be initialised for CRC-16-IBM (poly 0x8005, init 0x0000)
     * before this is called — see Core/Src/crc.c generated by CubeMX.
     *
     * @param data   Pointer to byte array.
     * @param length Number of bytes.
     * @return CRC-16-IBM result.
     */
    uint16_t calculateCRC(const uint8_t *data, uint16_t length);

    /**
     * @brief Send a WRITE instruction and wait for the status response.
     *
     * @param address Control table register address.
     * @param value   Value to write (up to 4 bytes, little-endian).
     * @param size    Register width in bytes (1, 2, or 4).
     * @return Error byte from status packet (0 = success).
     */
    uint8_t writeRegister(uint16_t address, uint32_t value, uint8_t size);

    /**
     * @brief Send a READ instruction and parse the typed response.
     *
     * @tparam T      Result type matching the register width.
     * @param address Control table register address.
     * @param[out] value  Receives the register value on success.
     * @param size    Register width in bytes (should equal sizeof(T)).
     * @return Error byte from status packet (0 = success).
     */
    template <typename T>
    uint8_t readRegister(uint16_t address, T &value, uint8_t size);

    /**
     * @brief Send a Sync Write instruction to multiple servos simultaneously.
     *
     * @param address    Control table register address.
     * @param dataLength Register width in bytes.
     * @param ids        Array of servo IDs.
     * @param values     Array of values to write, one per servo.
     * @param count      Number of servos.
     * @return True if the packet was sent successfully.
     */
    bool syncWrite(uint16_t address, uint8_t dataLength,
                   const uint8_t *ids, uint32_t *values, uint8_t count);

    /**
     * @brief Build and transmit a SYNC_WRITE packet from pre-encoded parameters.
     *
     * @param parameters       Pointer to pre-encoded parameter bytes.
     * @param parametersLength Number of parameter bytes.
     * @return True on success.
     */
    bool sendSyncWritePacket(const uint8_t *parameters, uint16_t parametersLength);

    /**
     * @brief Build and transmit a SYNC_READ packet.
     *
     * @param address    Control table register address to read.
     * @param dataLength Number of bytes to read from each servo.
     * @param ids        Array of servo IDs.
     * @param count      Number of servos.
     * @return True on success.
     */
    bool sendSyncReadPacket(uint16_t address, uint8_t dataLength,
                            const uint8_t *ids, uint8_t count);

    /**
     * @brief Build and transmit a BULK_WRITE packet.
     *
     * @param parameters       Pre-encoded parameter bytes (ID + address + length + data per servo).
     * @param parametersLength Number of parameter bytes.
     * @return True on success.
     */
    bool sendBulkWritePacket(const uint8_t *parameters, uint16_t parametersLength);

    /**
     * @brief Build and transmit a BULK_READ packet.
     *
     * @param ids         Array of servo IDs.
     * @param addresses   Array of control table addresses, one per servo.
     * @param dataLengths Array of byte counts to read, one per servo.
     * @param count       Number of servos.
     * @return True on success.
     */
    bool sendBulkReadPacket(const uint8_t *ids, uint16_t *addresses,
                            uint8_t *dataLengths, uint8_t count);

    /**
     * @brief Validate that the sync group contains at least `arraySize` entries.
     *
     * @param arraySize Minimum required number of motors in the sync group.
     * @return 0 if valid, non-zero error code otherwise.
     */
    uint8_t checkArraySize(uint8_t arraySize) const;
};

// Include template definitions.
#include "DynamixelLL.tpp"

#endif // DYNAMIXEL_LL_H 
