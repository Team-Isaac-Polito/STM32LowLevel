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
     * @note Blocking foreground API: interrupts must be enabled; not for ISR use.
     *       See the nonblocking transmit API for cooperative scheduling.
     */
    bool dxlDmaTransmit(USART_TypeDef* usart, const uint8_t* data, uint16_t length, uint32_t timeoutMs);

    /**
     * @brief State of a DMA transmission when checked without blocking.
     */
    typedef enum
    {
        DXL_DMA_TX_IDLE,     /**< No transmission has been started. */
        DXL_DMA_TX_BUSY,     /**< DMA or USART is still transmitting. */
        DXL_DMA_TX_COMPLETE, /**< USART TC occurred; RX DMA is armed. */
        DXL_DMA_TX_FAILED    /**< DMA/USART failure or transmission timeout. */
    } DxlDmaTxStatus;

    /**
     * @brief Start a DMA transmission without waiting for completion.
     * @param usart USART2 or USART3.
     * @param data Packet stored in DMA-accessible SRAM.
     * @param length Number of bytes to send.
     * @param timeoutMs Maximum time allowed for the TX operation.
     * @return True when the channel accepted the transfer.
     * @note The data buffer must remain valid and unchanged until
     *       dxlDmaPollTransmit() returns COMPLETE or FAILED.
     * @note Only one active transmission per USART is supported.
     */
    bool dxlDmaStartTransmit(USART_TypeDef* usart, const uint8_t* data, uint16_t length, uint32_t timeoutMs);

    /**
     * @brief Check DMA/USART TX completion without waiting.
     * @param usart USART2 or USART3.
     * @return Current TX status; FAILED includes a timed-out transfer.
     * @note Call periodically when using the asynchronous API so timeouts
     *       are enforced and a completed transfer can be acknowledged.
     */
    DxlDmaTxStatus dxlDmaPollTransmit(USART_TypeDef* usart);

    /**
     * @brief Abort TX and restore the bus to RX direction.
     * @param usart USART2 or USART3.
     * @note Cancels any pending TX and invalidates its completion result.
     */
    void dxlDmaAbort(USART_TypeDef* usart);

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
