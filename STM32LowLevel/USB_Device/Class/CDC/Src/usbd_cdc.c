/**
 * @file    usbd_cdc.c
 * @brief   USB CDC class implementation.
 */

#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"

static uint8_t USBD_CDC_Init(USBD_HandleTypeDef* pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_DeInit(USBD_HandleTypeDef* pdev, uint8_t cfgidx);
static uint8_t USBD_CDC_Setup(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req);
static uint8_t USBD_CDC_DataIn(USBD_HandleTypeDef* pdev, uint8_t epnum);
static uint8_t USBD_CDC_DataOut(USBD_HandleTypeDef* pdev, uint8_t epnum);
static uint8_t USBD_CDC_EP0_RxReady(USBD_HandleTypeDef* pdev);
static uint8_t* USBD_CDC_GetFSCfgDesc(uint16_t* length);
static uint8_t* USBD_CDC_GetHSCfgDesc(uint16_t* length);
static uint8_t* USBD_CDC_GetOtherSpeedCfgDesc(uint16_t* length);
static uint8_t* USBD_CDC_GetDeviceQualifierDesc(uint16_t* length);

static USBD_CDC_HandleTypeDef CDC_Handle;

/* CDC class callbacks */
USBD_ClassTypeDef USBD_CDC = {
    USBD_CDC_Init,
    USBD_CDC_DeInit,
    USBD_CDC_Setup,
    NULL, /* EP0_TxSent */
    USBD_CDC_EP0_RxReady,
    USBD_CDC_DataIn,
    USBD_CDC_DataOut,
    NULL, /* SOF */
    NULL, /* IsoINIncomplete */
    NULL, /* IsoOUTIncomplete */
    USBD_CDC_GetHSCfgDesc,
    USBD_CDC_GetFSCfgDesc,
    USBD_CDC_GetOtherSpeedCfgDesc,
    USBD_CDC_GetDeviceQualifierDesc,
};

/* USB CDC device configuration descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_CfgHSDesc[USB_CDC_CONFIG_DESC_SIZ] __ALIGN_END = {
    /* Configuration Descriptor */
    0x09,                            /* bLength */
    USB_DESC_TYPE_CONFIGURATION,     /* bDescriptorType */
    LOBYTE(USB_CDC_CONFIG_DESC_SIZ), /* wTotalLength */
    HIBYTE(USB_CDC_CONFIG_DESC_SIZ),
    0x02,                   /* bNumInterfaces */
    0x01,                   /* bConfigurationValue */
    0x00,                   /* iConfiguration */
    USB_CONFIG_BUS_POWERED, /* bmAttributes */
    0x32,                   /* bMaxPower (100 mA) */

    /* Interface Association Descriptor (CDC) */
    0x08, /* bLength */
    0x0B, /* bDescriptorType (IAD) */
    0x00, /* bFirstInterface */
    0x02, /* bInterfaceCount */
    0x02, /* bFunctionClass (CDC) */
    0x02, /* bFunctionSubClass (ACM) */
    0x01, /* bFunctionProtocol (AT) */
    0x00, /* iFunction */

    /* Communication Interface Descriptor */
    0x09,                    /* bLength */
    USB_DESC_TYPE_INTERFACE, /* bDescriptorType */
    0x00,                    /* bInterfaceNumber */
    0x00,                    /* bAlternateSetting */
    0x01,                    /* bNumEndpoints */
    0x02,                    /* bInterfaceClass (CDC) */
    0x02,                    /* bInterfaceSubClass (ACM) */
    0x01,                    /* bInterfaceProtocol (AT) */
    0x00,                    /* iInterface */

    /* Header Functional Descriptor */
    0x05, /* bLength */
    0x24, /* bDescriptorType (CS_INTERFACE) */
    0x00, /* bDescriptorSubtype (Header) */
    0x10,
    0x01, /* bcdCDC (1.10) */

    /* Call Management Functional Descriptor */
    0x05, /* bLength */
    0x24, /* bDescriptorType (CS_INTERFACE) */
    0x01, /* bDescriptorSubtype (Call Management) */
    0x00, /* bmCapabilities */
    0x01, /* bDataInterface */

    /* ACM Functional Descriptor */
    0x04, /* bLength */
    0x24, /* bDescriptorType (CS_INTERFACE) */
    0x02, /* bDescriptorSubtype (ACM) */
    0x02, /* bmCapabilities */

    /* Union Functional Descriptor */
    0x05, /* bLength */
    0x24, /* bDescriptorType (CS_INTERFACE) */
    0x06, /* bDescriptorSubtype (Union) */
    0x00, /* bMasterInterface */
    0x01, /* bSlaveInterface0 */

    /* Command Endpoint Descriptor (EP2 IN) */
    0x07,                        /* bLength */
    USB_DESC_TYPE_ENDPOINT,      /* bDescriptorType */
    CDC_CMD_EP,                  /* bEndpointAddress */
    USB_EP_TYPE_INTR,            /* bmAttributes */
    LOBYTE(CDC_CMD_PACKET_SIZE), /* wMaxPacketSize */
    HIBYTE(CDC_CMD_PACKET_SIZE),
    0x0A, /* bInterval (10 ms) */

    /* Data Interface Descriptor */
    0x09,                    /* bLength */
    USB_DESC_TYPE_INTERFACE, /* bDescriptorType */
    0x01,                    /* bInterfaceNumber */
    0x00,                    /* bAlternateSetting */
    0x02,                    /* bNumEndpoints */
    0x0A,                    /* bInterfaceClass (CDC Data) */
    0x00,                    /* bInterfaceSubClass */
    0x00,                    /* bInterfaceProtocol */
    0x00,                    /* iInterface */

    /* Data OUT Endpoint (EP1 OUT) */
    0x07,                                /* bLength */
    USB_DESC_TYPE_ENDPOINT,              /* bDescriptorType */
    CDC_OUT_EP,                          /* bEndpointAddress */
    USB_EP_TYPE_BULK,                    /* bmAttributes */
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE), /* wMaxPacketSize */
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00, /* bInterval */

    /* Data IN Endpoint (EP1 IN) */
    0x07,                                /* bLength */
    USB_DESC_TYPE_ENDPOINT,              /* bDescriptorType */
    CDC_IN_EP,                           /* bEndpointAddress */
    USB_EP_TYPE_BULK,                    /* bmAttributes */
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE), /* wMaxPacketSize */
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00, /* bInterval */
};

/* USB CDC device configuration descriptor (FS) */
__ALIGN_BEGIN static uint8_t USBD_CDC_CfgFSDesc[USB_CDC_CONFIG_DESC_SIZ] __ALIGN_END = {
    /* Same as HS for FS */
    /* Configuration Descriptor */
    0x09,
    USB_DESC_TYPE_CONFIGURATION,
    LOBYTE(USB_CDC_CONFIG_DESC_SIZ),
    HIBYTE(USB_CDC_CONFIG_DESC_SIZ),
    0x02,
    0x01,
    0x00,
    USB_CONFIG_BUS_POWERED,
    0x32,

    /* IAD */
    0x08,
    0x0B,
    0x00,
    0x02,
    0x02,
    0x02,
    0x01,
    0x00,

    /* Comm Interface */
    0x09,
    USB_DESC_TYPE_INTERFACE,
    0x00,
    0x00,
    0x01,
    0x02,
    0x02,
    0x01,
    0x00,

    /* Header FD */
    0x05,
    0x24,
    0x00,
    0x10,
    0x01,

    /* Call Management FD */
    0x05,
    0x24,
    0x01,
    0x00,
    0x01,

    /* ACM FD */
    0x04,
    0x24,
    0x02,
    0x02,

    /* Union FD */
    0x05,
    0x24,
    0x06,
    0x00,
    0x01,

    /* CMD EP */
    0x07,
    USB_DESC_TYPE_ENDPOINT,
    CDC_CMD_EP,
    USB_EP_TYPE_INTR,
    LOBYTE(CDC_CMD_PACKET_SIZE),
    HIBYTE(CDC_CMD_PACKET_SIZE),
    0x0A,

    /* Data Interface */
    0x09,
    USB_DESC_TYPE_INTERFACE,
    0x01,
    0x00,
    0x02,
    0x0A,
    0x00,
    0x00,
    0x00,

    /* Data OUT EP */
    0x07,
    USB_DESC_TYPE_ENDPOINT,
    CDC_OUT_EP,
    USB_EP_TYPE_BULK,
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,

    /* Data IN EP */
    0x07,
    USB_DESC_TYPE_ENDPOINT,
    CDC_IN_EP,
    USB_EP_TYPE_BULK,
    LOBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    HIBYTE(CDC_DATA_FS_MAX_PACKET_SIZE),
    0x00,
};

/* USB device qualifier descriptor */
__ALIGN_BEGIN static uint8_t USBD_CDC_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x02,
    0x02,
    0x00,
    0x40,
    0x01,
    0x00,
};

/**
 * @brief  Initializes the CDC class.
 */
static uint8_t USBD_CDC_Init(USBD_HandleTypeDef* pdev, uint8_t cfgidx)
{
    uint8_t ret = 0U;

    (void)cfgidx;
    pdev->pClassData = &CDC_Handle;
    CDC_Handle.TxState = 0U;
    CDC_Handle.RxState = 0U;
    CDC_Handle.dataTxLength = 0U;
    CDC_Handle.dataRxLength = 0U;

    if (pdev->dev_speed == USBD_SPEED_HIGH)
    {
        USBD_LL_OpenEP(pdev, CDC_IN_EP, USB_EP_TYPE_BULK, CDC_DATA_HS_MAX_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, CDC_OUT_EP, USB_EP_TYPE_BULK, CDC_DATA_HS_MAX_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, CDC_CMD_EP, USB_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);
    }
    else
    {
        USBD_LL_OpenEP(pdev, CDC_IN_EP, USB_EP_TYPE_BULK, CDC_DATA_FS_IN_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, CDC_OUT_EP, USB_EP_TYPE_BULK, CDC_DATA_FS_OUT_PACKET_SIZE);
        USBD_LL_OpenEP(pdev, CDC_CMD_EP, USB_EP_TYPE_INTR, CDC_CMD_PACKET_SIZE);
    }

    /* Call user init callback */
    USBD_CDC_ItfTypeDef* fops = (USBD_CDC_ItfTypeDef*)pdev->pUserData;
    if ((fops != NULL) && (fops->Init != NULL))
        fops->Init();

    if (CDC_Handle.RxBuffer != NULL)
        USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, CDC_Handle.RxBuffer, CDC_DATA_FS_OUT_PACKET_SIZE);

    return ret;
}

/**
 * @brief  De-initializes the CDC class.
 */
static uint8_t USBD_CDC_DeInit(USBD_HandleTypeDef* pdev, uint8_t cfgidx)
{
    uint8_t ret = 0U;

    (void)cfgidx;

    USBD_LL_CloseEP(pdev, CDC_IN_EP);
    USBD_LL_CloseEP(pdev, CDC_OUT_EP);
    USBD_LL_CloseEP(pdev, CDC_CMD_EP);

    /* Call user deinit callback */
    USBD_CDC_ItfTypeDef* fops = (USBD_CDC_ItfTypeDef*)pdev->pUserData;
    if ((fops != NULL) && (fops->DeInit != NULL))
        fops->DeInit();

    pdev->pClassData = NULL;

    return ret;
}

/**
 * @brief  Handles CDC-specific setup requests.
 */
static uint8_t USBD_CDC_Setup(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req)
{
    USBD_CDC_ItfTypeDef* fops = (USBD_CDC_ItfTypeDef*)pdev->pUserData;
    uint8_t ifalt = 0U;
    uint16_t status_info = 0U;
    uint8_t ret = USBD_OK;

    switch (req->bmRequest & USB_REQ_TYPE_MASK)
    {
        case USB_REQ_TYPE_CLASS:
            if (req->wLength != 0U)
            {
                if ((req->bmRequest & 0x80U) != 0U)
                {
                    if (fops != NULL)
                        fops->Control(req->bRequest, (uint8_t*)CDC_Handle.cmd, req->wLength);

                    USBD_CtlSendData(pdev, (uint8_t*)CDC_Handle.cmd, req->wLength);
                }
                else
                {
                    CDC_Handle.dataRxLength = req->wLength;
                    USBD_CtlPrepareRx(pdev, (uint8_t*)CDC_Handle.cmd, req->wLength);
                }
            }
            else
            {
                if (fops != NULL)
                    fops->Control(req->bRequest, (uint8_t*)&req->wValue, 0U);
            }
            break;

        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest)
            {
                case USB_REQ_GET_STATUS:
                    if (pdev->dev_state == USBD_CONFIGURED)
                    {
                        USBD_CtlSendData(pdev, (uint8_t*)&status_info, 2U);
                    }
                    else
                    {
                        USBD_CtlError(pdev);
                        ret = USBD_FAIL;
                    }
                    break;

                case USB_REQ_GET_INTERFACE:
                    if (pdev->dev_state == USBD_CONFIGURED)
                    {
                        USBD_CtlSendData(pdev, &ifalt, 1U);
                    }
                    else
                    {
                        USBD_CtlError(pdev);
                        ret = USBD_FAIL;
                    }
                    break;

                case USB_REQ_SET_INTERFACE:
                    if (pdev->dev_state != USBD_CONFIGURED)
                    {
                        USBD_CtlError(pdev);
                        ret = USBD_FAIL;
                    }
                    break;

                default:
                    USBD_CtlError(pdev);
                    ret = USBD_FAIL;
                    break;
            }
            break;

        default:
            USBD_CtlError(pdev);
            ret = USBD_FAIL;
            break;
    }

    return ret;
}

/**
 * @brief  Handles data IN completion.
 */
static uint8_t USBD_CDC_DataIn(USBD_HandleTypeDef* pdev, uint8_t epnum)
{
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)pdev->pClassData;

    if (hcdc == NULL)
        return USBD_FAIL;

    if ((epnum | 0x80U) == CDC_IN_EP)
    {
        hcdc->TxState = 0U;

        USBD_CDC_ItfTypeDef* fops = (USBD_CDC_ItfTypeDef*)pdev->pUserData;
        if ((fops != NULL) && (fops->TransmitCplt != NULL))
            fops->TransmitCplt(hcdc->TxBuffer, &hcdc->dataTxLength, epnum);
    }

    return USBD_OK;
}

/**
 * @brief  Handles data OUT.
 */
static uint8_t USBD_CDC_DataOut(USBD_HandleTypeDef* pdev, uint8_t epnum)
{
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)pdev->pClassData;

    if (hcdc == NULL)
        return USBD_FAIL;

    if (epnum == (CDC_OUT_EP & 0x7FU))
    {
        uint32_t rxLen = USBD_LL_GetRxDataSize(pdev, epnum);

        USBD_CDC_ItfTypeDef* fops = (USBD_CDC_ItfTypeDef*)pdev->pUserData;
        if ((fops != NULL) && (fops->Receive != NULL))
            fops->Receive(hcdc->RxBuffer, &rxLen);
    }

    return USBD_OK;
}

/**
 * @brief  Handles EP0 Rx ready.
 */
static uint8_t USBD_CDC_EP0_RxReady(USBD_HandleTypeDef* pdev)
{
    USBD_CDC_ItfTypeDef* fops = (USBD_CDC_ItfTypeDef*)pdev->pUserData;
    if ((fops != NULL) && (fops->Control != NULL))
        fops->Control(CDC_SEND_ENCAPSULATED_COMMAND, (uint8_t*)CDC_Handle.cmd, CDC_Handle.dataRxLength);

    return USBD_OK;
}

/**
 * @brief  Returns the FS configuration descriptor.
 */
static uint8_t* USBD_CDC_GetFSCfgDesc(uint16_t* length)
{
    USBD_CDC_CfgFSDesc[1] = USB_DESC_TYPE_CONFIGURATION;
    *length = sizeof(USBD_CDC_CfgFSDesc);
    return USBD_CDC_CfgFSDesc;
}

/**
 * @brief  Returns the HS configuration descriptor.
 */
static uint8_t* USBD_CDC_GetHSCfgDesc(uint16_t* length)
{
    *length = sizeof(USBD_CDC_CfgHSDesc);
    return USBD_CDC_CfgHSDesc;
}

/**
 * @brief  Returns the other speed configuration descriptor.
 */
static uint8_t* USBD_CDC_GetOtherSpeedCfgDesc(uint16_t* length)
{
    USBD_CDC_CfgFSDesc[1] = USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION;
    *length = sizeof(USBD_CDC_CfgFSDesc);
    return USBD_CDC_CfgFSDesc;
}

/**
 * @brief  Returns the device qualifier descriptor.
 */
static uint8_t* USBD_CDC_GetDeviceQualifierDesc(uint16_t* length)
{
    *length = sizeof(USBD_CDC_DeviceQualifierDesc);
    return USBD_CDC_DeviceQualifierDesc;
}

/**
 * @brief  Sets the TX buffer.
 */
uint8_t USBD_CDC_SetTxBuffer(USBD_HandleTypeDef* pdev, uint8_t* pbuff, uint32_t length)
{
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)pdev->pClassData;

    if ((hcdc == NULL) || (pbuff == NULL))
        return USBD_FAIL;

    hcdc->TxBuffer = pbuff;
    hcdc->dataTxLength = length;
    return USBD_OK;
}

/**
 * @brief  Sets the RX buffer.
 */
uint8_t USBD_CDC_SetRxBuffer(USBD_HandleTypeDef* pdev, uint8_t* pbuff)
{
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)pdev->pClassData;

    if ((hcdc == NULL) || (pbuff == NULL))
        return USBD_FAIL;

    hcdc->RxBuffer = pbuff;
    return USBD_OK;
}

/**
 * @brief  Transmits a packet.
 */
uint8_t USBD_CDC_TransmitPacket(USBD_HandleTypeDef* pdev)
{
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)pdev->pClassData;

    if (hcdc == NULL)
        return USBD_FAIL;

    if (hcdc->TxState != 0U)
        return USBD_BUSY;

    hcdc->TxState = 1U;

    USBD_LL_Transmit(pdev, CDC_IN_EP, hcdc->TxBuffer, hcdc->dataTxLength);

    return USBD_OK;
}

/**
 * @brief  Registers the CDC interface operations.
 */
void USBD_CDC_RegisterInterface(USBD_HandleTypeDef* pdev, USBD_CDC_ItfTypeDef* fops)
{
    if (fops != NULL)
        pdev->pUserData = fops;
}

/**
 * @brief  Prepares to receive a packet.
 */
uint8_t USBD_CDC_ReceivePacket(USBD_HandleTypeDef* pdev)
{
    USBD_CDC_HandleTypeDef* hcdc = (USBD_CDC_HandleTypeDef*)pdev->pClassData;

    if ((hcdc == NULL) || (hcdc->RxBuffer == NULL))
        return USBD_FAIL;

    USBD_LL_PrepareReceive(pdev, CDC_OUT_EP, hcdc->RxBuffer, CDC_DATA_FS_OUT_PACKET_SIZE);

    return USBD_OK;
}
