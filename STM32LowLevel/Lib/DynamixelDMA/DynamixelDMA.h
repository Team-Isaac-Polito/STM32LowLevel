#ifndef DYNAMIXEL_DMA_H
#define DYNAMIXEL_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* USART2 only: DMA1 channel 1 TX, channel 2 RX. All operations are foreground
 * except the short DMA/USART interrupt handlers. Shared by all motors on bus 1. */
bool dxlDmaTransmit(USART_TypeDef *usart, const uint8_t *data, uint16_t length,
                    uint32_t timeoutMs);
bool dxlDmaReceiveByte(USART_TypeDef *usart, uint8_t *value);
void dxlDmaDiscardRx(USART_TypeDef *usart);
void dxlDmaTxIrq(void);
void dxlDmaRxIrq(void);
void dxlDmaUsartIrq(void);

#ifdef __cplusplus
}
#endif
#endif
