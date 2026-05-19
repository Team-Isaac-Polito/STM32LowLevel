/**
 * @file    usbd_core.c
 * @brief   USB Device Core (simplified, CDC-only).
 */

#include "usbd_core.h"
#include "usbd_ioreq.h"
#include "usbd_ctlreq.h"
#include "usbd_conf.h"

/**
 * @brief  Initializes the USB device stack.
 */
USBD_StatusTypeDef USBD_Init(USBD_HandleTypeDef* pdev, USBD_DescriptorsTypeDef* pdesc, uint8_t id)
{
    if (pdev == NULL)
        return USBD_FAIL;

    if (pdesc != NULL)
        pdev->pDesc = pdesc;

    pdev->dev_state = USBD_DEFAULT;
    pdev->id = id;

    USBD_LL_Init(pdev);

    return USBD_OK;
}

/**
 * @brief  De-initializes the USB device stack.
 */
USBD_StatusTypeDef USBD_DeInit(USBD_HandleTypeDef* pdev)
{
    pdev->dev_state = USBD_DEFAULT;

    if (pdev->pClass != NULL)
        pdev->pClass->DeInit(pdev, pdev->dev_config);

    USBD_LL_DeInit(pdev);

    return USBD_OK;
}

/**
 * @brief  Starts the USB device.
 */
USBD_StatusTypeDef USBD_Start(USBD_HandleTypeDef* pdev)
{
    USBD_LL_Start(pdev);
    return USBD_OK;
}

/**
 * @brief  Stops the USB device.
 */
USBD_StatusTypeDef USBD_Stop(USBD_HandleTypeDef* pdev)
{
    if (pdev->pClass != NULL)
        pdev->pClass->DeInit(pdev, pdev->dev_config);

    USBD_LL_Stop(pdev);

    return USBD_OK;
}

/**
 * @brief  Registers a USB class.
 */
USBD_StatusTypeDef USBD_RegisterClass(USBD_HandleTypeDef* pdev, USBD_ClassTypeDef* pclass)
{
    USBD_StatusTypeDef ret = USBD_OK;

    if (pclass != NULL)
    {
        pdev->pClass = pclass;
        ret = USBD_OK;
    }
    else
    {
        ret = USBD_FAIL;
    }

    return ret;
}

/**
 * @brief  Handles setup stage.
 */
USBD_StatusTypeDef USBD_SetupStage(USBD_HandleTypeDef* pdev, uint8_t* psetup)
{
    USBD_ParseSetupRequest(&pdev->request, psetup);

    pdev->ep0_state = USBD_EP0_SETUP;
    pdev->ep0_data_len = pdev->request.wLength;

    switch (psetup[0] & USB_REQ_RECIPIENT_MASK)
    {
        case USB_REQ_RECIPIENT_DEVICE:
            USBD_StdDevReq(pdev, &pdev->request);
            break;

        case USB_REQ_RECIPIENT_INTERFACE:
            USBD_StdItfReq(pdev, &pdev->request);
            break;

        case USB_REQ_RECIPIENT_ENDPOINT:
            USBD_StdEPReq(pdev, &pdev->request);
            break;

        default:
            USBD_LL_StallEP(pdev, psetup[0] & 0x80U);
            break;
    }

    return USBD_OK;
}

/**
 * @brief  Handles data out stage.
 */
USBD_StatusTypeDef USBD_DataOutStage(USBD_HandleTypeDef* pdev, uint8_t epnum, uint8_t* pdata)
{
    if (epnum == 0U)
    {
        if (pdev->ep0_state == USBD_EP0_DATA_OUT)
        {
            uint32_t rx_count = USBD_GetRxCount(pdev, 0x00);

            if (pdev->ep_out[0].rem_length > 0U)
            {
                if (pdev->ep_out[0].rem_length > rx_count)
                    pdev->ep_out[0].rem_length -= rx_count;
                else
                    pdev->ep_out[0].rem_length = 0U;

                USBD_LL_PrepareReceive(pdev, 0x00, pdata + rx_count, MIN(pdev->ep_out[0].rem_length, USB_MAX_EP0_SIZE));
            }
            else
            {
                pdev->ep0_data_len = 0U;
            }

            if (pdev->pClass != NULL)
                pdev->pClass->EP0_RxReady(pdev);

            USBD_CtlSendStatus(pdev);
        }
    }
    else if (pdev->pClass != NULL)
    {
        pdev->pClass->DataOut(pdev, epnum);
    }

    return USBD_OK;
}

/**
 * @brief  Handles data in stage.
 */
USBD_StatusTypeDef USBD_DataInStage(USBD_HandleTypeDef* pdev, uint8_t epnum, uint8_t* pdata)
{
    if (epnum == 0U)
    {
        if (pdev->ep0_state == USBD_EP0_DATA_IN)
        {
            if (pdev->ep_in[0].rem_length > 0U)
            {
                uint32_t packet_len = MIN(pdev->ep_in[0].rem_length, USB_MAX_EP0_SIZE);
                pdev->ep_in[0].rem_length -= packet_len;

                USBD_CtlContinueSendData(pdev, pdata, packet_len);
                USBD_LL_PrepareReceive(pdev, 0x00, NULL, 0U);
            }
            else
            {
                if ((pdev->pClass != NULL) && (pdev->pClass->EP0_TxSent != NULL) &&
                    (pdev->dev_state == USBD_CONFIGURED))
                {
                    pdev->pClass->EP0_TxSent(pdev);
                }

                USBD_CtlReceiveStatus(pdev);
            }
        }
        else
        {
            if ((pdev->pClass != NULL) && (pdev->pClass->EP0_TxSent != NULL) && (pdev->dev_state == USBD_CONFIGURED))
                pdev->pClass->EP0_TxSent(pdev);
        }
    }
    else if (pdev->pClass != NULL)
    {
        pdev->pClass->DataIn(pdev, epnum);
    }

    return USBD_OK;
}

/**
 * @brief  Handles USB reset.
 */
USBD_StatusTypeDef USBD_Reset(USBD_HandleTypeDef* pdev)
{
    pdev->dev_config = 0U;
    pdev->dev_default_config = 0U;
    pdev->dev_state = USBD_DEFAULT;
    pdev->dev_old_state = USBD_DEFAULT;
    pdev->dev_remote_wakeup = 0U;

    USBD_LL_OpenEP(pdev, 0x00, USB_EP_TYPE_CTRL, USB_MAX_EP0_SIZE);
    USBD_LL_OpenEP(pdev, 0x80, USB_EP_TYPE_CTRL, USB_MAX_EP0_SIZE);

    if (pdev->pClass != NULL)
        pdev->pClass->DeInit(pdev, pdev->dev_config);

    return USBD_OK;
}

/**
 * @brief  Sets USB speed.
 */
USBD_StatusTypeDef USBD_SetSpeed(USBD_HandleTypeDef* pdev, USBD_SpeedTypeDef speed)
{
    pdev->dev_speed = speed;
    return USBD_OK;
}

/**
 * @brief  Handles suspend.
 */
USBD_StatusTypeDef USBD_Suspend(USBD_HandleTypeDef* pdev)
{
    pdev->dev_old_state = pdev->dev_state;
    pdev->dev_state = USBD_SUSPENDED;
    return USBD_OK;
}

/**
 * @brief  Handles resume.
 */
USBD_StatusTypeDef USBD_Resume(USBD_HandleTypeDef* pdev)
{
    pdev->dev_state = pdev->dev_old_state;
    return USBD_OK;
}

/**
 * @brief  Handles SOF.
 */
USBD_StatusTypeDef USBD_SOF(USBD_HandleTypeDef* pdev)
{
    if ((pdev->pClass != NULL) && (pdev->pClass->SOF != NULL))
        pdev->pClass->SOF(pdev);
    return USBD_OK;
}

/**
 * @brief  Device connected.
 */
USBD_StatusTypeDef USBD_DevConnected(USBD_HandleTypeDef* pdev)
{
    pdev->dev_connection_status = 1U;
    return USBD_OK;
}

/**
 * @brief  Device disconnected.
 */
USBD_StatusTypeDef USBD_DevDisconnected(USBD_HandleTypeDef* pdev)
{
    pdev->dev_connection_status = 0U;

    if (pdev->pClass != NULL)
        pdev->pClass->DeInit(pdev, pdev->dev_config);

    return USBD_OK;
}

/**
 * @brief  ISO IN incomplete.
 */
USBD_StatusTypeDef USBD_IsoINIncomplete(USBD_HandleTypeDef* pdev, uint8_t epnum)
{
    return USBD_OK;
}

/**
 * @brief  ISO OUT incomplete.
 */
USBD_StatusTypeDef USBD_IsoOUTIncomplete(USBD_HandleTypeDef* pdev, uint8_t epnum)
{
    return USBD_OK;
}

/* ------------------------------------------------------------------ */
/*  Standard request helpers (device, interface, endpoint)            */
/* ------------------------------------------------------------------ */

void USBD_ParseSetupRequest(USBD_SetupReqTypedef* req, uint8_t* pdata)
{
    req->bmRequest = pdata[0];
    req->bRequest = pdata[1];
    req->wValue = (uint16_t)(pdata[2] | (pdata[3] << 8));
    req->wIndex = (uint16_t)(pdata[4] | (pdata[5] << 8));
    req->wLength = (uint16_t)(pdata[6] | (pdata[7] << 8));
}

void USBD_CtlError(USBD_HandleTypeDef* pdev)
{
    USBD_LL_StallEP(pdev, 0x80);
    USBD_LL_StallEP(pdev, 0x00);
}

uint8_t USBD_GetString(uint8_t* desc, uint8_t* unicode, uint16_t* len)
{
    if (desc == NULL)
        return 0;

    *len = (uint16_t)USBD_MAX_STR_DESC_SIZ;

    unicode[0] = (uint16_t)(*len & 0xFF);
    unicode[1] = (uint16_t)USB_DESC_TYPE_STRING;

    *len = 2U;

    while (*desc != 0U)
    {
        unicode[*len] = *desc;
        desc++;
        unicode[*len + 1] = 0U;
        *len += 2U;
    }

    return 1;
}
