/**
 * @file    usbd_ctlreq.h
 * @brief   USB Device control request header.
 */

#ifndef USBD_CTLREQ_H
#define USBD_CTLREQ_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "usbd_def.h"

    void USBD_StdDevReq(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req);
    void USBD_StdItfReq(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req);
    void USBD_StdEPReq(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req);

    void USBD_ParseSetupRequest(USBD_SetupReqTypedef* req, uint8_t* pdata);
    void USBD_CtlError(USBD_HandleTypeDef* pdev);
    uint8_t USBD_GetString(uint8_t* desc, uint8_t* unicode, uint16_t* len);

    extern uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];

#ifdef __cplusplus
}
#endif

#endif /* USBD_CTLREQ_H */
