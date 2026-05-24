#pragma once

template <typename T>
uint8_t DynamixelLL::readRegister(uint16_t address, T& value, uint8_t size)
{
    // Build a 14-byte READ instruction packet:
    // [Header (4) | Servo ID (1) | Length (2) | Instruction (1) | Parameters (4) | CRC (2)]
    uint8_t packet[14];
    uint16_t length = 7; // Parameter bytes (4) + Instruction (1) + CRC (2)

    // Header (4 bytes):
    packet[0] = 0xFF;
    packet[1] = 0xFF;
    packet[2] = 0xFD;
    packet[3] = 0x00;

    // Servo ID (1 byte):
    packet[4] = _servoID;

    // Length field (2 bytes, little-endian):
    packet[5] = length & 0xFF;
    packet[6] = (length >> 8) & 0xFF;

    // Instruction (1 byte): READ (0x02)
    packet[7] = DXL_INST_READ;

    // Parameters (4 bytes): starting address and data length (each in little-endian)
    packet[8] = address & 0xFF;
    packet[9] = (address >> 8) & 0xFF;
    packet[10] = size & 0xFF;
    packet[11] = (size >> 8) & 0xFF;

    // Compute and append CRC (over the first 12 bytes)
    uint16_t crc = calculateCRC(packet, 12);
    packet[12] = crc & 0xFF;
    packet[13] = (crc >> 8) & 0xFF;

    // Transmit the packet.
    if (!sendPacket(packet, 14))
    {
        LOG_WARN("DXL: read send failed\n");
        return 1;
    }

    // Receive and process the response.
    DxlStatusPacket response = receivePacket();
    if (!response.valid)
    {
        LOG_WARN("DXL: invalid status packet received\n");
        return 1u; // Return error when no valid status packet received
    }
    if (response.error != 0)
        LOG_WARN("DXL: read error 0x%02X\n", response.error);

    value = 0;
    for (uint8_t i = 0; i < response.dataLength; i++)
        value |= (T)(response.data[i]) << (8 * i);

    return response.error;
}

template <typename T>
uint8_t DynamixelLL::syncRead(uint16_t address, uint8_t dataLength, const uint8_t* ids, T* values, uint8_t count)
{
    // Send Sync Read Instruction Packet.
    if (!sendSyncReadPacket(address, dataLength, ids, count))
    {
        LOG_WARN("DXL: sync read send failed\n");
        return 1;
    }

    uint8_t retError = 0;
    for (uint8_t i = 0; i < count; i++)
        values[i] = 0;

    // For each device, read its response.
    uint8_t received = 0;
    while (received < count)
    {
        DxlStatusPacket response = receivePacket();
        received++;
        if (!response.valid)
        {
            LOG_WARN("DXL: invalid status packet received\n");
            continue;
        }
        if (response.error != 0)
        {
            LOG_WARN("DXL: sync read error 0x%02X from ID %u\n", response.error, response.id);
            retError = response.error;
            continue;
        }
        // Find the index in the provided ids array that matches the response id.
        for (uint8_t i = 0; i < count; i++)
        {
            if (ids[i] == response.id)
            {
                for (uint8_t j = 0; j < response.dataLength; j++)
                    values[i] |= (T)(response.data[j]) << (8 * j);
                break;
            }
        }
    }
    return retError;
}

template <uint8_t N>
uint8_t DynamixelLL::setOperatingMode(const uint8_t (&modes)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        if (!(modes[i] == 1 || modes[i] == 3 || modes[i] == 4 || modes[i] == 16))
        {
            LOG_WARN("DXL: unsupported operating mode %u\n", modes[i]);
            return 1;
        }
        processed[i] = modes[i];
    }
    return syncWrite(11, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setHomingOffset(const int32_t (&offsets)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        int32_t v = offsets[i];
        if (v > 1044479)
            v = 1044479;
        if (v < -1044479)
            v = -1044479;
        processed[i] = static_cast<uint32_t>(v);
    }
    return syncWrite(20, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setHomingOffsetA(const float (&offsetAngles)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        int32_t v = static_cast<int32_t>(offsetAngles[i] / 0.088f);
        if (v > 1044479)
            v = 1044479;
        if (v < -1044479)
            v = -1044479;
        processed[i] = static_cast<uint32_t>(v);
    }
    return syncWrite(20, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setGoalPositionPcm(const uint16_t (&goalPositions)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        uint32_t v = goalPositions[i];
        if (v > 4095u)
            v = 4095u;
        processed[i] = v;
    }
    return syncWrite(116, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setGoalPositionAPcm(const float (&angleDegrees)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        uint32_t v = static_cast<uint32_t>(angleDegrees[i] / 0.088f);
        if (v > 4095u)
            v = 4095u;
        processed[i] = v;
    }
    return syncWrite(116, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setGoalPositionEpcm(const int32_t (&extendedPositions)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        int32_t v = extendedPositions[i];
        if (v > 1048575)
            v = 1048575;
        if (v < -1048575)
            v = -1048575;
        processed[i] = static_cast<uint32_t>(v);
    }
    return syncWrite(116, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setTorqueEnable(const bool (&enable)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
        processed[i] = enable[i] ? 1u : 0u;
    return syncWrite(64, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setLED(const bool (&enable)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
        processed[i] = enable[i] ? 1u : 0u;
    return syncWrite(65, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setStatusReturnLevel(const uint8_t (&levels)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        if (levels[i] > 2u)
        {
            LOG_WARN("DXL: invalid SRL %u\n", levels[i]);
            return 1;
        }
        processed[i] = levels[i];
    }
    return syncWrite(68, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setID(const uint8_t (&newIDs)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        if (newIDs[i] > 253u)
        {
            LOG_WARN("DXL: invalid ID %u\n", newIDs[i]);
            return 1;
        }
        processed[i] = newIDs[i];
    }
    return syncWrite(7, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setBaudRate(const uint8_t (&baudRates)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        if (baudRates[i] > 7u)
        {
            LOG_WARN("DXL: invalid baud %u\n", baudRates[i]);
            return 1;
        }
        processed[i] = baudRates[i];
    }
    return syncWrite(8, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setReturnDelayTime(const uint8_t (&delayTimes)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        uint8_t v = delayTimes[i];
        if (v > 254u)
            v = 254u;
        processed[i] = v;
    }
    return syncWrite(9, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setDriveMode(const bool (&torqueOnByGoalUpdate)[N],
                                  const bool (&timeBasedProfile)[N],
                                  const bool (&reverseMode)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        uint8_t mode = 0;
        if (torqueOnByGoalUpdate[i])
            mode |= 0x08u;
        if (timeBasedProfile[i])
            mode |= 0x04u;
        if (reverseMode[i])
            mode |= 0x01u;
        processed[i] = mode;
    }
    return syncWrite(10, 1, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setProfileVelocity(const uint32_t (&profileVelocity)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        uint8_t dm = 0;
        readRegister<uint8_t>(10u, dm, 1u);
        uint32_t maxPV = (dm & 0x04u) ? 32737UL : 32767UL;
        processed[i] = (profileVelocity[i] > maxPV) ? maxPV : profileVelocity[i];
    }
    return syncWrite(112, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setProfileAcceleration(const uint32_t (&profileAcceleration)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        uint8_t dm = 0;
        readRegister<uint8_t>(10u, dm, 1u);
        bool timeBased = (dm & 0x04u) != 0;
        uint32_t maxPA = timeBased ? 32737UL : 32767UL;
        uint32_t v = (profileAcceleration[i] > maxPA) ? maxPA : profileAcceleration[i];
        if (timeBased)
        {
            uint32_t pv = 0;
            if (readRegister<uint32_t>(112u, pv, 4u) == 0 && pv > 0 && v > pv / 2)
                v = pv / 2;
        }
        processed[i] = v;
    }
    return syncWrite(108, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::setGoalVelocityRpm(const float (&rpmValues)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    const float maxRPM = 30.0f;
    uint32_t processed[_numMotors];
    for (uint8_t i = 0; i < _numMotors; i++)
    {
        float rpm = rpmValues[i];
        if (rpm > maxRPM)
            rpm = maxRPM;
        if (rpm < -maxRPM)
            rpm = -maxRPM;
        processed[i] = static_cast<uint32_t>(static_cast<uint16_t>(static_cast<int16_t>(rpm / 0.229f)));
    }
    return syncWrite(104, 4, _motorIDs, processed, _numMotors) ? 0u : 1u;
}

template <uint8_t N>
uint8_t DynamixelLL::getPresentVelocityRpm(float (&rpms)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    int16_t temp[_numMotors];
    uint8_t err = syncRead(128, 4, _motorIDs, temp, _numMotors);
    if (err != 0)
        LOG_WARN("DXL: sync read present velocity error 0x%02X\n", err);
    else
        for (uint8_t i = 0; i < _numMotors; i++)
            rpms[i] = static_cast<float>(temp[i]) * 0.229f;
    return err;
}

template <uint8_t N>
uint8_t DynamixelLL::getPresentPosition(int32_t (&presentPositions)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint8_t err = syncRead(132, 4, _motorIDs, presentPositions, _numMotors);
    if (err != 0)
        LOG_WARN("DXL: sync read present position error 0x%02X\n", err);
    return err;
}

template <uint8_t N>
uint8_t DynamixelLL::getCurrentLoad(int16_t (&currentLoad)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint8_t err = syncRead(126, 2, _motorIDs, currentLoad, _numMotors);
    if (err != 0)
        LOG_WARN("DXL: sync read current load error 0x%02X\n", err);
    return err;
}

template <uint8_t N>
uint8_t DynamixelLL::getMovingStatus(DxlMovingStatus (&status)[N])
{
    if (checkArraySize(N) != 0)
        return 1;
    uint8_t temp[_numMotors];
    uint8_t err = syncRead(123, 1, _motorIDs, temp, _numMotors);
    if (err != 0)
    {
        LOG_WARN("DXL: sync read moving status error 0x%02X\n", err);
    }
    else
    {
        for (uint8_t i = 0; i < _numMotors; i++)
        {
            status[i].raw = temp[i];
            status[i].profileType = (DxlVelocityProfile)((temp[i] >> 4) & 0x03u);
            status[i].followingError = ((temp[i] >> 3) & 0x01u) != 0;
            status[i].profileOngoing = ((temp[i] >> 1) & 0x01u) != 0;
            status[i].inPosition = (temp[i] & 0x01u) != 0;
        }
    }
    return err;
}
