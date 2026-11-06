#ifndef DYNAMIXEL_DMA_H
#define DYNAMIXEL_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Transmit a Dynamixel packet using DMA and wait for USART TC.
     * @param usart USART2 (DMA1 CH1/2) or USART3 (DMA1 CH3/4).
     * @param data Packet buffer; must remain readable until this call returns.
     * @param length Number of bytes in the packet.
     * @param timeoutMs Maximum transmission wait, in milliseconds.
     * @return True if the final UART frame was transmitted; false on error or timeout.
     * @note This compatibility API blocks. See the nonblocking transport APIs.
     */
    bool dxlDmaTransmit(USART_TypeDef* usart, const uint8_t* data, uint16_t length, uint32_t timeoutMs);

    /**
     * @brief Read the oldest unconsumed byte from the circular RX DMA buffer.
     * @param usart USART2 or USART3.
     * @param[out] value Destination for one received byte.
     * @return True when a byte was read; false when empty, busy or failed.
     */
    bool dxlDmaReceiveByte(USART_TypeDef* usart, uint8_t* value);

    /**
     * @brief Check for RX DMA, USART or buffer-overrun failures.
     * @param usart USART2 or USART3.
     * @return True if the receive stream failed or the USART is unsupported.
     */
    bool dxlDmaRxFailed(USART_TypeDef* usart);

    /**
     * @brief Discard bytes already received into the DMA circular buffer.
     * @param usart USART2 or USART3.
     * @note Advances the software consumer; does not clear USART RDR.
     */
    void dxlDmaDiscardRx(USART_TypeDef* usart);

    /** @brief Handle DMA1 Channel 1 TX completion or error (USART2). */
    void dxlDmaTxIrq2(void);
    /** @brief Handle DMA1 Channel 3 TX completion or error (USART3). */
    void dxlDmaTxIrq3(void);
    /** @brief Handle DMA1 Channel 2 RX wrap or error (USART2). */
    void dxlDmaRxIrq2(void);
    /** @brief Handle DMA1 Channel 4 RX wrap or error (USART3). */
    void dxlDmaRxIrq3(void);
    /** @brief Handle USART2 transmission completion and USART errors. */
    void dxlDmaUsartIrq2(void);
    /** @brief Handle USART3 transmission completion and USART errors. */
    void dxlDmaUsartIrq3(void);

#ifdef __cplusplus
}
#endif
#endif
