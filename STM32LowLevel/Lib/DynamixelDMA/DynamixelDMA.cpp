#include "DynamixelDMA.h"
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_usart.h"
#include "stm32g4xx_ll_gpio.h"
#include "stm32g4xx_hal.h"

/* Circular RX buffer holds DMA-written bytes while foreground parses packets.
 * No per-byte ISR. Only one synchronous transaction may own USART2 at a time. */
namespace {
constexpr uint16_t RX_SIZE = 256;
alignas(4) uint8_t rxBuffer[RX_SIZE];
volatile bool txActive = false;
volatile bool txFinished = false;
volatile bool txFailed = false;
uint16_t rxConsumed = 0;

uint16_t rxWritten()
{
    return static_cast<uint16_t>((RX_SIZE -
        LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_2)) % RX_SIZE);
}
void stopRx()
{
    LL_USART_DisableDMAReq_RX(USART2);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    rxConsumed = 0;
}
void startRx()
{
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
    LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_2, LL_DMA_MODE_CIRCULAR);
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_2,
                           LL_USART_DMA_GetRegAddr(USART2, LL_USART_DMA_REG_DATA_RECEIVE),
                           reinterpret_cast<uint32_t>(rxBuffer),
                           LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, RX_SIZE);
    LL_DMA_ClearFlag_GI2(DMA1);
    rxConsumed = 0;
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);
    LL_USART_EnableDMAReq_RX(USART2);
}
void stopTx()
{
    LL_USART_DisableDMAReq_TX(USART2);
    LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableIT_TE(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_USART_DisableIT_TC(USART2);
}
}

extern "C" bool dxlDmaTransmit(USART_TypeDef *usart, const uint8_t *data,
                                uint16_t length, uint32_t timeoutMs)
{
    if (usart != USART2 || data == nullptr || length == 0 || txActive ||
        !LL_USART_IsEnabled(USART2)) return false;

    txActive = true;
    txFinished = false;
    txFailed = false;
    stopRx();
    while (LL_USART_IsActiveFlag_RXNE(USART2))
        (void)LL_USART_ReceiveData8(USART2);
    LL_USART_ClearFlag_ORE(USART2);
    LL_USART_ClearFlag_FE(USART2);
    LL_USART_ClearFlag_NE(USART2);

    /* Direction must stay TX until the USART TC event, not DMA TC.
     * DMA TC only means the final byte was written to TDR. */
    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_1);
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);
    LL_DMA_ConfigAddresses(DMA1, LL_DMA_CHANNEL_1,
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
    while (!txFinished && !txFailed) {
        if ((HAL_GetTick() - started) >= timeoutMs) {
            txFailed = true;
            break;
        }
    }
    if (txFailed) {
        stopTx();
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1);
        startRx();
    }
    bool ok = txFinished && !txFailed;
    txActive = false;
    return ok;
}

extern "C" bool dxlDmaReceiveByte(USART_TypeDef *usart, uint8_t *value)
{
    if (usart != USART2 || value == nullptr || txActive) return false;
    uint16_t written = rxWritten();
    if (rxConsumed == written) return false;
    *value = rxBuffer[rxConsumed];
    rxConsumed = (rxConsumed + 1u) % RX_SIZE;
    return true;
}

extern "C" void dxlDmaDiscardRx(USART_TypeDef *usart)
{
    if (usart == USART2 && !txActive) rxConsumed = rxWritten();
}

extern "C" void dxlDmaTxIrq(void)
{
    if (LL_DMA_IsActiveFlag_TE1(DMA1)) {
        LL_DMA_ClearFlag_TE1(DMA1);
        txFailed = true;
        stopTx();
    }
    if (LL_DMA_IsActiveFlag_TC1(DMA1)) {
        LL_DMA_ClearFlag_TC1(DMA1);
        /* USART TC ISR will signal actual wire completion. */
        LL_USART_DisableDMAReq_TX(USART2);
        LL_DMA_DisableIT_TC(DMA1, LL_DMA_CHANNEL_1);
    }
}

extern "C" void dxlDmaRxIrq(void)
{
    /* Normal circular wrap and half-buffer conditions require no CPU copy. */
    LL_DMA_ClearFlag_GI2(DMA1);
}

extern "C" void dxlDmaUsartIrq(void)
{
    if (LL_USART_IsEnabledIT_TC(USART2) && LL_USART_IsActiveFlag_TC(USART2)) {
        LL_USART_ClearFlag_TC(USART2);
        stopTx();
        /* Prepare DMA before exposing the RX side of the transceiver. */
        startRx();
        LL_GPIO_ResetOutputPin(GPIOA, LL_GPIO_PIN_1);
        txFinished = true;
    }
    if (LL_USART_IsActiveFlag_ORE(USART2)) LL_USART_ClearFlag_ORE(USART2);
    if (LL_USART_IsActiveFlag_FE(USART2)) LL_USART_ClearFlag_FE(USART2);
    if (LL_USART_IsActiveFlag_NE(USART2)) LL_USART_ClearFlag_NE(USART2);
}
