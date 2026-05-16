/**
 * @file    usbd_desc.h
 * @brief   USB Device descriptor header.
 */

#ifndef USBD_DESC_H
#define USBD_DESC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_def.h"

#define USB_CDC_CONFIG_DESC_SIZ       75U
#define USB_LEN_DEV_QUALIFIER_DESC    10U

extern USBD_DescriptorsTypeDef CDC_Desc;

#ifdef __cplusplus
}
#endif

#endif /* USBD_DESC_H */
