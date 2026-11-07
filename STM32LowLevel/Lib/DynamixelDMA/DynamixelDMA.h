#ifndef DYNAMIXEL_DMA_H
#define DYNAMIXEL_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DXL_DMA_MAX_PACKET 256U
#define DXL_DMA_MAX_FRAME 128U
#define DXL_DMA_MAX_REPLIES 8U

    /** @brief State of one USART's DMA-backed request/response transaction. */
    typedef enum
    {
        DXL_DMA_IDLE,
        DXL_DMA_SENDING,
        DXL_DMA_RECEIVING,
        DXL_DMA_COMPLETE,
        DXL_DMA_ERROR
    } DxlDmaState;

    /**
    * @brief Start an exclusive, interrupt-driven transaction.
    * @param uart USART2 or USART3.
    * @param packet Dynamixel instruction packet. Copied to an owned DMA buffer.
    * @param length Packet length, 1..DXL_DMA_MAX_PACKET bytes.
    * @param replies Expected response frames (0..DXL_DMA_MAX_REPLIES).
    * @param timeoutMs Total transaction timeout including all response frames.
    * @return True when the bus was idle and the transaction was accepted.
    * @note Call dxlDmaService() from foreground code to process received frames.
    *       The bus remains owned until the last response is consumed or cancelled.
    */
    bool dxlDmaStart(USART_TypeDef* uart, const uint8_t* packet, uint16_t length, uint8_t replies, uint32_t timeoutMs);

    /**
    * @brief Advance RX framing and check timeouts without blocking.
    * @param uart USART2 or USART3.
    * @return The current transaction state.
    * @note Processes only data already received by DMA; may be called every loop.
    */
    DxlDmaState dxlDmaService(USART_TypeDef* uart);

    /**
    * @brief Retrieve one completed raw Dynamixel response frame.
    * @param uart USART2 or USART3.
    * @param[out] frame Destination with at least DXL_DMA_MAX_FRAME bytes.
    * @param[out] length Number of bytes copied.
    * @return True when a frame was available.
    * @note Responses are returned in arrival order after ID and CRC validation.
    */
    bool dxlDmaReadFrame(USART_TypeDef* uart, uint8_t* frame, uint16_t* length);

    /**
    * @brief Stop the transaction, clear errors and return the transceiver to RX.
    * @param uart USART2 or USART3.
    * @note Also releases response ownership after a timeout or malformed packet.
    */
    void dxlDmaCancel(USART_TypeDef* uart);

    /** @brief Handle DMA1 CH1 TX completion/error for USART2. */
    void dxlDmaTxIrq2(void);

    /** @brief Handle DMA1 CH3 TX completion/error for USART3. */
    void dxlDmaTxIrq3(void);

    /** @brief Handle DMA1 CH2 RX wrap/error for USART2. */
    void dxlDmaRxIrq2(void);

    /** @brief Handle DMA1 CH4 RX wrap/error for USART3. */
    void dxlDmaRxIrq3(void);

    /** @brief Handle USART2 TC, IDLE and receive-error interrupts. */
    void dxlDmaUsartIrq2(void);

    /** @brief Handle USART3 TC, IDLE and receive-error interrupts. */
    void dxlDmaUsartIrq3(void);

#ifdef __cplusplus
}
#endif
#endif
