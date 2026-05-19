/**
 * @file    usbd_cdc.h
 * @brief   USB CDC class header.
 */

#ifndef USBD_CDC_H
#define USBD_CDC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "usbd_ioreq.h"

/* CDC endpoints — must match usbd_cdc_if.c */
#define CDC_IN_EP 0x81U  /* EP1 IN (data)  */
#define CDC_OUT_EP 0x01U /* EP1 OUT (data) */
#define CDC_CMD_EP 0x82U /* EP2 IN (cmd)   */

#define CDC_DATA_HS_MAX_PACKET_SIZE 512U
#define CDC_DATA_FS_MAX_PACKET_SIZE 64U
#define CDC_CMD_PACKET_SIZE 8U

#define CDC_DATA_FS_IN_PACKET_SIZE CDC_DATA_FS_MAX_PACKET_SIZE
#define CDC_DATA_FS_OUT_PACKET_SIZE CDC_DATA_FS_MAX_PACKET_SIZE

/* CDC class-specific requests */
#define CDC_SEND_ENCAPSULATED_COMMAND 0x00U
#define CDC_GET_ENCAPSULATED_RESPONSE 0x01U
#define CDC_SET_COMM_FEATURE 0x02U
#define CDC_GET_COMM_FEATURE 0x03U
#define CDC_CLEAR_COMM_FEATURE 0x04U
#define CDC_SET_LINE_CODING 0x20U
#define CDC_GET_LINE_CODING 0x21U
#define CDC_SET_CONTROL_LINE_STATE 0x22U
#define CDC_SEND_BREAK 0x23U

    /* CDC line coding */
    typedef struct
    {
        uint32_t bitrate;
        uint8_t format;
        uint8_t paritytype;
        uint8_t datatype;
    } USBD_CDC_LineCodingTypeDef;

    /* CDC interface structure */
    typedef struct
    {
        uint32_t data[CDC_DATA_FS_MAX_PACKET_SIZE / 4U];
        uint8_t cmd[CDC_CMD_PACKET_SIZE];
        uint32_t dataRxLength;
        uint32_t dataTxLength;
        uint8_t* RxBuffer;
        uint8_t* TxBuffer;
        volatile uint8_t TxState;
        volatile uint8_t RxState;
    } USBD_CDC_HandleTypeDef;

    /* CDC interface callbacks */
    typedef struct
    {
        uint8_t (*Init)(void);
        uint8_t (*DeInit)(void);
        uint8_t (*Control)(uint8_t cmd, uint8_t* pbuf, uint16_t length);
        uint8_t (*Receive)(uint8_t* Buf, uint32_t* Len);
        uint8_t (*TransmitCplt)(uint8_t* Buf, uint32_t* Len, uint8_t epnum);
    } USBD_CDC_ItfTypeDef;

    extern USBD_ClassTypeDef USBD_CDC;

    uint8_t USBD_CDC_SetTxBuffer(USBD_HandleTypeDef* pdev, uint8_t* pbuff, uint32_t length);
    uint8_t USBD_CDC_SetRxBuffer(USBD_HandleTypeDef* pdev, uint8_t* pbuff);
    uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef* pdev);
    uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef* pdev);
    void USBD_CDC_RegisterInterface(USBD_HandleTypeDef* pdev, USBD_CDC_ItfTypeDef* fops);

#ifdef __cplusplus
}
#endif

#endif /* USBD_CDC_H */
