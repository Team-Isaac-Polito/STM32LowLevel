/**
 * @file DynamixelLL.cpp
 * @brief Dynamixel Protocol 2.0 low-level driver — STM32G474 USART/LL backend.
 */

#include "DynamixelLL.h"
#include "DynamixelDMA.h"
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

void DynamixelLL::enableSync(const uint8_t* motorIDs, uint8_t numMotors)
{
    if (numMotors < 2)
    {
        LOG_WARN("DXL: enableSync requires >= 2 motors\n");
        return;
    }
    disableSync();
    _motorIDs = new uint8_t[numMotors];
    _numMotors = numMotors;
    _sync = true;
    for (uint8_t i = 0; i < numMotors; ++i)
    {
        _motorIDs[i] = motorIDs[i];
    }
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
    {
        debug.log(Level::LogDebug, "0x%02X ", pkt.data[i]);
    }
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
    {
        return 1u;
    }

    DxlStatusPacket rsp = receivePacket();
    modelNumber = 0;
    for (uint8_t i = 0; i < rsp.dataLength && i < 4u; ++i)
    {
        modelNumber |= (uint32_t)rsp.data[i] << (8 * i);
    }
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
    {
        return 1u;
    }
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
    {
        return 1u;
    }
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
        LOG_WARN("DXL: unsupported operating mode %u\n", mode);
        return 1u;
    }
    return writeRegister(11u, mode, 1u);
}

uint8_t DynamixelLL::setHomingOffset(int32_t offset)
{
    if (offset > 1044479)
    {
        offset = 1044479;
    }
    if (offset < -1044479)
    {
        offset = -1044479;
    }
    return writeRegister(20u, (uint32_t)offset, 4u);
}

uint8_t DynamixelLL::setHomingOffsetA(float offsetAngle)
{
    return setHomingOffset((int32_t)(offsetAngle / 0.088f));
}

uint8_t DynamixelLL::setGoalPositionPcm(uint16_t goalPosition)
{
    if (goalPosition > 4095u)
    {
        goalPosition = 4095u;
    }
    return writeRegister(116u, goalPosition, 4u);
}

uint8_t DynamixelLL::setGoalPositionAPcm(float angleDegrees)
{
    uint32_t pos = (uint32_t)(angleDegrees / 0.088f);
    if (pos > 4095u)
    {
        pos = 4095u;
    }
    return writeRegister(116u, pos, 4u);
}

uint8_t DynamixelLL::setGoalPositionEpcm(int32_t extendedPosition)
{
    if (extendedPosition > 1048575)
    {
        extendedPosition = 1048575;
    }
    if (extendedPosition < -1048575)
    {
        extendedPosition = -1048575;
    }
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
        LOG_WARN("DXL: invalid SRL %u\n", level);
        return 1u;
    }
    return writeRegister(68u, level, 1u);
}

uint8_t DynamixelLL::setID(uint8_t newID)
{
    if (newID > 253u)
    {
        LOG_WARN("DXL: invalid ID %u\n", newID);
        return 1u;
    }
    return writeRegister(7u, newID, 1u);
}

uint8_t DynamixelLL::setBaudRate(uint8_t baudRate)
{
    if (baudRate > 7u)
    {
        LOG_WARN("DXL: invalid baud %u\n", baudRate);
        return 1u;
    }
    return writeRegister(8u, baudRate, 1u);
}

uint8_t DynamixelLL::setReturnDelayTime(uint8_t delayTime)
{
    if (delayTime > 254u)
    {
        delayTime = 254u;
    }
    return writeRegister(9u, delayTime, 1u);
}

uint8_t DynamixelLL::setDriveMode(bool torqueOnByGoalUpdate, bool timeBasedProfile, bool reverseMode)
{
    uint8_t mode = 0;
    if (torqueOnByGoalUpdate)
    {
        mode |= 0x08u;
    }
    if (timeBasedProfile)
    {
        mode |= 0x04u;
    }
    if (reverseMode)
    {
        mode |= 0x01u;
    }
    return writeRegister(10u, mode, 1u);
}

uint8_t DynamixelLL::setProfileVelocity(uint32_t profileVelocity)
{
    uint8_t dm = 0;
    readRegister<uint8_t>(10u, dm, 1u);
    uint32_t maxPV = (dm & 0x04u) ? 32737UL : 32767UL;
    if (profileVelocity > maxPV)
    {
        profileVelocity = maxPV;
    }
    return writeRegister(112u, profileVelocity, 4u);
}

uint8_t DynamixelLL::setProfileAcceleration(uint32_t profileAcceleration)
{
    uint8_t dm = 0;
    readRegister<uint8_t>(10u, dm, 1u);
    bool timeBased = (dm & 0x04u) != 0;
    uint32_t maxPA = timeBased ? 32737UL : 32767UL;
    if (profileAcceleration > maxPA)
    {
        profileAcceleration = maxPA;
    }
    if (timeBased)
    {
        uint32_t pv = 0;
        if (readRegister<uint32_t>(112u, pv, 4u) == 0 && pv > 0 && profileAcceleration > pv / 2)
        {
            profileAcceleration = pv / 2;
        }
    }
    return writeRegister(108u, profileAcceleration, 4u);
}

uint8_t DynamixelLL::setGoalVelocityRpm(float rpm)
{
    const float maxRPM = 30.0f; // conservative limit at 12 V
    if (rpm > maxRPM)
    {
        rpm = maxRPM;
    }
    if (rpm < -maxRPM)
    {
        rpm = -maxRPM;
    }
    int16_t val = (int16_t)(rpm / 0.229f);
    return writeRegister(104u, (uint32_t)val, 4u);
}

uint8_t DynamixelLL::getPresentVelocityRpm(float& rpm)
{
    int32_t raw = 0;
    uint8_t err = readRegister<int32_t>(128u, raw, 4u);
    if (!err)
    {
        rpm = raw * 0.229f;
    }
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
    {
        goalPWM = 885;
    }
    if (goalPWM < -885)
    {
        goalPWM = -885;
    }
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
    {
        LL_CRC_FeedData8(CRC, data[i]);
    }

    return static_cast<uint16_t>(LL_CRC_ReadData16(CRC));
}

bool DynamixelLL::sendPacket(const uint8_t* packet, uint16_t length)
{
    if (!packet || length < 10 || !LL_USART_IsEnabled(_usart) || __get_IPSR() != 0U || (__get_PRIMASK() & 1U) != 0U)
    {
        return false;
    }

    // Sync/bulk READ return one packet per requested motor.
    // Other broadcasts (e.g. SYNC_WRITE) do not produce status packets.
    uint8_t replies = packet[4] == DXL_BROADCAST_ID ? 0U : 1U;
    if (packet[4] == DXL_BROADCAST_ID && packet[7] == DXL_INST_SYNC_READ)
    {
        if (length < 14)
        {
            return false;
        }
        replies = static_cast<uint8_t>(length - 14U);
    }
    else if (packet[4] == DXL_BROADCAST_ID && packet[7] == DXL_INST_BULK_READ)
    {
        if (length < 10 || (length - 10U) % 5U != 0U)
        {
            return false;
        }
        replies = static_cast<uint8_t>((length - 10U) / 5U);
    }
    if (replies > DXL_DMA_MAX_REPLIES)
    {
        return false;
    }

    // A preceding asynchronous no-reply command may still be transmitting.
    // WFI sleeps the core until SysTick, DMA or USART interrupts wake it.
    const uint32_t start = HAL_GetTick();
    while (!dxlDmaStart(_usart, packet, length, replies, static_cast<uint32_t>(DXL_RX_TIMEOUT_MS) + 2U * replies))
    {
        if (dxlDmaService(_usart) == DXL_DMA_ERROR)
        {
            dxlDmaCancel(_usart);
        }
        if (HAL_GetTick() - start >= DXL_RX_TIMEOUT_MS)
        {
            return false;
        }
        __WFI();
    }
    for (;;)
    {
        const DxlDmaState state = dxlDmaService(_usart);
        if (state == DXL_DMA_ERROR)
        {
            dxlDmaCancel(_usart);
            return false;
        }
        if (state == DXL_DMA_RECEIVING || state == DXL_DMA_COMPLETE)
        {
            if (replies == 0)
            {
                dxlDmaCancel(_usart);
            }
            if (activityCb)
            {
                activityCb();
            }
            return true;
        }
        __WFI();
    }
}

DxlStatusPacket DynamixelLL::receivePacket()
{
    DxlStatusPacket result = {false, 0, 0, {}, 0};
    if (__get_IPSR() != 0U || (__get_PRIMASK() & 1U) != 0U)
    {
        return result;
    }

    uint8_t frame[DXL_DMA_MAX_FRAME];
    uint16_t length = 0;
    for (;;)
    {
        const DxlDmaState state = dxlDmaService(_usart);
        if (state == DXL_DMA_ERROR)
        {
            dxlDmaCancel(_usart);
            return result;
        }
        if (dxlDmaReadFrame(_usart, frame, &length))
        {
            break;
        }

        if (state == DXL_DMA_IDLE || state == DXL_DMA_COMPLETE)
        {
            dxlDmaCancel(_usart);
            return result;
        }
        __WFI();
    }

    // DMA transaction framing already verified the header, ID and CRC.
    const uint16_t lengthField = static_cast<uint16_t>(frame[5]) | (static_cast<uint16_t>(frame[6]) << 8);
    if (length < 11U || static_cast<uint32_t>(lengthField) + 7U != length || lengthField < 4U ||
        lengthField - 4U > sizeof(result.data))
    {
        return result;
    }

    result.id = frame[4];
    result.error = frame[8];
    result.dataLength = static_cast<uint8_t>(lengthField - 4U);
    memcpy(result.data, &frame[9], result.dataLength);
    result.valid = true;
    if (activityCb)
    {
        activityCb();
    }
    return result;
}

uint8_t DynamixelLL::writeRegister(uint16_t address, uint32_t value, uint8_t size)
{
    if (_sync)
    {
        uint32_t buf[_numMotors];
        for (uint8_t i = 0; i < _numMotors; ++i)
        {
            buf[i] = value;
        }
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
    {
        packet[10 + i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    }
    uint16_t crc = calculateCRC(packet, pktLen - 2u);
    packet[pktLen - 2] = (uint8_t)(crc & 0xFF);
    packet[pktLen - 1] = (uint8_t)(crc >> 8);

    if (!sendPacket(packet, pktLen))
    {
        LOG_WARN("DXL: write send failed\n");
        return 1u;
    }

    DxlStatusPacket rsp = receivePacket();
    if (!rsp.valid)
    {
        LOG_WARN("DXL: write invalid response\n");
        return 1u; // Return error when no valid status packet received
    }
    if (rsp.error)
    {
        LOG_WARN("DXL: write error 0x%02X\n", rsp.error);
    }
    return rsp.error;
}

bool DynamixelLL::syncWrite(uint16_t address, uint8_t dataLength, const uint8_t* ids, uint32_t* values, uint8_t count)
{
    const uint16_t fixedParam = 4u;               // addr(2) + datalen(2)
    const uint16_t deviceBlock = 1u + dataLength; // id + data
    const uint16_t paramLen = fixedParam + deviceBlock * count;
    if (!ids || !values || count == 0U || dataLength == 0U || dataLength > 4U || paramLen > DXL_DMA_MAX_PACKET - 10U)
    {
        return false;
    }

    uint8_t params[DXL_DMA_MAX_PACKET] = {};
    uint16_t idx = 0;
    params[idx++] = (uint8_t)(address & 0xFF);
    params[idx++] = (uint8_t)(address >> 8);
    params[idx++] = dataLength;
    params[idx++] = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
        params[idx++] = ids[i];
        for (uint8_t j = 0; j < dataLength; ++j)
        {
            params[idx++] = (uint8_t)((values[i] >> (8 * j)) & 0xFF);
        }
    }
    return sendSyncWritePacket(params, paramLen);
}

bool DynamixelLL::trySetGoalVelocityRpm(const float* rpmValues, uint8_t count)
{
    if (!_sync || !_motorIDs || !rpmValues || count != _numMotors || count == 0 || count > DXL_DMA_MAX_REPLIES)
    {
        return false;
    }

    // Fixed-size instruction buffer; DMA copies it into bus-owned SRAM.
    uint8_t packet[10U + 4U + DXL_DMA_MAX_REPLIES * 5U] = {};
    const uint16_t parametersLength = static_cast<uint16_t>(4U + 5U * count);
    const uint16_t packetLength = static_cast<uint16_t>(10U + parametersLength);
    const uint16_t lengthField = static_cast<uint16_t>(parametersLength + 3U);
    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = 0xFD;
    packet[3] = 0x00;
    packet[4] = DXL_BROADCAST_ID;
    packet[5] = static_cast<uint8_t>(lengthField);
    packet[6] = static_cast<uint8_t>(lengthField >> 8);
    packet[7] = DXL_INST_SYNC_WRITE;
    packet[8] = 104U; // Goal Velocity register address (0x0068)
    packet[9] = 0U;
    packet[10] = 4U; // 32-bit signed raw velocity
    packet[11] = 0U;

    uint16_t idx = 12U;
    for (uint8_t i = 0; i < count; ++i)
    {
        float rpm = rpmValues[i];
        if (rpm > 30.0f)
        {
            rpm = 30.0f;
        }
        if (rpm < -30.0f)
        {
            rpm = -30.0f;
        }
        const int32_t raw = static_cast<int32_t>(rpm / 0.229f);
        const uint32_t encoded = static_cast<uint32_t>(raw);
        packet[idx++] = _motorIDs[i];
        for (uint8_t byte = 0; byte < 4U; ++byte)
        {
            packet[idx++] = static_cast<uint8_t>(encoded >> (8U * byte));
        }
    }
    const uint16_t crc = calculateCRC(packet, static_cast<uint16_t>(packetLength - 2U));
    packet[idx++] = static_cast<uint8_t>(crc);
    packet[idx++] = static_cast<uint8_t>(crc >> 8);
    return dxlDmaStart(_usart, packet, packetLength, 0U, DXL_RX_TIMEOUT_MS);
}

bool DynamixelLL::sendSyncWritePacket(const uint8_t* parameters, uint16_t parametersLength)
{
    if (!parameters || parametersLength > DXL_DMA_MAX_PACKET - 10U)
    {
        return false;
    }
    uint16_t lengthField = parametersLength + 3u;
    uint16_t pktSize = 10u + parametersLength;
    uint8_t packet[DXL_DMA_MAX_PACKET] = {};
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
    if (!ids || count == 0U || count > DXL_DMA_MAX_REPLIES || dataLength == 0U ||
        dataLength > sizeof(DxlStatusPacket::data))
    {
        return false;
    }
    const uint16_t paramLen = 4u + count;
    uint16_t lengthField = paramLen + 3u;
    uint16_t pktSize = 10u + paramLen;
    uint8_t packet[DXL_DMA_MAX_PACKET] = {};
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
    {
        packet[idx++] = ids[i];
    }
    uint16_t crc = calculateCRC(packet, pktSize - 2u);
    packet[idx++] = (uint8_t)(crc & 0xFF);
    packet[idx++] = (uint8_t)(crc >> 8);
    return sendPacket(packet, pktSize);
}

bool DynamixelLL::bulkWrite(
    const uint8_t* ids, uint16_t* addresses, uint8_t* dataLengths, uint32_t* values, uint8_t count)
{
    if (!ids || !addresses || !dataLengths || !values || count == 0U)
    {
        return false;
    }
    uint16_t paramBlockLength = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
        if (dataLengths[i] == 0U || dataLengths[i] > 4U ||
            static_cast<uint32_t>(paramBlockLength) + 5U + dataLengths[i] > DXL_DMA_MAX_PACKET - 10U)
        {
            return false;
        }
        paramBlockLength = static_cast<uint16_t>(paramBlockLength + 5U + dataLengths[i]);
    }

    uint8_t params[DXL_DMA_MAX_PACKET] = {};
    uint16_t idx = 0;

    for (uint8_t i = 0; i < count; i++)
    {
        params[idx++] = ids[i];
        params[idx++] = addresses[i] & 0xFF;
        params[idx++] = (addresses[i] >> 8) & 0xFF;
        params[idx++] = dataLengths[i] & 0xFF;
        params[idx++] = (dataLengths[i] >> 8) & 0xFF;
        for (uint8_t j = 0; j < dataLengths[i]; j++)
        {
            params[idx++] = (values[i] >> (8 * j)) & 0xFF;
        }
    }

    return sendBulkWritePacket(params, paramBlockLength);
}

uint8_t
DynamixelLL::bulkRead(const uint8_t* ids, uint16_t* addresses, uint8_t* dataLengths, uint32_t* values, uint8_t count)
{
    if (!sendBulkReadPacket(ids, addresses, dataLengths, count))
    {
        return 1U;
    }

    for (uint8_t i = 0; i < count; ++i)
    {
        values[i] = 0U;
    }

    uint8_t status = 0U;
    for (uint8_t received = 0; received < count; ++received)
    {
        const DxlStatusPacket response = receivePacket();
        if (!response.valid || response.error != 0U)
        {
            status = response.valid ? response.error : 1U;
            continue;
        }
        bool matched = false;
        for (uint8_t i = 0; i < count; ++i)
        {
            if (ids[i] != response.id)
            {
                continue;
            }
            matched = true;
            if (response.dataLength != dataLengths[i] || response.dataLength > 4U)
            {
                status = 1U;
                break;
            }
            for (uint8_t j = 0; j < response.dataLength; ++j)
            {
                values[i] |= static_cast<uint32_t>(response.data[j]) << (8U * j);
            }
            break;
        }
        if (!matched)
        {
            status = 1U;
        }
    }
    return status;
}

bool DynamixelLL::sendBulkWritePacket(const uint8_t* parameters, uint16_t parametersLength)
{
    if (!parameters || parametersLength > DXL_DMA_MAX_PACKET - 10U)
    {
        return false;
    }
    uint16_t lengthField = parametersLength + 3u;
    uint16_t pktSize = 10u + parametersLength;
    uint8_t packet[DXL_DMA_MAX_PACKET] = {};
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
    if (!ids || !addresses || !dataLengths || count == 0U || count > DXL_DMA_MAX_REPLIES)
    {
        return false;
    }
    for (uint8_t i = 0; i < count; ++i)
    {
        if (dataLengths[i] == 0U || dataLengths[i] > sizeof(uint32_t))
        {
            return false;
        }
    }
    const uint16_t paramLen = 5u * count;
    uint16_t lengthField = paramLen + 3u;
    uint16_t pktSize = 10u + paramLen;
    uint8_t packet[DXL_DMA_MAX_PACKET] = {};
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
    {
        return 1u;
    }
    return 0u;
}
