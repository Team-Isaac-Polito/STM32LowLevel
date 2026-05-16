/**
 * @file    usbd_conf.c
 * @brief   USB Device low-level configuration and PCD callbacks.
 */

#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"

/* hpcd_USB_FS is defined in Core/Src/usb.c (CubeMX-generated) */
extern PCD_HandleTypeDef hpcd_USB_FS;
USBD_HandleTypeDef hUsbDeviceFS;

/**
 * @brief  Setup stage callback.
 */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_SetupStage((USBD_HandleTypeDef *)hpcd->pData, (uint8_t *)hpcd->Setup);
}

/**
 * @brief  Data Out stage callback.
 */
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_DataOutStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
 * @brief  Data In stage callback.
 */
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
 * @brief  SOF callback.
 */
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief  Reset callback.
 */
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

    /* Set USB current speed */
    switch (hpcd->Init.speed)
    {
    case PCD_SPEED_FULL:
        speed = USBD_SPEED_FULL;
        break;
    default:
        speed = USBD_SPEED_FULL;
        break;
    }

    USBD_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, speed);
    USBD_Reset((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief  Suspend callback.
 */
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_Suspend((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief  Resume callback.
 */
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief  Connect callback.
 */
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief  Disconnect callback.
 */
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief  ISO IN incomplete callback.
 */
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_IsoINIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

/**
 * @brief  ISO OUT incomplete callback.
 */
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_IsoOUTIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

/**
 * @brief  Initializes the USB device low-level part.
 * @param  pdev: Device handle
 * @retval USBD status
 */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    /* Link the driver to the stack */
    hpcd_USB_FS.pData = pdev;
    pdev->pData = &hpcd_USB_FS;

    hpcd_USB_FS.Instance = USB;
    hpcd_USB_FS.Init.dev_endpoints = 8;
    hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_FS.Init.Sof_enable = DISABLE;
    hpcd_USB_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_FS.Init.lpm_enable = DISABLE;
    hpcd_USB_FS.Init.battery_charging_enable = DISABLE;

    if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
    {
        return USBD_FAIL;
    }

    /* Configure endpoints for CDC: 1 CMD (INTR) + 1 BULK IN + 1 BULK OUT */
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x00, PCD_SNG_BUF, 0x18);  /* EP0 OUT */
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x80, PCD_SNG_BUF, 0x58);  /* EP0 IN  */
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_CMD_EP, PCD_SNG_BUF, 0x98);   /* CMD IN  */
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_IN_EP, PCD_SNG_BUF, 0xD8);   /* DATA IN */
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_OUT_EP, PCD_SNG_BUF, 0x118); /* DATA OUT*/

    return USBD_OK;
}

/**
 * @brief  De-initializes the USB device low-level part.
 */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_DeInit((PCD_HandleTypeDef *)pdev->pData);
    return USBD_OK;
}

/**
 * @brief  Starts the USB device.
 */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_Start((PCD_HandleTypeDef *)pdev->pData);
    return USBD_OK;
}

/**
 * @brief  Stops the USB device.
 */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
    HAL_PCD_Stop((PCD_HandleTypeDef *)pdev->pData);
    return USBD_OK;
}

/**
 * @brief  Opens an endpoint.
 */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type,
                                  uint16_t ep_mps)
{
    HAL_PCD_EP_Open((PCD_HandleTypeDef *)pdev->pData, ep_addr, ep_mps, ep_type);
    return USBD_OK;
}

/**
 * @brief  Closes an endpoint.
 */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_Close((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Flushes an endpoint.
 */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_Flush((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Sets a stall condition on an endpoint.
 */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_SetStall((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Clears a stall condition on an endpoint.
 */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_PCD_EP_ClrStall((PCD_HandleTypeDef *)pdev->pData, ep_addr);
    return USBD_OK;
}

/**
 * @brief  Returns stall condition.
 */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;
    if ((ep_addr & 0x80U) == 0x80U)
        return hpcd->IN_ep[ep_addr & 0x7FU].is_stall;
    else
        return hpcd->OUT_ep[ep_addr & 0x7FU].is_stall;
}

/**
 * @brief  Assigns a USB address to the device.
 */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
    HAL_PCD_SetAddress((PCD_HandleTypeDef *)pdev->pData, dev_addr);
    return USBD_OK;
}

/**
 * @brief  Transmits data over an endpoint.
 */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf,
                                    uint32_t len)
{
    HAL_PCD_EP_Transmit((PCD_HandleTypeDef *)pdev->pData, ep_addr, pbuf, len);
    return USBD_OK;
}

/**
 * @brief  Prepares an endpoint for reception.
 */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf,
                                          uint32_t len)
{
    HAL_PCD_EP_Receive((PCD_HandleTypeDef *)pdev->pData, ep_addr, pbuf, len);
    return USBD_OK;
}

/**
 * @brief  Returns the last transferred packet size.
 */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

/**
 * @brief  Delays for the given time (ms).
 */
void USBD_Delay(uint32_t Delay)
{
    HAL_Delay(Delay);
}

/**
 * @brief  Initializes the USB device (called from main).
 */
void MX_USB_Device_Init(void)
{
    USBD_Init(&hUsbDeviceFS, &CDC_Desc, 0U);
    USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC);
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_CDC_fops);
    USBD_Start(&hUsbDeviceFS);
}
