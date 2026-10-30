#ifndef DYNAMIXEL_DMA_H
#define DYNAMIXEL_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* USART2 DMA1 channels 1/2 and USART3 DMA1 channels 3/4. */
    bool dxlDmaTransmit(USART_TypeDef* usart, const uint8_t* data, uint16_t length, uint32_t timeoutMs);
    bool dxlDmaReceiveByte(USART_TypeDef* usart, uint8_t* value);
    void dxlDmaDiscardRx(USART_TypeDef* usart);
    void dxlDmaTxIrq2(void);
    void dxlDmaTxIrq3(void);
    void dxlDmaRxIrq2(void);
    void dxlDmaRxIrq3(void);
    void dxlDmaUsartIrq2(void);
    void dxlDmaUsartIrq3(void);

#ifdef __cplusplus
}
#endif
#endif
