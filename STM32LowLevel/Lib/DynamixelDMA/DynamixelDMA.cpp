#include "DynamixelDMA.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_ll_crc.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_hal.h"
#include <cstring>

namespace
{
constexpr uint16_t RX_CAPACITY = 256;
constexpr uint8_t HEADER[4] = {0xFF, 0xFF, 0xFD, 0x00};
constexpr uint8_t STATUS_INST = 0x55;
constexpr uint8_t SYNC_READ_INST = 0x82;
constexpr uint8_t BULK_READ_INST = 0x92;

struct RawFrame
{
    uint16_t length;
    uint8_t bytes[DXL_DMA_MAX_FRAME];
};
struct Bus
{
    USART_TypeDef* uart;
    uint32_t txCh;
    uint32_t rxCh;
    GPIO_TypeDef* dirPort;
    uint32_t dirPin;
    alignas(4) uint8_t tx[DXL_DMA_MAX_PACKET];
    alignas(4) uint8_t rx[RX_CAPACITY];
    RawFrame replies[DXL_DMA_MAX_REPLIES];
    volatile DxlDmaState state;
    volatile bool fault;
    volatile uint32_t wraps;
    uint32_t consumed;
    uint32_t started;
    uint32_t timeout;
    uint16_t frameLength;
    uint16_t frameTarget;
    uint8_t scratch[DXL_DMA_MAX_FRAME];
    uint8_t expected;
    uint8_t expectedIds[DXL_DMA_MAX_REPLIES];
    uint8_t received;
    uint8_t read;
};

// These are the two independent hardware buses, not two implementations.
Bus buses[2] = {{USART2, LL_DMA_CHANNEL_1, LL_DMA_CHANNEL_2, GPIOA, LL_GPIO_PIN_1},
                {USART3, LL_DMA_CHANNEL_3, LL_DMA_CHANNEL_4, GPIOB, LL_GPIO_PIN_14}};

Bus* findBus(USART_TypeDef* uart)
{
    for (auto& bus : buses)
    {
        if (bus.uart == uart)
        {
            return &bus;
        }
    }
    return nullptr;
}
void clearTxFlags(Bus& b)
{
    if (b.uart == USART2)
    {
        LL_DMA_ClearFlag_GI1(DMA1);
    }
    else
    {
        LL_DMA_ClearFlag_GI3(DMA1);
    }
}
void clearRxFlags(Bus& b)
{
    if (b.uart == USART2)
    {
        LL_DMA_ClearFlag_GI2(DMA1);
    }
    else
    {
        LL_DMA_ClearFlag_GI4(DMA1);
    }
}
bool txError(Bus& b)
{
    return b.uart == USART2 ? LL_DMA_IsActiveFlag_TE1(DMA1) : LL_DMA_IsActiveFlag_TE3(DMA1);
}
bool txComplete(Bus& b)
{
    return b.uart == USART2 ? LL_DMA_IsActiveFlag_TC1(DMA1) : LL_DMA_IsActiveFlag_TC3(DMA1);
}
bool rxError(Bus& b)
{
    return b.uart == USART2 ? LL_DMA_IsActiveFlag_TE2(DMA1) : LL_DMA_IsActiveFlag_TE4(DMA1);
}
bool rxComplete(Bus& b)
{
    return b.uart == USART2 ? LL_DMA_IsActiveFlag_TC2(DMA1) : LL_DMA_IsActiveFlag_TC4(DMA1);
}
bool rxHalf(Bus& b)
{
    return b.uart == USART2 ? LL_DMA_IsActiveFlag_HT2(DMA1) : LL_DMA_IsActiveFlag_HT4(DMA1);
}
void stopTx(Bus& b)
{
    LL_USART_DisableDMAReq_TX(b.uart);
    LL_USART_DisableIT_TC(b.uart);
    LL_DMA_DisableIT_TC(DMA1, b.txCh);
    LL_DMA_DisableIT_TE(DMA1, b.txCh);
    LL_DMA_DisableChannel(DMA1, b.txCh);
}
void stopRx(Bus& b)
{
    LL_USART_DisableDMAReq_RX(b.uart);
    LL_USART_DisableIT_IDLE(b.uart);
    LL_USART_DisableIT_ERROR(b.uart);
    LL_DMA_DisableIT_HT(DMA1, b.rxCh);
    LL_DMA_DisableIT_TC(DMA1, b.rxCh);
    LL_DMA_DisableIT_TE(DMA1, b.rxCh);
    LL_DMA_DisableChannel(DMA1, b.rxCh);
}
void startRx(Bus& b)
{
    stopRx(b);
    b.wraps = 0;
    b.consumed = 0;
    b.frameLength = 0;
    b.frameTarget = 0;
    LL_DMA_SetMode(DMA1, b.rxCh, LL_DMA_MODE_CIRCULAR);
    LL_DMA_ConfigAddresses(DMA1,
                           b.rxCh,
                           LL_USART_DMA_GetRegAddr(b.uart, LL_USART_DMA_REG_DATA_RECEIVE),
                           reinterpret_cast<uint32_t>(b.rx),
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, b.rxCh, RX_CAPACITY);
    clearRxFlags(b);
    LL_USART_ClearFlag_IDLE(b.uart);
    LL_DMA_EnableIT_HT(DMA1, b.rxCh);
    LL_DMA_EnableIT_TC(DMA1, b.rxCh);
    LL_DMA_EnableIT_TE(DMA1, b.rxCh);
    LL_DMA_EnableChannel(DMA1, b.rxCh);
    LL_USART_EnableDMAReq_RX(b.uart);
    LL_USART_EnableIT_IDLE(b.uart);
    LL_USART_EnableIT_ERROR(b.uart);
}
void fail(Bus& b)
{
    stopTx(b);
    stopRx(b);
    LL_GPIO_ResetOutputPin(b.dirPort, b.dirPin);
    b.fault = true;
    b.state = DXL_DMA_ERROR;
}

// Count bytes written into circular SRAM. IRQs are masked while sampling
// the software wrap counter together with DMA flags and its NDTR register.
uint32_t produced(Bus& b)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const uint32_t laps = b.wraps;
    const bool pending = rxComplete(b);
    const uint32_t remaining = LL_DMA_GetDataLength(DMA1, b.rxCh);
    if (rxError(b) || (pending && rxHalf(b)))
    {
        b.fault = true;
    }
    __set_PRIMASK(primask);
    return (laps + ((pending && remaining != 0U) ? 1U : 0U)) * RX_CAPACITY + RX_CAPACITY - remaining;
}

// Incremental framing; no CPU read of USART RDR while RX DMA is active.
// Decode only complete responses; malformed/echo frames do not consume
// one of the expected reply slots.
bool validStatus(Bus& b)
{
    if (b.scratch[7] != STATUS_INST)
    {
        return false;
    }
    const uint8_t id = b.scratch[4];
    bool requested = false;
    for (uint8_t i = 0; i < b.expected; ++i)
    {
        if (b.expectedIds[i] == id)
        {
            requested = true;
        }
    }
    if (!requested)
    {
        return false;
    }
    for (uint8_t i = 0; i < b.received; ++i)
    {
        if (b.replies[i].bytes[4] == id)
        {
            return false;
        }
    }

    LL_CRC_ResetCRCCalculationUnit(CRC);
    for (uint16_t i = 0; i < b.frameLength - 2U; ++i)
    {
        LL_CRC_FeedData8(CRC, b.scratch[i]);
    }
    const uint16_t computed = static_cast<uint16_t>(LL_CRC_ReadData16(CRC));
    const uint16_t transmitted = static_cast<uint16_t>(b.scratch[b.frameLength - 2U]) |
                                 (static_cast<uint16_t>(b.scratch[b.frameLength - 1U]) << 8);
    return transmitted == computed;
}

void acceptByte(Bus& b, uint8_t value)
{
    if (b.frameLength < 4)
    {
        if (value == HEADER[b.frameLength])
        {
            b.scratch[b.frameLength++] = value;
        }
        else
        {
            b.frameLength = (value == 0xFF) ? 1 : 0;
            if (b.frameLength)
            {
                b.scratch[0] = value;
            }
        }
        return;
    }
    if (b.frameLength >= DXL_DMA_MAX_FRAME)
    {
        b.fault = true;
        return;
    }
    b.scratch[b.frameLength++] = value;
    if (b.frameLength == 7)
    {
        const uint32_t size = 7U + static_cast<uint32_t>(b.scratch[5]) + (static_cast<uint32_t>(b.scratch[6]) << 8);
        if (size < 11U || size > DXL_DMA_MAX_FRAME)
        {
            b.frameLength = 0;
            b.frameTarget = 0;
            return;
        }
        b.frameTarget = static_cast<uint16_t>(size);
    }
    if (b.frameTarget != 0 && b.frameLength == b.frameTarget)
    {
        if (validStatus(b))
        {
            if (b.received >= b.expected || b.received >= DXL_DMA_MAX_REPLIES)
            {
                b.fault = true;
                return;
            }
            auto& response = b.replies[b.received++];
            response.length = b.frameLength;
            memcpy(response.bytes, b.scratch, b.frameLength);
        }
        b.frameLength = 0;
        b.frameTarget = 0;
    }
}
void txIrq(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b || b->state != DXL_DMA_SENDING)
    {
        return;
    }
    const bool error = txError(*b);
    const bool done = txComplete(*b);
    clearTxFlags(*b);
    if (error)
    {
        fail(*b);
    }
    else if (done)
    {
        // DMA TC precedes USART TC: leave direction asserted.
        LL_USART_DisableDMAReq_TX(uart);
        LL_DMA_DisableIT_TC(DMA1, b->txCh);
        // Arm UART TC only after DMA provided the final byte to TDR.
        LL_USART_EnableIT_TC(uart);
    }
}
void rxIrq(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b || b->state != DXL_DMA_RECEIVING)
    {
        return;
    }
    const bool error = rxError(*b);
    const bool half = rxHalf(*b);
    const bool complete = rxComplete(*b);
    clearRxFlags(*b);
    if (error || (half && complete))
    {
        fail(*b);
    }
    else if (complete)
    {
        ++b->wraps;
    }
}
void uartIrq(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b)
    {
        return;
    }
    if (LL_USART_IsEnabledIT_TC(uart) && LL_USART_IsActiveFlag_TC(uart))
    {
        LL_USART_ClearFlag_TC(uart);
        if (b->state == DXL_DMA_SENDING)
        {
            stopTx(*b);
            if (b->expected != 0)
            {
                startRx(*b);
                LL_GPIO_ResetOutputPin(b->dirPort, b->dirPin);
                b->state = DXL_DMA_RECEIVING;
            }
            else
            {
                LL_GPIO_ResetOutputPin(b->dirPort, b->dirPin);
                b->state = DXL_DMA_COMPLETE;
            }
        }
    }
    if (LL_USART_IsActiveFlag_IDLE(uart))
    {
        LL_USART_ClearFlag_IDLE(uart);
    }
    bool error = false;
    if (LL_USART_IsActiveFlag_ORE(uart))
    {
        LL_USART_ClearFlag_ORE(uart);
        error = true;
    }
    if (LL_USART_IsActiveFlag_FE(uart))
    {
        LL_USART_ClearFlag_FE(uart);
        error = true;
    }
    if (LL_USART_IsActiveFlag_NE(uart))
    {
        LL_USART_ClearFlag_NE(uart);
        error = true;
    }
    if (error && b->state == DXL_DMA_RECEIVING)
    {
        fail(*b);
    }
}
} // namespace

extern "C" bool
dxlDmaStart(USART_TypeDef* uart, const uint8_t* packet, uint16_t length, uint8_t replies, uint32_t timeoutMs)
{
    Bus* b = findBus(uart);
    if (!b || !packet || length < 10U || length > DXL_DMA_MAX_PACKET || replies > DXL_DMA_MAX_REPLIES ||
        timeoutMs == 0U || !LL_USART_IsEnabled(uart) || __get_IPSR() != 0U)
    {
        return false;
    }

    // Only the requested motors can complete this transaction.
    uint8_t ids[DXL_DMA_MAX_REPLIES] = {};
    if (replies != 0U)
    {
        if (packet[4] != 0xFEU && replies == 1U)
        {
            ids[0] = packet[4];
        }
        else if (packet[4] == 0xFEU && packet[7] == SYNC_READ_INST && length == static_cast<uint16_t>(14U + replies))
        {
            for (uint8_t i = 0; i < replies; ++i)
            {
                ids[i] = packet[12U + i];
            }
        }
        else if (packet[4] == 0xFEU && packet[7] == BULK_READ_INST &&
                 length == static_cast<uint16_t>(10U + 5U * replies))
        {
            for (uint8_t i = 0; i < replies; ++i)
            {
                ids[i] = packet[8U + 5U * i];
            }
        }
        else
        {
            return false;
        }
        for (uint8_t i = 0; i < replies; ++i)
        {
            for (uint8_t j = 0; j < i; ++j)
            {
                if (ids[j] == ids[i])
                {
                    return false;
                }
            }
        }
    }

    // Claim the whole request/response period, not merely the TX period.
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    bool available = (b->state == DXL_DMA_IDLE) || (b->state == DXL_DMA_COMPLETE && b->expected == 0);
    if (available)
    {
        b->state = DXL_DMA_SENDING;
    }
    __set_PRIMASK(primask);
    if (!available)
    {
        return false;
    }

    b->fault = false;
    b->expected = replies;
    memcpy(b->expectedIds, ids, sizeof(ids));
    b->received = 0;
    b->read = 0;
    b->started = HAL_GetTick();
    b->timeout = timeoutMs;
    memcpy(b->tx, packet, length);
    stopRx(*b);

    // Discard stale RDR bytes only while the DMA consumer is disabled.
    while (LL_USART_IsActiveFlag_RXNE(uart))
    {
        (void)LL_USART_ReceiveData8(uart);
    }
    LL_USART_ClearFlag_ORE(uart);
    LL_USART_ClearFlag_FE(uart);
    LL_USART_ClearFlag_NE(uart);

    LL_GPIO_SetOutputPin(b->dirPort, b->dirPin);
    LL_DMA_DisableChannel(DMA1, b->txCh);
    LL_DMA_SetMode(DMA1, b->txCh, LL_DMA_MODE_NORMAL);
    LL_DMA_ConfigAddresses(DMA1,
                           b->txCh,
                           reinterpret_cast<uint32_t>(b->tx),
                           LL_USART_DMA_GetRegAddr(uart, LL_USART_DMA_REG_DATA_TRANSMIT),
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(DMA1, b->txCh, length);
    clearTxFlags(*b);
    LL_USART_ClearFlag_TC(uart);
    LL_DMA_EnableIT_TC(DMA1, b->txCh);
    LL_DMA_EnableIT_TE(DMA1, b->txCh);
    LL_DMA_EnableChannel(DMA1, b->txCh);
    LL_USART_EnableDMAReq_TX(uart);
    return true;
}

extern "C" DxlDmaState dxlDmaService(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b)
    {
        return DXL_DMA_ERROR;
    }
    if (b->state == DXL_DMA_IDLE)
    {
        return DXL_DMA_IDLE;
    }
    if (b->state == DXL_DMA_ERROR)
    {
        return DXL_DMA_ERROR;
    }
    if ((HAL_GetTick() - b->started) >= b->timeout && b->state != DXL_DMA_COMPLETE)
    {
        fail(*b);
        return DXL_DMA_ERROR;
    }
    if (b->state == DXL_DMA_RECEIVING)
    {
        const uint32_t end = produced(*b);
        if (b->fault || end < b->consumed || end - b->consumed > RX_CAPACITY)
        {
            fail(*b);
            return DXL_DMA_ERROR;
        }
        while (b->consumed != end && b->state == DXL_DMA_RECEIVING)
        {
            acceptByte(*b, b->rx[b->consumed % RX_CAPACITY]);
            ++b->consumed;
            if (b->fault)
            {
                fail(*b);
                return DXL_DMA_ERROR;
            }
            if (b->received == b->expected)
            {
                stopRx(*b);
                b->state = DXL_DMA_COMPLETE;
            }
        }
    }
    return b->state;
}

extern "C" bool dxlDmaReadFrame(USART_TypeDef* uart, uint8_t* frame, uint16_t* length)
{
    Bus* b = findBus(uart);
    if (!b || !frame || !length || b->read >= b->received)
    {
        return false;
    }
    const RawFrame& result = b->replies[b->read++];
    *length = result.length;
    memcpy(frame, result.bytes, result.length);
    if (b->state == DXL_DMA_COMPLETE && b->read == b->received)
    {
        b->state = DXL_DMA_IDLE;
    }
    return true;
}

extern "C" void dxlDmaCancel(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b)
    {
        return;
    }
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    stopTx(*b);
    stopRx(*b);
    LL_GPIO_ResetOutputPin(b->dirPort, b->dirPin);
    b->state = DXL_DMA_IDLE;
    b->fault = false;
    b->expected = b->received = b->read = 0;
    __set_PRIMASK(primask);
}

extern "C" void dxlDmaTxIrq2(void)
{
    txIrq(USART2);
}
extern "C" void dxlDmaTxIrq3(void)
{
    txIrq(USART3);
}
extern "C" void dxlDmaRxIrq2(void)
{
    rxIrq(USART2);
}
extern "C" void dxlDmaRxIrq3(void)
{
    rxIrq(USART3);
}
extern "C" void dxlDmaUsartIrq2(void)
{
    uartIrq(USART2);
}
extern "C" void dxlDmaUsartIrq3(void)
{
    uartIrq(USART3);
}
