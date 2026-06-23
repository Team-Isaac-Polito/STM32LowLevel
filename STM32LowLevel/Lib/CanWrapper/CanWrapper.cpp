#include "CanWrapper.h"

#include <cstring>

#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_fdcan.h"
#include "stm32g4xx_ll_gpio.h"
#include "fdcan.h"
#include "main.h"
#include "mod_config.h"
#include "Debug.h"

// TX FIFO element size in bytes: 2 header words + 2 data words (8 bytes) = 16 bytes
// For classic 8-byte CAN frames, the TX element is 4 words × 4 bytes = 16 bytes.
// SRAMCAN_TFQ_SIZE (from hal_fdcan.c) = 18 × 4 = 72 bytes (max FD frame).
static constexpr uint32_t CANWRAP_TFQ_SIZE = 18U * 4U; // 72 bytes per TX element
static constexpr uint32_t CANWRAP_RF0_SIZE = 18U * 4U; // 72 bytes per RX FIFO0 element

// Extended ID element masks (mirrors FDCAN_ELEMENT_MASK_* from stm32g4xx_hal_fdcan.c)
static constexpr uint32_t EXTID_MASK = 0x1FFFFFFFU; // bits[28:0]: extended ID
static constexpr uint32_t DLC_MASK = 0x000F0000U;   // bits[19:16]: DLC in RX word 1
static constexpr uint32_t DLC_SHIFT = 16U;

void CanWrapper::begin()
{
    // Configure one extended-ID mask filter:
    //   FilterID1 = pattern: bits[15:8] == CAN_ID (destination field)
    //   FilterID2 = mask:    only bits[15:8] matter
    FDCAN_FilterTypeDef f = {};
    f.IdType = FDCAN_EXTENDED_ID;
    f.FilterIndex = 0;
    f.FilterType = FDCAN_FILTER_MASK;
    f.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    f.FilterID1 = static_cast<uint32_t>(CAN_ID) << 8;
    f.FilterID2 = 0x0000FF00UL;
    HAL_FDCAN_ConfigFilter(&hfdcan2, &f);
    HAL_FDCAN_ConfigGlobalFilter(&hfdcan2, FDCAN_REJECT, FDCAN_REJECT, FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE);
    HAL_FDCAN_Start(&hfdcan2);
    LL_GPIO_ResetOutputPin(CAN_SHDN_GPIO_Port, CAN_SHDN_Pin);
    LL_GPIO_ResetOutputPin(CAN_STBY_GPIO_Port, CAN_STBY_Pin);
}

bool CanWrapper::sendMessage(uint8_t msgType, const void* data, uint8_t length)
{
    // Check TX FIFO not full (FDCAN_TXFQS.TFQF)
    if (FDCAN2->TXFQS & FDCAN_TXFQS_TFQF)
    {
        LOG_WARN("CAN: TX FIFO full\n");
        return false;
    }

    // Get TX FIFO put index (FDCAN_TXFQS.TFQPI bits[17:16])
    uint32_t putIdx = (FDCAN2->TXFQS & FDCAN_TXFQS_TFQPI_Msk) >> FDCAN_TXFQS_TFQPI_Pos;

    // Build 29-bit extended CAN ID:
    //   bits[23:16] = msgType (PDU format / packet identifier)
    //   bits[15:8]  = 0x00   (broadcast destination)
    //   bits[7:0]   = CAN_ID (source address)
    uint32_t extId = (static_cast<uint32_t>(msgType) << 16) | static_cast<uint32_t>(CAN_ID);

    // Calculate TX buffer element address in message RAM
    uint32_t* txAddr = reinterpret_cast<uint32_t*>(hfdcan2.msgRam.TxFIFOQSA + (putIdx * CANWRAP_TFQ_SIZE));

    // Word 0 (T0): XTD=1 (extended ID), RTR=0, ESI=0, ID[28:0]
    txAddr[0] = FDCAN_EXTENDED_ID | extId;

    // Word 1 (T1): FDF=0 (classic CAN), BRS=0, DLC=8
    txAddr[1] = FDCAN_DLC_BYTES_8 << DLC_SHIFT;

    // Words 2–3: payload (little-endian, 4 bytes per word)
    if (length > 8U)
        length = 8U;
    const uint8_t* src = reinterpret_cast<const uint8_t*>(data);
    uint8_t padded[8] = {};
    memcpy(padded, src, length);
    uint32_t w0 = (static_cast<uint32_t>(padded[3]) << 24) | (static_cast<uint32_t>(padded[2]) << 16) |
                  (static_cast<uint32_t>(padded[1]) << 8) | static_cast<uint32_t>(padded[0]);
    uint32_t w1 = (static_cast<uint32_t>(padded[7]) << 24) | (static_cast<uint32_t>(padded[6]) << 16) |
                  (static_cast<uint32_t>(padded[5]) << 8) | static_cast<uint32_t>(padded[4]);
    txAddr[2] = w0;
    txAddr[3] = w1;

    // Request transmission (FDCAN2->TXBAR bit per put index)
    FDCAN2->TXBAR = (1U << putIdx);

    return true;
}

bool CanWrapper::readMessage(uint8_t* msgType, uint8_t* data)
{
    // Check RX FIFO0 fill level (FDCAN_RXF0S.F0FL bits[6:0])
    if ((FDCAN2->RXF0S & FDCAN_RXF0S_F0FL) == 0U)
        return false;

    // Get RX FIFO0 get index (FDCAN_RXF0S.F0GI bits[13:8])
    uint32_t getIdx = (FDCAN2->RXF0S & FDCAN_RXF0S_F0GI_Msk) >> FDCAN_RXF0S_F0GI_Pos;

    // Calculate RX FIFO0 element address in message RAM
    uint32_t* rxAddr = reinterpret_cast<uint32_t*>(hfdcan2.msgRam.RxFIFO0SA + (getIdx * CANWRAP_RF0_SIZE));

    // Word 0 (R0): bits[28:0] = 29-bit extended ID
    uint32_t extId = rxAddr[0] & EXTID_MASK;
    *msgType = static_cast<uint8_t>((extId >> 16) & 0xFFU);

    // Word 1 (R1): bits[19:16] = DLC
    uint8_t dlc = static_cast<uint8_t>((rxAddr[1] & DLC_MASK) >> DLC_SHIFT);
    if (dlc > 8U)
        dlc = 8U;

    // Words 2+: payload bytes (little-endian in 32-bit words)
    uint8_t* payload = reinterpret_cast<uint8_t*>(&rxAddr[2]);
    memcpy(data, payload, dlc);

    // Acknowledge RX FIFO0 (increment get index)
    FDCAN2->RXF0A = getIdx;

    LOG_DEBUG("CAN RX: type=0x%02X len=%u\n", *msgType, dlc);

    return true;
}
