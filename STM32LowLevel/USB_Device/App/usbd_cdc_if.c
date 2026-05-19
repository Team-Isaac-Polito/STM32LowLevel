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

/* DTR auto-reset: when the host opens the serial port (DTR asserted),
 * trigger a software reset so the entire boot sequence is captured
 * from the beginning. Anti-loop protection via RCC_CSR SFTRSTF flag. */
static uint8_t dtrResetDone = 0U;

/* TX ring buffer — 4096 bytes */
#define CDC_TX_BUF_SIZE 4096U

static uint8_t txBuf[CDC_TX_BUF_SIZE];
static uint8_t txPacketBuf[CDC_DATA_FS_MAX_PACKET_SIZE];
static uint8_t rxBuf[CDC_DATA_FS_MAX_PACKET_SIZE];
static volatile uint32_t txHead = 0U;
static volatile uint32_t txTail = 0U;
static volatile uint8_t txActive = 0U;
static volatile uint8_t cdcConnected = 0U;

static void copy_tx_chunk(uint8_t *dest, uint32_t first, uint32_t chunk)
{
    if (first + chunk <= CDC_TX_BUF_SIZE)
    {
        memcpy(dest, &txBuf[first], chunk);
    }
    else
    {
        uint32_t firstPart = CDC_TX_BUF_SIZE - first;
        memcpy(dest, &txBuf[first], firstPart);
        memcpy(&dest[firstPart], txBuf, chunk - firstPart);
    }
}

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

    /* Check if this is a software reset (from our DTR auto-reset).
     * If so, skip DTR auto-reset on this boot to prevent infinite loop.
     * Clear the flag afterward so future DTR assertions work. */
    if (RCC->CSR & RCC_CSR_SFTRSTF)
    {
        dtrResetDone = 1U;
        RCC->CSR |= RCC_CSR_RMVF;
    }

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txPacketBuf, 0U);
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
uint8_t CDC_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length)
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
            /* DTR auto-reset: reset the MCU when the host opens the serial
             * port so the full boot sequence is captured. Anti-loop: skip if
             * this boot was already triggered by a DTR reset. */
            if (cdcConnected && !dtrResetDone)
            {
                dtrResetDone = 1U;
                NVIC_SystemReset();
            }
            /* If DTR was just set and we have buffered data, kick the TX */
            if (cdcConnected && !txActive)
            {
                uint32_t remaining = (txHead - txTail) & (CDC_TX_BUF_SIZE - 1U);
                if (remaining > 0U)
                {
                    uint32_t chunk = remaining;
                    if (chunk > CDC_DATA_FS_MAX_PACKET_SIZE)
                        chunk = CDC_DATA_FS_MAX_PACKET_SIZE;
                    uint32_t first = txTail & (CDC_TX_BUF_SIZE - 1U);
                    copy_tx_chunk(txPacketBuf, first, chunk);
                    uint32_t savedTail = txTail;
                    txTail = (txTail + chunk) & (CDC_TX_BUF_SIZE - 1U);
                    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txPacketBuf, chunk);
                    if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK)
                    {
                        txActive = 1U;
                    }
                    else
                    {
                        txTail = savedTail;
                    }
                }
            }
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
uint8_t CDC_Receive(uint8_t* Buf, uint32_t* Len)
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
uint8_t CDC_TransmitCplt(uint8_t* Buf, uint32_t* Len, uint8_t epnum)
{
    (void)Buf;
    (void)Len;
    (void)epnum;

    txActive = 0U;

    /* Send next chunk from the ring buffer */
    uint32_t remaining = (txHead - txTail) & (CDC_TX_BUF_SIZE - 1U);
    if (remaining > 0U)
    {
        uint32_t chunk = remaining;
        if (chunk > CDC_DATA_FS_MAX_PACKET_SIZE)
            chunk = CDC_DATA_FS_MAX_PACKET_SIZE;

        uint32_t first = txTail & (CDC_TX_BUF_SIZE - 1U);
        copy_tx_chunk(txPacketBuf, first, chunk);
        uint32_t savedTail = txTail;
        txTail = (txTail + chunk) & (CDC_TX_BUF_SIZE - 1U);

        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txPacketBuf, chunk);
        if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK)
        {
            txActive = 1U;
        }
        else
        {
            txTail = savedTail;
        }
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
 * @brief  Wait for the USB host to connect (DTR asserted).
 *         Called after USB init on software reset to pause boot until
 *         the serial monitor reconnects. Returns after ~10 s timeout
 *         even if host never connects.
 */
void CDC_WaitForHost(void)
{
    uint32_t start = HAL_GetTick();

    while (!cdcConnected)
    {
        if ((HAL_GetTick() - start) > 10000U)
        {
            return;
        }
    }
}

/**
 * @brief  Transmits data over CDC. Non-blocking — data goes into a ring buffer.
 *         When the buffer is full, oldest data is overwritten so the most
 *         recent debug messages are always preserved.
 * @param  buf  Data to send
 * @param  len  Number of bytes
 * @retval 0 on success
 */
uint8_t CDC_Transmit(const uint8_t* buf, uint32_t len)
{
    /* If len exceeds the buffer size, only keep the last CDC_TX_BUF_SIZE - 1 bytes */
    if (len >= CDC_TX_BUF_SIZE)
    {
        buf = buf + len - (CDC_TX_BUF_SIZE - 1U);
        len = CDC_TX_BUF_SIZE - 1U;
    }

    /* Make room by advancing tail if buffer is full */
    uint32_t space = CDC_TX_BUF_SIZE - 1U - ((txHead - txTail) & (CDC_TX_BUF_SIZE - 1U));
    if (len > space)
    {
        uint32_t drop = len - space;
        txTail = (txTail + drop) & (CDC_TX_BUF_SIZE - 1U);
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
            chunk = CDC_DATA_FS_MAX_PACKET_SIZE;

        uint32_t first = txTail & (CDC_TX_BUF_SIZE - 1U);
        copy_tx_chunk(txPacketBuf, first, chunk);
        uint32_t savedTail = txTail;
        txTail = (txTail + chunk) & (CDC_TX_BUF_SIZE - 1U);

        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txPacketBuf, chunk);
        if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK)
            txActive = 1U;
        else
            txTail = savedTail;
    }

    return 0U;
}

/**
 * @brief  Force any buffered CDC TX data to be transmitted immediately.
 *         Call this after critical debug messages to flush the buffer.
 */
void CDC_ForceTx(void)
{
    if (txActive == 0U)
    {
        uint32_t chunk = (txHead - txTail) & (CDC_TX_BUF_SIZE - 1U);
        if (chunk > 0U)
        {
            if (chunk > CDC_DATA_FS_MAX_PACKET_SIZE)
            {
                chunk = CDC_DATA_FS_MAX_PACKET_SIZE;
            }

            uint32_t first = txTail & (CDC_TX_BUF_SIZE - 1U);
            copy_tx_chunk(txPacketBuf, first, chunk);
            uint32_t savedTail = txTail;
            txTail = (txTail + chunk) & (CDC_TX_BUF_SIZE - 1U);

            USBD_CDC_SetTxBuffer(&hUsbDeviceFS, txPacketBuf, chunk);
            if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK)
            {
                txActive = 1U;
            }
            else
            {
                txTail = savedTail;
            }
        }
    }
}
