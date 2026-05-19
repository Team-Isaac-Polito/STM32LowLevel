/**
 * @file DynamixelLL.cpp
 * @brief Dynamixel Protocol 2.0 low-level driver — STM32G474 USART/LL backend.
 */

#include "DynamixelLL.h"
#include "Debug.h"

#include <cstring>
#include <cstdio>

void (*DynamixelLL::activityCb)(void) = nullptr;

DynamixelLL::DynamixelLL(USART_TypeDef* usart, uint8_t servoID)
    : _usart(usart), _servoID(servoID)
{}

DynamixelLL::~DynamixelLL()
{
    disableSync();
}

void DynamixelLL::begin()
{
    LL_USART_Disable(_usart);

    SET_BIT(_usart->CR3, USART_CR3_HDSEL);

    LL_USART_Enable(_usart);

    while (LL_USART_IsActiveFlag_RXNE(_usart))
        (void)LL_USART_ReceiveData8(_usart);

    // Set default direction: RX mode (DIR = LOW)
    // This ensures the level shifter is ready to receive data
    if (_usart == USART2) {
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1); // DXL1_DE = LOW (RX mode)
    } else if (_usart == USART3) {
        LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_14); // DXL2_DE = LOW (RX mode)
    }
}

void DynamixelLL::setDebug(bool enable)
{
    _debug = enable;
}

void DynamixelLL::enableSync(const uint8_t* motorIDs, uint8_t numMotors)
{
    if (numMotors < 2)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: enableSync requires >= 2 motors\n");
        return;
    }
    disableSync();
    _motorIDs = new uint8_t[numMotors];
    _numMotors = numMotors;
    _sync = true;
    for (uint8_t i = 0; i < numMotors; ++i)
        _motorIDs[i] = motorIDs[i];
}

void DynamixelLL::disableSync()
{
    delete[] _motorIDs;
    _motorIDs = nullptr;
    _numMotors = 1;
    _sync = false;
}

/* -----------------------------------------------------------------------
 * Instruction Functions
 * ----------------------------------------------------------------------- */

void DynamixelLL::ledOff()
{
    uint8_t packet[13] = {0xFF, 0xFF, 0xFD, 0x00, _servoID, 0x06, 0x00, 0x03, 0x41, 0x00, 0x00, 0x00, 0x00};
    uint16_t crc = calculateCRC(packet, 11);
    packet[11] = crc & 0xFF;
    packet[12] = (crc >> 8) & 0xFF;
    sendPacket(packet, 13);
}

void DynamixelLL::printResponse()
{
    DxlStatusPacket pkt = receivePacket();
    if (!pkt.valid)
    {
        debug.log(Level::LogDebug, "DXL: no valid response\n");
        return;
    }
    debug.log(Level::LogDebug, "DXL response: id=0x%02X err=0x%02X data=", pkt.id, pkt.error);
    for (uint8_t i = 0; i < pkt.dataLength; i++)
        debug.log(Level::LogDebug, "0x%02X ", pkt.data[i]);
    debug.log(Level::LogDebug, "\n");
}

uint8_t DynamixelLL::ping(uint32_t& modelNumber)
{
    uint8_t packet[10];
    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = 0xFD;
    packet[3] = 0x00;
    packet[4] = _servoID;
    packet[5] = 0x03;
    packet[6] = 0x00;
    packet[7] = DXL_INST_PING;
    uint16_t crc = calculateCRC(packet, 8u);
    packet[8] = (uint8_t)(crc & 0xFF);
    packet[9] = (uint8_t)(crc >> 8);

    if (!sendPacket(packet, 10u))
        return 1u;

    DxlStatusPacket rsp = receivePacket();
    modelNumber = 0;
    for (uint8_t i = 0; i < rsp.dataLength && i < 4u; ++i)
        modelNumber |= (uint32_t)rsp.data[i] << (8 * i);
    return rsp.valid ? rsp.error : 1u;
}

uint8_t DynamixelLL::reboot()
{
    uint8_t packet[10];
    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = 0xFD;
    packet[3] = 0x00;
    packet[4] = _servoID;
    packet[5] = 0x03;
    packet[6] = 0x00;
    packet[7] = DXL_INST_REBOOT;
    uint16_t crc = calculateCRC(packet, 8u);
    packet[8] = (uint8_t)(crc & 0xFF);
    packet[9] = (uint8_t)(crc >> 8);

    if (!sendPacket(packet, 10u))
        return 1u;
    DxlStatusPacket rsp = receivePacket();
    return rsp.valid ? rsp.error : 1u;
}

uint8_t DynamixelLL::factoryReset(uint8_t level)
{
    uint8_t packet[11];
    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = 0xFD;
    packet[3] = 0x00;
    packet[4] = _servoID;
    packet[5] = 0x04;
    packet[6] = 0x00;
    packet[7] = DXL_INST_FACTORY_RESET;
    packet[8] = level;
    uint16_t crc = calculateCRC(packet, 9u);
    packet[9] = (uint8_t)(crc & 0xFF);
    packet[10] = (uint8_t)(crc >> 8);

    if (!sendPacket(packet, 11u))
        return 1u;
    DxlStatusPacket rsp = receivePacket();
    return rsp.valid ? rsp.error : 1u;
}

/* -----------------------------------------------------------------------
 * Control Table Functions
 * ----------------------------------------------------------------------- */

uint8_t DynamixelLL::setOperatingMode(uint8_t mode)
{
    if (!(mode == 1 || mode == 3 || mode == 4 || mode == 16))
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: unsupported operating mode %u\n", mode);
        return 1u;
    }
    return writeRegister(11u, mode, 1u);
}

uint8_t DynamixelLL::setHomingOffset(int32_t offset)
{
    if (offset > 1044479)
        offset = 1044479;
    if (offset < -1044479)
        offset = -1044479;
    return writeRegister(20u, (uint32_t)offset, 4u);
}

uint8_t DynamixelLL::setHomingOffsetA(float offsetAngle)
{
    return setHomingOffset((int32_t)(offsetAngle / 0.088f));
}

uint8_t DynamixelLL::setGoalPositionPcm(uint16_t goalPosition)
{
    if (goalPosition > 4095u)
        goalPosition = 4095u;
    return writeRegister(116u, goalPosition, 4u);
}

uint8_t DynamixelLL::setGoalPositionAPcm(float angleDegrees)
{
    uint32_t pos = (uint32_t)(angleDegrees / 0.088f);
    if (pos > 4095u)
        pos = 4095u;
    return writeRegister(116u, pos, 4u);
}

uint8_t DynamixelLL::setGoalPositionEpcm(int32_t extendedPosition)
{
    if (extendedPosition > 1048575)
        extendedPosition = 1048575;
    if (extendedPosition < -1048575)
        extendedPosition = -1048575;
    return writeRegister(116u, (uint32_t)extendedPosition, 4u);
}

uint8_t DynamixelLL::setTorqueEnable(bool enable)
{
    return writeRegister(64u, enable ? 1u : 0u, 1u);
}

uint8_t DynamixelLL::setLED(bool enable)
{
    return writeRegister(65u, enable ? 1u : 0u, 1u);
}

uint8_t DynamixelLL::setStatusReturnLevel(uint8_t level)
{
    if (level > 2u)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: invalid SRL %u\n", level);
        return 1u;
    }
    return writeRegister(68u, level, 1u);
}

uint8_t DynamixelLL::setID(uint8_t newID)
{
    if (newID > 253u)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: invalid ID %u\n", newID);
        return 1u;
    }
    return writeRegister(7u, newID, 1u);
}

uint8_t DynamixelLL::setBaudRate(uint8_t baudRate)
{
    if (baudRate > 7u)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: invalid baud %u\n", baudRate);
        return 1u;
    }
    return writeRegister(8u, baudRate, 1u);
}

uint8_t DynamixelLL::setReturnDelayTime(uint8_t delayTime)
{
    if (delayTime > 254u)
        delayTime = 254u;
    return writeRegister(9u, delayTime, 1u);
}

uint8_t DynamixelLL::setDriveMode(bool torqueOnByGoalUpdate, bool timeBasedProfile, bool reverseMode)
{
    uint8_t mode = 0;
    if (torqueOnByGoalUpdate)
        mode |= 0x08u;
    if (timeBasedProfile)
        mode |= 0x04u;
    if (reverseMode)
        mode |= 0x01u;
    return writeRegister(10u, mode, 1u);
}

uint8_t DynamixelLL::setProfileVelocity(uint32_t profileVelocity)
{
    uint8_t dm = 0;
    readRegister<uint8_t>(10u, dm, 1u);
    uint32_t maxPV = (dm & 0x04u) ? 32737UL : 32767UL;
    if (profileVelocity > maxPV)
        profileVelocity = maxPV;
    return writeRegister(112u, profileVelocity, 4u);
}

uint8_t DynamixelLL::setProfileAcceleration(uint32_t profileAcceleration)
{
    uint8_t dm = 0;
    readRegister<uint8_t>(10u, dm, 1u);
    bool timeBased = (dm & 0x04u) != 0;
    uint32_t maxPA = timeBased ? 32737UL : 32767UL;
    if (profileAcceleration > maxPA)
        profileAcceleration = maxPA;
    if (timeBased)
    {
        uint32_t pv = 0;
        if (readRegister<uint32_t>(112u, pv, 4u) == 0 && pv > 0 && profileAcceleration > pv / 2)
            profileAcceleration = pv / 2;
    }
    return writeRegister(108u, profileAcceleration, 4u);
}

uint8_t DynamixelLL::setGoalVelocityRpm(float rpm)
{
    const float maxRPM = 30.0f; // conservative limit at 12 V
    if (rpm > maxRPM)
        rpm = maxRPM;
    if (rpm < -maxRPM)
        rpm = -maxRPM;
    int16_t val = (int16_t)(rpm / 0.229f);
    return writeRegister(104u, (uint32_t)(uint16_t)val, 4u);
}

uint8_t DynamixelLL::getPresentVelocityRpm(float& rpm)
{
    int16_t raw = 0;
    uint8_t err = readRegister<int16_t>(128u, raw, 4u);
    if (!err)
        rpm = raw * 0.229f;
    return err;
}

uint8_t DynamixelLL::getPresentPosition(int32_t& presentPosition)
{
    return readRegister<int32_t>(132u, presentPosition, 4u);
}

uint8_t DynamixelLL::getCurrentLoad(int16_t& currentLoad)
{
    return readRegister<int16_t>(126u, currentLoad, 2u);
}

uint8_t DynamixelLL::getMovingStatus(DxlMovingStatus& status)
{
    uint8_t err = readRegister<uint8_t>(123u, status.raw, 1u);
    if (!err)
    {
        status.profileType = (DxlVelocityProfile)((status.raw >> 4) & 0x03u);
        status.followingError = ((status.raw >> 3) & 0x01u) != 0;
        status.profileOngoing = ((status.raw >> 1) & 0x01u) != 0;
        status.inPosition = (status.raw & 0x01u) != 0;
    }
    return err;
}

uint8_t DynamixelLL::getHardwareErrorStatus(uint8_t& hwErrorStatus)
{
    return readRegister<uint8_t>(70u, hwErrorStatus, 1u);
}

uint8_t DynamixelLL::setGoalPWM(int16_t goalPWM)
{
    if (goalPWM > 885)
        goalPWM = 885;
    if (goalPWM < -885)
        goalPWM = -885;
    return writeRegister(100u, (uint32_t)(uint16_t)goalPWM, 2u);
}

uint8_t DynamixelLL::getPresentTemperature(uint8_t& temperature)
{
    return readRegister<uint8_t>(146u, temperature, 1u);
}

uint16_t DynamixelLL::calculateCRC(const uint8_t* data, uint16_t length)
{
    // Reset the CRC unit — reloads the initial value (0x0000)
    LL_CRC_ResetCRCCalculationUnit(CRC);

    // Feed bytes one at a time.
    for (uint16_t i = 0; i < length; ++i)
        LL_CRC_FeedData8(CRC, data[i]);

    return static_cast<uint16_t>(LL_CRC_ReadData16(CRC));
}

bool DynamixelLL::sendPacket(const uint8_t* packet, uint16_t length)
{
    if (_debug)
    {
        char hexBuf[DXL_MAX_PACKET_SIZE * 3 + 8];
        int pos = snprintf(hexBuf, sizeof(hexBuf), "DXL TX(%u):", (unsigned)length);
        for (uint16_t i = 0; i < length && pos < (int)sizeof(hexBuf) - 4; ++i)
            pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, " %02X", packet[i]);
        debug.log(Level::LogDebug, "%s\n", hexBuf);
    }

    // Verify USART is enabled
    if (!LL_USART_IsEnabled(_usart))
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: USART not enabled!\n");
        return false;
    }

    // Flush any truly stale RX data and clear latent errors before starting
    while (LL_USART_IsActiveFlag_RXNE(_usart))
        (void)LL_USART_ReceiveData8(_usart);
    LL_USART_ClearFlag_ORE(_usart);
    LL_USART_ClearFlag_FE(_usart);

    // STEP 1: Switch the SN74LVC1T45 to Transmit mode (A -> B)
    if (_usart == USART2) {
        LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_1); // DXL1_DE = HIGH (TX mode)
    } else if (_usart == USART3) {
        LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_14); // DXL2_DE = HIGH (TX mode)
    }

    // Transmit all bytes (blocking)
    for (uint16_t i = 0; i < length; ++i)
    {
        uint32_t timeout = 100000U;
        while (!LL_USART_IsActiveFlag_TXE(_usart))
        {
            if (--timeout == 0U)
            {
                if (_debug)
                    debug.log(Level::LogWarn, "DXL: TXE timeout at byte %u\n", i);
                return false;
            }
        }
        LL_USART_TransmitData8(_usart, packet[i]);
    }

    // Wait until transmission complete (TC flag) — all bits are out
    uint32_t tcTimeout = 100000U;
    while (!LL_USART_IsActiveFlag_TC(_usart))
    {
        if (--tcTimeout == 0U)
        {
            if (_debug)
                debug.log(Level::LogWarn, "DXL: TC timeout\n");
            return false;
        }
    }

    // STEP 2: Switch the SN74LVC1T45 back to Receive mode (B -> A)
    if (_usart == USART2) {
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1); // DXL1_DE = LOW (RX mode)
    } else if (_usart == USART3) {
        LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_14); // DXL2_DE = LOW (RX mode)
    }

    if (activityCb)
        activityCb();

    return true;
}

DxlStatusPacket DynamixelLL::receivePacket()
{
    DxlStatusPacket result = {false, 0, 0, {}, 0};

    // Clear latent error flags that may have accumulated from noise,
    // floating bus, or self-echo during transmission.
    // FE (Framing Error): triggered by floating line when no motor connected
    // NE (Noise Error): triggered by noise on the bus
    // ORE (Overrun Error): triggered by self-echo in half-duplex mode
    if (LL_USART_IsActiveFlag_FE(_usart))
        LL_USART_ClearFlag_FE(_usart);
    if (LL_USART_IsActiveFlag_NE(_usart))
        LL_USART_ClearFlag_NE(_usart);
    if (LL_USART_IsActiveFlag_ORE(_usart))
        LL_USART_ClearFlag_ORE(_usart);

    // Flush any stale bytes from a previous failed read
    while (LL_USART_IsActiveFlag_RXNE(_usart))
        (void)LL_USART_ReceiveData8(_usart);

    uint8_t buf[DXL_MAX_PACKET_SIZE];
    uint16_t idx = 0;
    bool headerFound = false;

    uint32_t start = HAL_GetTick();

    // locate the 4-byte header 0xFF 0xFF 0xFD 0x00
    // Also handle the case where the first 0xFF might be missing (partial header)
    while ((HAL_GetTick() - start) < DXL_RX_TIMEOUT_MS && idx < DXL_MAX_PACKET_SIZE)
    {
        if (LL_USART_IsActiveFlag_RXNE(_usart))
        {
            buf[idx++] = LL_USART_ReceiveData8(_usart);
            if (idx >= 4 && buf[idx - 4] == 0xFF && buf[idx - 3] == 0xFF && buf[idx - 2] == 0xFD &&
                buf[idx - 1] == 0x00)
            {
                // Normalise header to start at index 0
                buf[0] = 0xFF;
                buf[1] = 0xFF;
                buf[2] = 0xFD;
                buf[3] = 0x00;
                idx = 4;
                headerFound = true;
                break;
            }
            else if (idx >= 3 &&
                     buf[idx - 3] == 0xFF &&
                     buf[idx - 2] == 0xFD &&
                     buf[idx - 1] == 0x00)
            {
                // Partial header: first 0xFF was likely consumed as stale byte
                buf[0] = 0xFF;
                buf[1] = 0xFF;
                buf[2] = 0xFD;
                buf[3] = 0x00;
                idx = 4;
                headerFound = true;
                break;
            }
        }
    }

    if (!headerFound)
    {
        if (_debug)
        {
            debug.log(Level::LogWarn, "DXL: header timeout\n");
            // Debug: check if USART is still enabled and if any flags are set
            debug.log(Level::LogDebug, "DXL: USART ISR=0x%08lX UE=%u HDSEL=%u DEM=%u\n",
                      (unsigned long)_usart->ISR,
                      (unsigned)LL_USART_IsEnabled(_usart),
                      (unsigned)READ_BIT(_usart->CR3, USART_CR3_HDSEL),
                      (unsigned)READ_BIT(_usart->CR3, USART_CR3_DEM));
        }
        return result;
    }

    // read until we have the ID + 2-byte length field (7 bytes total)
    while ((HAL_GetTick() - start) < DXL_RX_TIMEOUT_MS && idx < 7)
        if (LL_USART_IsActiveFlag_RXNE(_usart))
            buf[idx++] = LL_USART_ReceiveData8(_usart);
    if (idx < 7)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: header extension timeout\n");
        return result;
    }

    result.id = buf[4];
    uint16_t lengthField = (uint16_t)buf[5] | ((uint16_t)buf[6] << 8);
    uint16_t totalPacketLength = 7u + lengthField;

    if (totalPacketLength > DXL_MAX_PACKET_SIZE)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: packet too large (%u)\n", totalPacketLength);
        return result;
    }

    // read remaining bytes
    while ((HAL_GetTick() - start) < DXL_RX_TIMEOUT_MS && idx < totalPacketLength)
        if (LL_USART_IsActiveFlag_RXNE(_usart))
            buf[idx++] = LL_USART_ReceiveData8(_usart);
    if (idx < totalPacketLength)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: incomplete packet (got %u/%u)\n", idx, totalPacketLength);
        return result;
    }

    if (_debug)
    {
        char hexBuf[DXL_MAX_PACKET_SIZE * 3 + 8];
        int pos = snprintf(hexBuf, sizeof(hexBuf), "DXL RX(%u):", totalPacketLength);
        for (uint16_t i = 0; i < totalPacketLength && pos < (int)sizeof(hexBuf) - 4; ++i)
            pos += snprintf(hexBuf + pos, sizeof(hexBuf) - pos, " %02X", buf[i]);
        debug.log(Level::LogDebug, "%s\n", hexBuf);
    }

    // verify instruction byte (must be 0x55 for status packet)
    if (buf[7] != DXL_STATUS_INST)
    {
        if (_debug)
            debug.log(Level::LogDebug, "DXL: echo detected, retrying\n");
        return receivePacket(); // discard echo, try again
    }

    result.error = buf[8];
    uint8_t paramLength = (uint8_t)(lengthField - 4u);
    result.dataLength = paramLength;
    for (uint8_t i = 0; i < paramLength && i < (uint8_t)sizeof(result.data); ++i)
        result.data[i] = buf[9 + i];

    // verify CRC
    uint16_t receivedCRC = (uint16_t)buf[9 + paramLength] | ((uint16_t)buf[10 + paramLength] << 8);
    uint16_t computedCRC = calculateCRC(buf, 9u + paramLength);

    if (receivedCRC != computedCRC)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: CRC mismatch (got %04X, expected %04X)\n", receivedCRC, computedCRC);
        return result;
    }

    result.valid = true;
    if (activityCb)
        activityCb();
    return result;
}

uint8_t DynamixelLL::writeRegister(uint16_t address, uint32_t value, uint8_t size)
{
    if (_sync)
    {
        uint32_t buf[_numMotors];
        for (uint8_t i = 0; i < _numMotors; ++i)
            buf[i] = value;
        return syncWrite(address, size, _motorIDs, buf, _numMotors) ? 0u : 1u;
    }

    // Packet: Header(4) + ID(1) + Length(2) + Inst(1) + Addr(2) + Data(size) + CRC(2)
    uint16_t length = 5u + size; // Inst + Addr + Data + CRC
    uint8_t pktLen = 12u + size; // total bytes
    uint8_t packet[pktLen];

    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = 0xFD;
    packet[3] = 0x00;
    packet[4] = _servoID;
    packet[5] = (uint8_t)(length & 0xFF);
    packet[6] = (uint8_t)(length >> 8);
    packet[7] = DXL_INST_WRITE;
    packet[8] = (uint8_t)(address & 0xFF);
    packet[9] = (uint8_t)(address >> 8);
    for (uint8_t i = 0; i < size; ++i)
        packet[10 + i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    uint16_t crc = calculateCRC(packet, pktLen - 2u);
    packet[pktLen - 2] = (uint8_t)(crc & 0xFF);
    packet[pktLen - 1] = (uint8_t)(crc >> 8);

    if (!sendPacket(packet, pktLen))
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: write send failed\n");
        return 1u;
    }

    DxlStatusPacket rsp = receivePacket();
    if (!rsp.valid)
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: write invalid response\n");
        return 1u; // Return error when no valid status packet received
    }
    if (_debug && rsp.error)
        debug.log(Level::LogWarn, "DXL: write error 0x%02X\n", rsp.error);
    return rsp.error;
}

bool DynamixelLL::syncWrite(uint16_t address, uint8_t dataLength, const uint8_t* ids, uint32_t* values, uint8_t count)
{
    const uint16_t fixedParam = 4u;               // addr(2) + datalen(2)
    const uint16_t deviceBlock = 1u + dataLength; // id + data
    const uint16_t paramLen = fixedParam + deviceBlock * count;

    uint8_t params[paramLen];
    uint16_t idx = 0;
    params[idx++] = (uint8_t)(address & 0xFF);
    params[idx++] = (uint8_t)(address >> 8);
    params[idx++] = dataLength;
    params[idx++] = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
        params[idx++] = ids[i];
        for (uint8_t j = 0; j < dataLength; ++j)
            params[idx++] = (uint8_t)((values[i] >> (8 * j)) & 0xFF);
    }
    return sendSyncWritePacket(params, paramLen);
}

bool DynamixelLL::sendSyncWritePacket(const uint8_t* parameters, uint16_t parametersLength)
{
    uint16_t lengthField = parametersLength + 3u;
    uint16_t pktSize = 10u + parametersLength;
    uint8_t packet[pktSize];
    uint16_t idx = 0;

    packet[idx++] = 0xFF;
    packet[idx++] = 0xFF;
    packet[idx++] = 0xFD;
    packet[idx++] = 0x00;
    packet[idx++] = DXL_BROADCAST_ID;
    packet[idx++] = (uint8_t)(lengthField & 0xFF);
    packet[idx++] = (uint8_t)(lengthField >> 8);
    packet[idx++] = DXL_INST_SYNC_WRITE;
    memcpy(&packet[idx], parameters, parametersLength);
    idx += parametersLength;
    uint16_t crc = calculateCRC(packet, pktSize - 2u);
    packet[idx++] = (uint8_t)(crc & 0xFF);
    packet[idx++] = (uint8_t)(crc >> 8);
    return sendPacket(packet, pktSize);
}

bool DynamixelLL::sendSyncReadPacket(uint16_t address, uint8_t dataLength, const uint8_t* ids, uint8_t count)
{
    const uint16_t paramLen = 4u + count;
    uint16_t lengthField = paramLen + 3u;
    uint16_t pktSize = 10u + paramLen;
    uint8_t packet[pktSize];
    uint16_t idx = 0;

    packet[idx++] = 0xFF;
    packet[idx++] = 0xFF;
    packet[idx++] = 0xFD;
    packet[idx++] = 0x00;
    packet[idx++] = DXL_BROADCAST_ID;
    packet[idx++] = (uint8_t)(lengthField & 0xFF);
    packet[idx++] = (uint8_t)(lengthField >> 8);
    packet[idx++] = DXL_INST_SYNC_READ;
    packet[idx++] = (uint8_t)(address & 0xFF);
    packet[idx++] = (uint8_t)(address >> 8);
    packet[idx++] = dataLength;
    packet[idx++] = 0;
    for (uint8_t i = 0; i < count; ++i)
        packet[idx++] = ids[i];
    uint16_t crc = calculateCRC(packet, pktSize - 2u);
    packet[idx++] = (uint8_t)(crc & 0xFF);
    packet[idx++] = (uint8_t)(crc >> 8);
    return sendPacket(packet, pktSize);
}

bool DynamixelLL::bulkWrite(
    const uint8_t* ids, uint16_t* addresses, uint8_t* dataLengths, uint32_t* values, uint8_t count)
{
    uint16_t paramBlockLength = 0;
    for (uint8_t i = 0; i < count; i++)
        paramBlockLength += 5 + dataLengths[i];

    uint8_t params[paramBlockLength];
    uint16_t idx = 0;

    for (uint8_t i = 0; i < count; i++)
    {
        params[idx++] = ids[i];
        params[idx++] = addresses[i] & 0xFF;
        params[idx++] = (addresses[i] >> 8) & 0xFF;
        params[idx++] = dataLengths[i] & 0xFF;
        params[idx++] = (dataLengths[i] >> 8) & 0xFF;
        for (uint8_t j = 0; j < dataLengths[i]; j++)
            params[idx++] = (values[i] >> (8 * j)) & 0xFF;
    }

    return sendBulkWritePacket(params, paramBlockLength);
}

uint8_t
DynamixelLL::bulkRead(const uint8_t* ids, uint16_t* addresses, uint8_t* dataLengths, uint32_t* values, uint8_t count)
{
    if (!sendBulkReadPacket(ids, addresses, dataLengths, count))
    {
        if (_debug)
            debug.log(Level::LogWarn, "DXL: error sending Bulk Read packet\n");
        return 1;
    }

    uint8_t retError = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        DxlStatusPacket response = receivePacket();
        if (!response.valid || response.error != 0)
        {
            if (_debug)
                debug.log(Level::LogWarn, "DXL: Bulk Read error from id %u: 0x%02X\n", ids[i], response.error);
            retError = response.error;
        }
        values[i] = 0;
        for (uint8_t j = 0; j < response.dataLength; j++)
            values[i] |= (static_cast<uint32_t>(response.data[j]) << (8 * j));
    }

    return retError;
}

bool DynamixelLL::sendBulkWritePacket(const uint8_t* parameters, uint16_t parametersLength)
{
    uint16_t lengthField = parametersLength + 3u;
    uint16_t pktSize = 10u + parametersLength;
    uint8_t packet[pktSize];
    uint16_t idx = 0;

    packet[idx++] = 0xFF;
    packet[idx++] = 0xFF;
    packet[idx++] = 0xFD;
    packet[idx++] = 0x00;
    packet[idx++] = DXL_BROADCAST_ID;
    packet[idx++] = (uint8_t)(lengthField & 0xFF);
    packet[idx++] = (uint8_t)(lengthField >> 8);
    packet[idx++] = DXL_INST_BULK_WRITE;
    memcpy(&packet[idx], parameters, parametersLength);
    idx += parametersLength;
    uint16_t crc = calculateCRC(packet, pktSize - 2u);
    packet[idx++] = (uint8_t)(crc & 0xFF);
    packet[idx++] = (uint8_t)(crc >> 8);
    return sendPacket(packet, pktSize);
}

bool DynamixelLL::sendBulkReadPacket(const uint8_t* ids, uint16_t* addresses, uint8_t* dataLengths, uint8_t count)
{
    const uint16_t paramLen = 5u * count;
    uint16_t lengthField = paramLen + 3u;
    uint16_t pktSize = 10u + paramLen;
    uint8_t packet[pktSize];
    uint16_t idx = 0;

    packet[idx++] = 0xFF;
    packet[idx++] = 0xFF;
    packet[idx++] = 0xFD;
    packet[idx++] = 0x00;
    packet[idx++] = DXL_BROADCAST_ID;
    packet[idx++] = (uint8_t)(lengthField & 0xFF);
    packet[idx++] = (uint8_t)(lengthField >> 8);
    packet[idx++] = DXL_INST_BULK_READ;
    for (uint8_t i = 0; i < count; ++i)
    {
        packet[idx++] = ids[i];
        packet[idx++] = (uint8_t)(addresses[i] & 0xFF);
        packet[idx++] = (uint8_t)(addresses[i] >> 8);
        packet[idx++] = dataLengths[i];
        packet[idx++] = 0;
    }
    uint16_t crc = calculateCRC(packet, pktSize - 2u);
    packet[idx++] = (uint8_t)(crc & 0xFF);
    packet[idx++] = (uint8_t)(crc >> 8);
    return sendPacket(packet, pktSize);
}

uint8_t DynamixelLL::checkArraySize(uint8_t arraySize) const
{
    if (arraySize != _numMotors)
        return 1u;
    return 0u;
}
