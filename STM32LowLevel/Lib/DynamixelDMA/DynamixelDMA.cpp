#include "DynamixelDMA.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_hal.h"

/* Circular RX buffer holds DMA-written bytes while foreground parses packets.
 * No per-byte ISR. Only one synchronous transaction may own USART2 at a time. */
namespace
{
constexpr uint16_t RX_SIZE2 = 256;
alignas(4) uint8_t rxBuffer2[RX_SIZE2];
volatile bool txActive2 = false;
volatile bool txFinished2 = false;
volatile bool txFailed2 = false;
uint16_t rxConsumed2 = 0;

uint16_t rxWritten2()
{
    return static_cast<uint16_t>((RX_SIZE2 - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_2)) % RX_SIZE2);
}
void stopRx2()
{
    LL_USART_DisableDMAReq_RX(USART2);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    rxConsumed2 = 0;
}
void startRx2()
{
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_CIRCULAR);
    LL_DMA_ConfigAddresses(DMA1,
                           LL_DMA_CHANNEL_2,
                           LL_USART_DMA_GetRegAddr(USART2, LL_USART_DMA_REG_DATA_RECEIVE),
                           reinterpret_cast<uint32_t>(rxBuffer2),
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, RX_SIZE2);
    LL_DMA_ClearFlag_GI2(DMA1);
    rxConsumed2 = 0;
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
    LL_USART_EnableDMAReq_RX(USART2);
}
void stopTx2()
{
    LL_USART_DisableDMAReq_TX(USART2);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableIT_TE(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_USART_DisableIT_TC(USART2);
}
} // namespace

extern "C" bool dxlDmaTransmit2(USART_TypeDef* usart, const uint8_t* data, uint16_t length, uint32_t timeoutMs)
{
    if (usart != USART2 || data == nullptr || length == 0 || txActive2 || !LL_USART_IsEnabled(USART2))
    {
        return false;
    }

    txActive2 = true;
    txFinished2 = false;
    txFailed2 = false;
    stopRx2();
    while (LL_USART_IsActiveFlag_RXNE(USART2))
    {
        (void)LL_USART_ReceiveData8(USART2);
    }
    LL_USART_ClearFlag_ORE(USART2);
    LL_USART_ClearFlag_FE(USART2);
    LL_USART_ClearFlag_NE(USART2);

    /* Direction must stay TX until the USART TC event, not DMA TC.
     * DMA TC only means the final byte was written to TDR. */
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ConfigAddresses(DMA1,
                           LL_DMA_CHANNEL_1,
                           reinterpret_cast<uint32_t>(data),
                           LL_USART_DMA_GetRegAddr(USART2, LL_USART_DMA_REG_DATA_TRANSMIT),
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_1, length);
    LL_DMA_ClearFlag_GI1(DMA1);
    LL_USART_ClearFlag_TC(USART2);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_1);
    LL_USART_EnableIT_TC(USART2);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_USART_EnableDMAReq_TX(USART2);

    const uint32_t started = HAL_GetTick();
    while (!txFinished2 && !txFailed2)
    {
        if ((HAL_GetTick() - started) >= timeoutMs)
        {
            txFailed2 = true;
            break;
        }
    }
    if (txFailed2)
    {
        stopTx2();
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1);
        startRx2();
    }
    bool ok = txFinished2 && !txFailed2;
    txActive2 = false;
    return ok;
}

extern "C" bool dxlDmaReceiveByte2(USART_TypeDef* usart, uint8_t* value)
{
    if (usart != USART2 || value == nullptr || txActive2)
    {
        return false;
    }
    uint16_t written = rxWritten2();
    if (rxConsumed2 == written)
    {
        return false;
    }
    *value = rxBuffer2[rxConsumed2];
    rxConsumed2 = (rxConsumed2 + 1u) % RX_SIZE2;
    return true;
}

extern "C" void dxlDmaDiscardRx2(USART_TypeDef* usart)
{
    if (usart == USART2 && !txActive2)
    {
        rxConsumed2 = rxWritten2();
    }
}

extern "C" void dxlDmaTxIrq2(void)
{
    if (LL_DMA_IsActiveFlag_TE1(DMA1))
    {
        LL_DMA_ClearFlag_TE1(DMA1);
        txFailed2 = true;
        stopTx2();
    }
    if (LL_DMA_IsActiveFlag_TC1(DMA1))
    {
        LL_DMA_ClearFlag_TC1(DMA1);
        /* USART TC ISR will signal actual wire completion. */
        LL_USART_DisableDMAReq_TX(USART2);
        LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    }
}

extern "C" void dxlDmaRxIrq2(void)
{
    /* Normal circular wrap and half-buffer conditions require no CPU copy. */
    LL_DMA_ClearFlag_GI2(DMA1);
}

extern "C" void dxlDmaUsartIrq2(void)
{
    if (LL_USART_IsEnabledIT_TC(USART2) && LL_USART_IsActiveFlag_TC(USART2))
    {
        LL_USART_ClearFlag_TC(USART2);
        stopTx2();
        /* Prepare DMA before exposing the RX side of the transceiver. */
        startRx2();
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1);
        txFinished2 = true;
    }
    if (LL_USART_IsActiveFlag_ORE(USART2))
    {
        LL_USART_ClearFlag_ORE(USART2);
    }
    if (LL_USART_IsActiveFlag_FE(USART2))
    {
        LL_USART_ClearFlag_FE(USART2);
    }
    if (LL_USART_IsActiveFlag_NE(USART2))
    {
        LL_USART_ClearFlag_NE(USART2);
    }
}

/* Independent USART3 DMA context: DMA1 channels 3/4; PB14 direction. */
namespace
{
constexpr uint16_t RX_SIZE3 = 256;
alignas(4) uint8_t rxBuffer3[RX_SIZE3];
volatile bool txActive3 = false;
volatile bool txFinished3 = false;
volatile bool txFailed3 = false;
uint16_t rxConsumed3 = 0;

uint16_t rxWritten3()
{
    return static_cast<uint16_t>((RX_SIZE3 - LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_4)) % RX_SIZE3);
}
void stopRx3()
{
    LL_USART_DisableDMAReq_RX(USART3);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
    rxConsumed3 = 0;
}
void startRx3()
{
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_4);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_4, LL_DMA_MODE_CIRCULAR);
    LL_DMA_ConfigAddresses(DMA1,
                           LL_DMA_CHANNEL_4,
                           LL_USART_DMA_GetRegAddr(USART3, LL_USART_DMA_REG_DATA_RECEIVE),
                           reinterpret_cast<uint32_t>(rxBuffer3),
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_4, RX_SIZE3);
    LL_DMA_ClearFlag_GI4(DMA1);
    rxConsumed3 = 0;
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_4);
    LL_USART_EnableDMAReq_RX(USART3);
}
void stopTx3()
{
    LL_USART_DisableDMAReq_TX(USART3);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_DisableIT_TE(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    LL_USART_DisableIT_TC(USART3);
}
} // namespace

extern "C" bool dxlDmaTransmit3(USART_TypeDef* usart, const uint8_t* data, uint16_t length, uint32_t timeoutMs)
{
    if (usart != USART3 || data == nullptr || length == 0 || txActive3 || !LL_USART_IsEnabled(USART3))
    {
        return false;
    }

    txActive3 = true;
    txFinished3 = false;
    txFailed3 = false;
    stopRx3();
    while (LL_USART_IsActiveFlag_RXNE(USART3))
    {
        (void)LL_USART_ReceiveData8(USART3);
    }
    LL_USART_ClearFlag_ORE(USART3);
    LL_USART_ClearFlag_FE(USART3);
    LL_USART_ClearFlag_NE(USART3);

    /* Direction must stay TX until the USART TC event, not DMA TC.
     * DMA TC only means the final byte was written to TDR. */
    LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_14);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_ConfigAddresses(DMA1,
                           LL_DMA_CHANNEL_3,
                           reinterpret_cast<uint32_t>(data),
                           LL_USART_DMA_GetRegAddr(USART3, LL_USART_DMA_REG_DATA_TRANSMIT),
                           LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, length);
    LL_DMA_ClearFlag_GI3(DMA1);
    LL_USART_ClearFlag_TC(USART3);
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_3);
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_3);
    LL_USART_EnableIT_TC(USART3);
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);
    LL_USART_EnableDMAReq_TX(USART3);

    const uint32_t started = HAL_GetTick();
    while (!txFinished3 && !txFailed3)
    {
        if ((HAL_GetTick() - started) >= timeoutMs)
        {
            txFailed3 = true;
            break;
        }
    }
    if (txFailed3)
    {
        stopTx3();
        LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_14);
        startRx3();
    }
    bool ok = txFinished3 && !txFailed3;
    txActive3 = false;
    return ok;
}

extern "C" bool dxlDmaReceiveByte3(USART_TypeDef* usart, uint8_t* value)
{
    if (usart != USART3 || value == nullptr || txActive3)
    {
        return false;
    }
    uint16_t written = rxWritten3();
    if (rxConsumed3 == written)
    {
        return false;
    }
    *value = rxBuffer3[rxConsumed3];
    rxConsumed3 = (rxConsumed3 + 1u) % RX_SIZE3;
    return true;
}

extern "C" void dxlDmaDiscardRx3(USART_TypeDef* usart)
{
    if (usart == USART3 && !txActive3)
    {
        rxConsumed3 = rxWritten3();
    }
}

extern "C" void dxlDmaTxIrq3(void)
{
    if (LL_DMA_IsActiveFlag_TE3(DMA1))
    {
        LL_DMA_ClearFlag_TE3(DMA1);
        txFailed3 = true;
        stopTx3();
    }
    if (LL_DMA_IsActiveFlag_TC3(DMA1))
    {
        LL_DMA_ClearFlag_TC3(DMA1);
        /* USART TC ISR will signal actual wire completion. */
        LL_USART_DisableDMAReq_TX(USART3);
        LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_3);
    }
}

extern "C" void dxlDmaRxIrq3(void)
{
    /* Normal circular wrap and half-buffer conditions require no CPU copy. */
    LL_DMA_ClearFlag_GI4(DMA1);
}

extern "C" void dxlDmaUsartIrq3(void)
{
    if (LL_USART_IsEnabledIT_TC(USART3) && LL_USART_IsActiveFlag_TC(USART3))
    {
        LL_USART_ClearFlag_TC(USART3);
        stopTx3();
        /* Prepare DMA before exposing the RX side of the transceiver. */
        startRx3();
        LL_GPIO_ResetOutputPin(GPIOB, LL_GPIO_PIN_14);
        txFinished3 = true;
    }
    if (LL_USART_IsActiveFlag_ORE(USART3))
    {
        LL_USART_ClearFlag_ORE(USART3);
    }
    if (LL_USART_IsActiveFlag_FE(USART3))
    {
        LL_USART_ClearFlag_FE(USART3);
    }
    if (LL_USART_IsActiveFlag_NE(USART3))
    {
        LL_USART_ClearFlag_NE(USART3);
    }
}

extern "C" bool dxlDmaTransmit(USART_TypeDef* usart, const uint8_t* data, uint16_t length, uint32_t timeoutMs)
{
    if (usart == USART2)
    {
        return dxlDmaTransmit2(usart, data, length, timeoutMs);
    }
    if (usart == USART3)
    {
        return dxlDmaTransmit3(usart, data, length, timeoutMs);
    }
    return false;
}
extern "C" bool dxlDmaReceiveByte(USART_TypeDef* usart, uint8_t* value)
{
    if (usart == USART2)
    {
        return dxlDmaReceiveByte2(usart, value);
    }
    if (usart == USART3)
    {
        return dxlDmaReceiveByte3(usart, value);
    }
    return false;
}
extern "C" void dxlDmaDiscardRx(USART_TypeDef* usart)
{
    if (usart == USART2)
    {
        dxlDmaDiscardRx2(usart);
    }
    else if (usart == USART3)
    {
        dxlDmaDiscardRx3(usart);
    }
}
