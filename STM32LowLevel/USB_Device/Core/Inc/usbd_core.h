/**
 * @file    usbd_core.h
 * @brief   USB Device Core header (simplified, CDC-only).
 */

#ifndef USBD_CORE_H
#define USBD_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"
#include "usbd_conf.h"

USBD_StatusTypeDef USBD_Init(USBD_HandleTypeDef *pdev, USBD_DescriptorsTypeDef *pdesc,
                             uint8_t id);
USBD_StatusTypeDef USBD_DeInit(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_Start(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_Stop(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_RegisterClass(USBD_HandleTypeDef *pdev, USBD_ClassTypeDef *pclass);
USBD_StatusTypeDef USBD_RunTestMode(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_SetupStage(USBD_HandleTypeDef *pdev, uint8_t *psetup);
USBD_StatusTypeDef USBD_DataOutStage(USBD_HandleTypeDef *pdev, uint8_t epnum, uint8_t *pdata);
USBD_StatusTypeDef USBD_DataInStage(USBD_HandleTypeDef *pdev, uint8_t epnum, uint8_t *pdata);
USBD_StatusTypeDef USBD_Reset(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_SetSpeed(USBD_HandleTypeDef *pdev, USBD_SpeedTypeDef speed);
USBD_StatusTypeDef USBD_Suspend(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_Resume(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_SOF(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_DevConnected(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_DevDisconnected(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_IsoINIncomplete(USBD_HandleTypeDef *pdev, uint8_t epnum);
USBD_StatusTypeDef USBD_IsoOUTIncomplete(USBD_HandleTypeDef *pdev, uint8_t epnum);
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t ep_type,
                                   uint16_t ep_mps);
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr);
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr);
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr);
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr);
USBD_StatusTypeDef USBD_SetDevAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr);
USBD_StatusTypeDef USBD_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf,
                                 uint32_t len);
USBD_StatusTypeDef USBD_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf,
                                       uint32_t len);
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr);
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr);
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr);
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr, uint8_t *pbuf,
                                    uint32_t len);
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                          uint8_t *pbuf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* USBD_CORE_H */
