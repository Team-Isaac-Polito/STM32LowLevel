/**
 * @file    usbd_ioreq.c
 * @brief   USB Device I/O request helper.
 */

#include "usbd_ioreq.h"
#include "usbd_core.h"

/**
 * @brief  Sends data on the control endpoint.
 */
USBD_StatusTypeDef USBD_CtlSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len)
{
    uint32_t packet_len = MIN(len, USB_MAX_EP0_SIZE);

    pdev->ep0_state = USBD_EP0_DATA_IN;
    pdev->ep0_data_len = len;
    pdev->ep_in[0].total_length = len;
    pdev->ep_in[0].rem_length = len;

    if (pdev->ep_in[0].rem_length >= packet_len)
    {
        pdev->ep_in[0].rem_length -= packet_len;
    }
    else
    {
        pdev->ep_in[0].rem_length = 0U;
    }

    USBD_LL_Transmit(pdev, 0x00, pbuf, packet_len);

    return USBD_OK;
}

/**
 * @brief  Continues sending data on the control endpoint.
 */
USBD_StatusTypeDef USBD_CtlContinueSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len)
{
    USBD_LL_Transmit(pdev, 0x00, pbuf, len);

    return USBD_OK;
}

/**
 * @brief  Prepares the control endpoint for reception.
 */
USBD_StatusTypeDef USBD_CtlPrepareRx(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len)
{
    uint32_t packet_len = MIN(len, USB_MAX_EP0_SIZE);

    pdev->ep0_state = USBD_EP0_DATA_OUT;
    pdev->ep0_data_len = len;
    pdev->ep_out[0].total_length = len;
    pdev->ep_out[0].rem_length = len;

    if (pdev->ep_out[0].rem_length >= packet_len)
    {
        pdev->ep_out[0].rem_length -= packet_len;
    }
    else
    {
        pdev->ep_out[0].rem_length = 0U;
    }

    USBD_LL_PrepareReceive(pdev, 0x00, pbuf, packet_len);

    return USBD_OK;
}

/**
 * @brief  Receives status on the control endpoint.
 */
USBD_StatusTypeDef USBD_CtlReceiveStatus(USBD_HandleTypeDef *pdev)
{
    pdev->ep0_state = USBD_EP0_STATUS_OUT;

    USBD_LL_PrepareReceive(pdev, 0x00, NULL, 0U);

    return USBD_OK;
}

/**
 * @brief  Sends status on the control endpoint.
 */
USBD_StatusTypeDef USBD_CtlSendStatus(USBD_HandleTypeDef *pdev)
{
    pdev->ep0_state = USBD_EP0_STATUS_IN;

    USBD_LL_Transmit(pdev, 0x00, NULL, 0U);

    return USBD_OK;
}

/**
 * @brief  Returns the last received packet size.
 */
uint32_t USBD_GetRxCount(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return USBD_LL_GetRxDataSize(pdev, ep_addr);
}
