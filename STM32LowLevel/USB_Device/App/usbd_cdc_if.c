/**
 * @file    usbd_cdc_if.c
 * @brief   USB CDC interface implementation (application callbacks).
 *
 * Provides a simple TX ring buffer for debug output. Data written via
 * CDC_Transmit() is buffered and sent in 64-byte USB packets.
 */

#include "usbd_cdc_if.h"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"

#include <string.h>

/* TX ring buffer */
#define CDC_TX_BUF_SIZE 1024U

static uint8_t txBuf[CDC_TX_BUF_SIZE];
static uint8_t rxBuf[CDC_DATA_FS_MAX_PACKET_SIZE];
static volatile uint32_t txHead = 0U;
static volatile uint32_t txTail = 0U;
static volatile uint8_t txActive = 0U;
static volatile uint8_t cdcConnected = 0U;

/* Line coding: 115200, 8N1 */
static uint8_t lineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08};

/* CDC interface operations */
USBD_CDC_ItfTypeDef USBD_CDC_fops = {
    CDC_Init,
    CDC_DeInit,
    CDC_Control,
    CDC_Receive,
    CDC_TransmitCplt,
};

/* USB device handle */
extern USBD_HandleTypeDef hUsbDeviceFS;

/**
 * @brief  Initializes the CDC media layer.
 */
uint8_t CDC_Init(void)
{
    txHead = 0U;
    txTail = 0U;
    txActive = 0U;
    cdcConnected = 0U;

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txBuf, 0U);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rxBuf);
    return USBD_OK;
}

/**
 * @brief  De-initializes the CDC media layer.
 */
uint8_t CDC_DeInit(void)
{
    cdcConnected = 0U;
    return USBD_OK;
}

/**
 * @brief  Handles CDC class requests.
 */
uint8_t CDC_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    switch (cmd)
    {
    case CDC_SEND_ENCAPSULATED_COMMAND:
        break;

    case CDC_GET_ENCAPSULATED_RESPONSE:
        break;

    case CDC_SET_COMM_FEATURE:
        break;

    case CDC_GET_COMM_FEATURE:
        break;

    case CDC_CLEAR_COMM_FEATURE:
        break;

    case CDC_SET_LINE_CODING:
        memcpy(lineCoding, pbuf, sizeof(lineCoding));
        break;

    case CDC_GET_LINE_CODING:
        memcpy(pbuf, lineCoding, sizeof(lineCoding));
        break;

    case CDC_SET_CONTROL_LINE_STATE:
        /* Bit 0 = DTR, Bit 1 = RTS */
        cdcConnected = (pbuf[0] & 0x01U) ? 1U : 0U;
        break;

    case CDC_SEND_BREAK:
        break;

    default:
        break;
    }
    return USBD_OK;
}

/**
 * @brief  Data received over USB OUT endpoint.
 */
uint8_t CDC_Receive(uint8_t *Buf, uint32_t *Len)
{
    /* Echo back for now — can be extended for CLI commands */
    (void)Buf;
    (void)Len;

    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

/**
 * @brief  CDC data transmit complete callback.
 */
uint8_t CDC_TransmitCplt(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    (void)Buf;
    (void)Len;
    (void)epnum;

    txActive = 0U;

    /* Send next chunk if available */
    uint32_t remaining = (txHead - txTail) & (CDC_TX_BUF_SIZE - 1U);
    if (remaining > 0U)
    {
        uint32_t chunk = remaining;
        if (chunk > CDC_DATA_FS_MAX_PACKET_SIZE)
        {
            chunk = CDC_DATA_FS_MAX_PACKET_SIZE;
        }

        /* Copy linear chunk from ring buffer */
        uint32_t first = txTail & (CDC_TX_BUF_SIZE - 1U);
        if (first + chunk <= CDC_TX_BUF_SIZE)
        {
            memcpy(txBuf, &txBuf[first], chunk);
        }
        else
        {
            uint32_t firstPart = CDC_TX_BUF_SIZE - first;
            memcpy(txBuf, &txBuf[first], firstPart);
            memcpy(&txBuf[firstPart], txBuf, chunk - firstPart);
        }

        txTail = (txTail + chunk) & (CDC_TX_BUF_SIZE - 1U);

        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txBuf, chunk);
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);
        txActive = 1U;
    }
    return USBD_OK;
}

/**
 * @brief  Returns whether the CDC host is connected (DTR set).
 */
uint8_t CDC_IsConnected(void)
{
    return cdcConnected;
}

/**
 * @brief  Transmits data over CDC. Non-blocking — data goes into a ring buffer.
 * @param  buf  Data to send
 * @param  len  Number of bytes
 * @retval 0 on success, 1 if buffer full
 */
uint8_t CDC_Transmit(const uint8_t *buf, uint32_t len)
{
    if (!cdcConnected)
    {
        return 1U;
    }

    uint32_t space = CDC_TX_BUF_SIZE - 1U - ((txHead - txTail) & (CDC_TX_BUF_SIZE - 1U));
    if (len > space)
    {
        return 1U; /* Buffer full */
    }

    for (uint32_t i = 0U; i < len; i++)
    {
        txBuf[txHead & (CDC_TX_BUF_SIZE - 1U)] = buf[i];
        txHead = (txHead + 1U) & (CDC_TX_BUF_SIZE - 1U);
    }

    /* Start transmission if not already active */
    if (!txActive)
    {
        uint32_t chunk = (txHead - txTail) & (CDC_TX_BUF_SIZE - 1U);
        if (chunk > CDC_DATA_FS_MAX_PACKET_SIZE)
        {
            chunk = CDC_DATA_FS_MAX_PACKET_SIZE;
        }

        uint32_t first = txTail & (CDC_TX_BUF_SIZE - 1U);
        if (first + chunk <= CDC_TX_BUF_SIZE)
        {
            memcpy(txBuf, &txBuf[first], chunk);
        }
        else
        {
            uint32_t firstPart = CDC_TX_BUF_SIZE - first;
            memcpy(txBuf, &txBuf[first], firstPart);
            memcpy(&txBuf[firstPart], txBuf, chunk - firstPart);
        }

        txTail = (txTail + chunk) & (CDC_TX_BUF_SIZE - 1U);

        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txBuf, chunk);
        if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK)
        {
            txActive = 1U;
        }
    }

    return 0U;
}
