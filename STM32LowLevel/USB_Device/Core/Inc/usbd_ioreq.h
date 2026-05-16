/**
 * @file    usbd_ioreq.h
 * @brief   USB Device I/O request header.
 */

#ifndef USBD_IOREQ_H
#define USBD_IOREQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"
#include "usbd_core.h"

USBD_StatusTypeDef USBD_CtlSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len);
USBD_StatusTypeDef USBD_CtlContinueSendData(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len);
USBD_StatusTypeDef USBD_CtlPrepareRx(USBD_HandleTypeDef *pdev, uint8_t *pbuf, uint32_t len);
USBD_StatusTypeDef USBD_CtlReceiveStatus(USBD_HandleTypeDef *pdev);
USBD_StatusTypeDef USBD_CtlSendStatus(USBD_HandleTypeDef *pdev);
uint32_t USBD_GetRxCount(USBD_HandleTypeDef *pdev, uint8_t ep_addr);

#ifdef __cplusplus
}
#endif

#endif /* USBD_IOREQ_H */
