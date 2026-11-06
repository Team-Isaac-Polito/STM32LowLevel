#include "DynamixelDMA.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_hal.h"

namespace
{
constexpr uint16_t RX_SIZE = 256;
typedef struct
{
    USART_TypeDef* uart;
    uint32_t txChannel;
    uint32_t rxChannel;
    GPIO_TypeDef* directionPort;
    uint32_t directionPin;
    alignas(4) uint8_t rx[RX_SIZE];
    volatile bool busy;
    volatile bool done;
    volatile bool failed;
    volatile uint32_t wraps;
    uint32_t consumed;
    uint32_t txStarted;
    uint32_t txTimeout;
} Bus;
Bus buses[] = {
    {USART2, LL_DMA_CHANNEL_1, LL_DMA_CHANNEL_2, GPIOA, LL_GPIO_PIN_1, {}, false, false, false, 0, 0, 0, 0},
    {USART3, LL_DMA_CHANNEL_3, LL_DMA_CHANNEL_4, GPIOB, LL_GPIO_PIN_14, {}, false, false, false, 0, 0, 0, 0},
};
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
bool rxHalfComplete(Bus& b)
{
    return b.uart == USART2 ? LL_DMA_IsActiveFlag_HT2(DMA1) : LL_DMA_IsActiveFlag_HT4(DMA1);
}
void stopTx(Bus& b)
{
    LL_USART_DisableDMAReq_TX(b.uart);
    LL_USART_DisableIT_TC(b.uart);
    LL_DMA_DisableIT_TC(DMA1, b.txChannel);
    LL_DMA_DisableIT_TE(DMA1, b.txChannel);
    LL_DMA_DisableChannel(DMA1, b.txChannel);
}
void stopRx(Bus& b)
{
    LL_USART_DisableDMAReq_RX(b.uart);
    LL_USART_DisableIT_ERROR(b.uart);
    LL_DMA_DisableIT_TC(DMA1, b.rxChannel);
    LL_DMA_DisableIT_HT(DMA1, b.rxChannel);
    LL_DMA_DisableIT_TE(DMA1, b.rxChannel);
    LL_DMA_DisableChannel(DMA1, b.rxChannel);
}
void startRx(Bus& b)
{
    stopRx(b);
    b.wraps = 0;
    b.consumed = 0;
    LL_DMA_SetMode(DMA1, b.rxChannel, LL_DMA_MODE_CIRCULAR);
    LL_DMA_ConfigAddresses(DMA1,
                           b.rxChannel,
                           LL_USART_DMA_GetRegAddr(b.uart, LL_USART_DMA_REG_DATA_RECEIVE),
                           reinterpret_cast<uint32_t>(b.rx),
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, b.rxChannel, RX_SIZE);
    clearRxFlags(b);
    LL_DMA_EnableIT_TC(DMA1, b.rxChannel);
    LL_DMA_EnableIT_HT(DMA1, b.rxChannel);
    LL_DMA_EnableIT_TE(DMA1, b.rxChannel);
    LL_DMA_EnableChannel(DMA1, b.rxChannel);
    LL_USART_EnableDMAReq_RX(b.uart);
    LL_USART_EnableIT_ERROR(b.uart);
}
// Snapshot interrupt-maintained wraps and the DMA counter without ISR races.
uint32_t produced(Bus& b)
{
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t remaining;
    bool before, after;
    do
    {
        before = rxComplete(b);
        remaining = LL_DMA_GetDataLength(DMA1, b.rxChannel);
        after = rxComplete(b);
    } while (before != after);
    const uint32_t wraps = b.wraps + ((after && remaining != 0U) ? 1U : 0U);
    if ((primask & 1U) == 0U)
    {
        __enable_irq();
    }
    return wraps * RX_SIZE + (RX_SIZE - remaining);
}
void txIrq(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b)
    {
        return;
    }
    const bool error = txError(*b);
    const bool complete = txComplete(*b);
    clearTxFlags(*b);
    if (error)
    {
        b->failed = true;
        stopTx(*b);
        startRx(*b);
        LL_GPIO_ResetOutputPin(b->directionPort, b->directionPin);
    }
    else if (complete)
    {
        LL_USART_DisableDMAReq_TX(uart);
        LL_DMA_DisableIT_TC(DMA1, b->txChannel);
    }
}
void rxIrq(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b)
    {
        return;
    }
    const bool error = rxError(*b);
    const bool half = rxHalfComplete(*b);
    const bool complete = rxComplete(*b);
    clearRxFlags(*b);
    // HT and TC pending together indicate that servicing fell behind by
    // at least half a buffer. Reject possibly overwritten data.
    if (error || (half && complete))
    {
        b->failed = true;
        stopRx(*b);
    }
    else if (complete)
    {
        ++b->wraps;
    }
}
void usartIrq(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b)
    {
        return;
    }
    if (LL_USART_IsEnabledIT_TC(uart) && LL_USART_IsActiveFlag_TC(uart))
    {
        LL_USART_ClearFlag_TC(uart);
        stopTx(*b);
        startRx(*b);
        LL_GPIO_ResetOutputPin(b->directionPort, b->directionPin);
        b->done = true;
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
    if (error)
    {
        b->failed = true;
        stopRx(*b);
        stopTx(*b);
        LL_GPIO_ResetOutputPin(b->directionPort, b->directionPin);
    }
}
} // namespace

extern "C" bool dxlDmaStartTransmit(USART_TypeDef* uart, const uint8_t* data, uint16_t length, uint32_t timeoutMs)
{
    Bus* b = findBus(uart);
    if (!b || !data || length == 0U || timeoutMs == 0U || !LL_USART_IsEnabled(uart))
    {
        return false;
    }

    // Claim TX atomically against interrupt-driven callers.
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    const bool alreadyBusy = b->busy;
    if (!alreadyBusy)
    {
        b->busy = true;
    }
    __set_PRIMASK(primask);
    if (alreadyBusy)
    {
        return false;
    }
    b->done = false;
    b->failed = false;
    b->txStarted = HAL_GetTick();
    b->txTimeout = timeoutMs;
    stopRx(*b);

    // The RX DMA consumer is stopped, so discarding stale RDR bytes is safe.
    while (LL_USART_IsActiveFlag_RXNE(uart))
    {
        (void)LL_USART_ReceiveData8(uart);
    }
    LL_USART_ClearFlag_ORE(uart);
    LL_USART_ClearFlag_FE(uart);
    LL_USART_ClearFlag_NE(uart);

    // Keep the external transceiver driving until USART TC, not DMA TC.
    LL_GPIO_SetOutputPin(b->directionPort, b->directionPin);
    LL_DMA_DisableChannel(DMA1, b->txChannel);
    LL_DMA_ConfigAddresses(DMA1,
                           b->txChannel,
                           reinterpret_cast<uint32_t>(data),
                           LL_USART_DMA_GetRegAddr(uart, LL_USART_DMA_REG_DATA_TRANSMIT),
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(DMA1, b->txChannel, length);
    clearTxFlags(*b);
    LL_USART_ClearFlag_TC(uart);
    LL_DMA_EnableIT_TC(DMA1, b->txChannel);
    LL_DMA_EnableIT_TE(DMA1, b->txChannel);
    LL_USART_EnableIT_TC(uart);
    LL_DMA_EnableChannel(DMA1, b->txChannel);
    LL_USART_EnableDMAReq_TX(uart);
    return true;
}

extern "C" DxlDmaTxStatus dxlDmaPollTransmit(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b)
    {
        return DXL_DMA_TX_FAILED;
    }
    if (!b->busy)
    {
        if (b->failed) return DXL_DMA_TX_FAILED;
        return b->done ? DXL_DMA_TX_COMPLETE : DXL_DMA_TX_IDLE;
    }
    if (b->failed)
    {
        stopTx(*b);
        startRx(*b);
        LL_GPIO_ResetOutputPin(b->directionPort, b->directionPin);
        b->busy = false;
        return DXL_DMA_TX_FAILED;
    }
    if (b->done)
    {
        b->busy = false;
        return DXL_DMA_TX_COMPLETE;
    }
    if ((HAL_GetTick() - b->txStarted) >= b->txTimeout)
    {
        // A timed-out transmit must not leave the level shifter in TX mode.
        stopTx(*b);
        LL_GPIO_ResetOutputPin(b->directionPort, b->directionPin);
        startRx(*b);
        b->failed = true;
        b->busy = false;
        return DXL_DMA_TX_FAILED;
    }
    return DXL_DMA_TX_BUSY;
}

extern "C" void dxlDmaAbort(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (!b) return;
    stopTx(*b);
    stopRx(*b);
    LL_GPIO_ResetOutputPin(b->directionPort, b->directionPin);
    startRx(*b);
    b->failed = true;
    b->done = false;
    b->busy = false;
}

extern "C" bool dxlDmaTransmit(USART_TypeDef* uart, const uint8_t* data, uint16_t length, uint32_t timeoutMs)
{
    if (!dxlDmaStartTransmit(uart, data, length, timeoutMs))
    {
        return false;
    }
    DxlDmaTxStatus status;
    do
    {
        status = dxlDmaPollTransmit(uart);
    } while (status == DXL_DMA_TX_BUSY);
    return status == DXL_DMA_TX_COMPLETE;
}
extern "C" bool dxlDmaReceiveByte(USART_TypeDef* uart, uint8_t* value)
{
    Bus* b = findBus(uart);
    if (!b || !value || b->busy || b->failed)
    {
        return false;
    }
    const uint32_t producedCount = produced(*b);
    const uint32_t pending = producedCount - b->consumed;
    if (pending > RX_SIZE)
    {
        b->failed = true;
        return false;
    }
    if (pending == 0)
    {
        return false;
    }
    *value = b->rx[b->consumed % RX_SIZE];
    ++b->consumed;
    return true;
}
extern "C" void dxlDmaDiscardRx(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    if (b && !b->busy)
    {
        b->consumed = produced(*b);
    }
}
extern "C" bool dxlDmaRxFailed(USART_TypeDef* uart)
{
    Bus* b = findBus(uart);
    return !b || b->failed;
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
    usartIrq(USART2);
}
extern "C" void dxlDmaUsartIrq3(void)
{
    usartIrq(USART3);
}
