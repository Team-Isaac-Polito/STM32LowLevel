/**
 * @file    usbd_ctlreq.c
 * @brief   USB Device control request handler.
 */

#include "usbd_ctlreq.h"
#include "usbd_ioreq.h"

uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];

static void USBD_GetDescriptor(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static void USBD_SetAddress(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static void USBD_SetConfig(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static void USBD_GetConfig(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static void USBD_GetStatus(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static void USBD_SetFeature(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static void USBD_ClrFeature(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);

/**
 * @brief  Handles standard device requests.
 */
void USBD_StdDevReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    switch (req->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR:
        USBD_GetDescriptor(pdev, req);
        break;

    case USB_REQ_SET_ADDRESS:
        USBD_SetAddress(pdev, req);
        break;

    case USB_REQ_SET_CONFIGURATION:
        USBD_SetConfig(pdev, req);
        break;

    case USB_REQ_GET_CONFIGURATION:
        USBD_GetConfig(pdev, req);
        break;

    case USB_REQ_GET_STATUS:
        USBD_GetStatus(pdev, req);
        break;

    case USB_REQ_SET_FEATURE:
        USBD_SetFeature(pdev, req);
        break;

    case USB_REQ_CLEAR_FEATURE:
        USBD_ClrFeature(pdev, req);
        break;

    default:
        USBD_CtlError(pdev);
        break;
    }
}

/**
 * @brief  Handles standard interface requests.
 */
void USBD_StdItfReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    if (pdev->dev_state == USBD_CONFIGURED)
    {
        if (LOBYTE(req->wIndex) < USBD_MAX_NUM_INTERFACES)
        {
            if (pdev->pClass != NULL)
            {
                pdev->pClass->Setup(pdev, req);
            }

            if (req->wLength == 0U)
            {
                USBD_CtlSendStatus(pdev);
            }
        }
        else
        {
            USBD_CtlError(pdev);
        }
    }
    else
    {
        USBD_CtlError(pdev);
    }
}

/**
 * @brief  Handles standard endpoint requests.
 */
void USBD_StdEPReq(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    uint8_t ep_addr = LOBYTE(req->wIndex);

    if ((ep_addr & 0x80U) == 0x80U)
    {
        /* IN endpoint */
    }
    else
    {
        /* OUT endpoint */
    }

    switch (req->bRequest)
    {
    case USB_REQ_GET_STATUS:
    {
        uint16_t status = 0U;
        if (USBD_LL_IsStallEP(pdev, ep_addr) != 0U)
        {
            status = 1U;
        }
        USBD_CtlSendData(pdev, (uint8_t *)&status, 2U);
        break;
    }

    case USB_REQ_CLEAR_FEATURE:
        if (req->wValue == USB_FEATURE_ENDPOINT_STALL)
        {
            USBD_LL_ClearStallEP(pdev, ep_addr);
            if (pdev->pClass != NULL)
            {
                pdev->pClass->Setup(pdev, req);
            }
            USBD_CtlSendStatus(pdev);
        }
        break;

    case USB_REQ_SET_FEATURE:
        if (req->wValue == USB_FEATURE_ENDPOINT_STALL)
        {
            USBD_LL_StallEP(pdev, ep_addr);
        }
        break;

    default:
        break;
    }
}

/**
 * @brief  Handles GET_DESCRIPTOR request.
 */
static void USBD_GetDescriptor(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    uint16_t len = 0U;
    uint8_t *pbuf = NULL;

    switch (HIBYTE(req->wValue))
    {
    case USB_DESC_TYPE_DEVICE:
        pbuf = pdev->pDesc->GetDeviceDescriptor(&len);
        break;

    case USB_DESC_TYPE_CONFIGURATION:
        pbuf = pdev->pClass->GetFSConfigDescriptor(&len);
        break;

    case USB_DESC_TYPE_DEVICE_QUALIFIER:
        pbuf = pdev->pClass->GetDeviceQualifierDescriptor(&len);
        break;

    case USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION:
        pbuf = pdev->pClass->GetOtherSpeedConfigDescriptor(&len);
        break;

    case USB_DESC_TYPE_STRING:
    {
        uint8_t idx = LOBYTE(req->wValue);
        switch (idx)
        {
        case 0x00:
            pbuf = pdev->pDesc->GetLangIDStrDescriptor(&len);
            break;
        case 0x01:
            pbuf = pdev->pDesc->GetManufacturerStrDescriptor(&len);
            break;
        case 0x02:
            pbuf = pdev->pDesc->GetProductStrDescriptor(&len);
            break;
        case 0x03:
            pbuf = pdev->pDesc->GetSerialStrDescriptor(&len);
            break;
        case 0x04:
            pbuf = pdev->pDesc->GetConfigurationStrDescriptor(&len);
            break;
        case 0x05:
            pbuf = pdev->pDesc->GetInterfaceStrDescriptor(&len);
            break;
        default:
            USBD_CtlError(pdev);
            return;
        }
        break;
    }

    default:
        USBD_CtlError(pdev);
        return;
    }

    if ((len != 0U) && (req->wLength != 0U))
    {
        len = MIN(len, req->wLength);
        USBD_CtlSendData(pdev, pbuf, len);
    }
}

/**
 * @brief  Handles SET_ADDRESS request.
 */
static void USBD_SetAddress(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    if ((req->wIndex == 0U) && (req->wLength == 0U) && (req->wValue < 128U))
    {
        pdev->dev_address = req->wValue;
        USBD_LL_SetUSBAddress(pdev, req->wValue);
        USBD_CtlSendStatus(pdev);

        if (req->wValue != 0U)
        {
            pdev->dev_state = USBD_ADDRESSED;
        }
        else
        {
            pdev->dev_state = USBD_DEFAULT;
        }
    }
    else
    {
        USBD_CtlError(pdev);
    }
}

/**
 * @brief  Handles SET_CONFIGURATION request.
 */
static void USBD_SetConfig(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    uint8_t cfgidx = LOBYTE(req->wValue);

    if (cfgidx > USBD_MAX_NUM_CONFIGURATION)
    {
        USBD_CtlError(pdev);
    }
    else
    {
        pdev->dev_config = cfgidx;

        if (cfgidx == 0U)
        {
            pdev->dev_state = USBD_ADDRESSED;

            if (pdev->pClass != NULL)
            {
                pdev->pClass->DeInit(pdev, cfgidx);
            }
        }
        else
        {
            pdev->dev_state = USBD_CONFIGURED;

            if (pdev->pClass != NULL)
            {
                if (pdev->pClass->Init(pdev, cfgidx) != 0U)
                {
                    USBD_CtlError(pdev);
                    return;
                }
            }
        }

        USBD_CtlSendStatus(pdev);
    }
}

/**
 * @brief  Handles GET_CONFIGURATION request.
 */
static void USBD_GetConfig(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    if (req->wLength != 1U)
    {
        USBD_CtlError(pdev);
    }
    else
    {
        USBD_CtlSendData(pdev, &pdev->dev_config, 1U);
    }
}

/**
 * @brief  Handles GET_STATUS request.
 */
static void USBD_GetStatus(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    if (req->wLength != 2U)
    {
        USBD_CtlError(pdev);
    }
    else
    {
        uint16_t status = 0U;
        USBD_CtlSendData(pdev, (uint8_t *)&status, 2U);
    }
}

/**
 * @brief  Handles SET_FEATURE request.
 */
static void USBD_SetFeature(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    if (req->wValue == USB_FEATURE_REMOTE_WAKEUP)
    {
        pdev->dev_remote_wakeup = 1U;
        if (pdev->pClass != NULL)
        {
            pdev->pClass->Setup(pdev, req);
        }
        USBD_CtlSendStatus(pdev);
    }
}

/**
 * @brief  Handles CLEAR_FEATURE request.
 */
static void USBD_ClrFeature(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
    switch (req->wValue)
    {
    case USB_FEATURE_REMOTE_WAKEUP:
        pdev->dev_remote_wakeup = 0U;
        if (pdev->pClass != NULL)
        {
            pdev->pClass->Setup(pdev, req);
        }
        USBD_CtlSendStatus(pdev);
        break;

    default:
        USBD_CtlError(pdev);
        break;
    }
}

