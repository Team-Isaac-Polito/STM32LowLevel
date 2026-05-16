/**
 * @file    usbd_conf.h
 * @brief   USB Device configuration for STM32G474 (CDC ACM VCP).
 */

#ifndef USBD_CONF_H
#define USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include "usbd_def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USB device configuration */
#define USBD_MAX_NUM_INTERFACES     2U
#define USBD_MAX_NUM_CONFIGURATION  1U
#define USBD_MAX_STR_DESC_SIZ       0x100U
#define USBD_SELF_POWERED           1U
#define USBD_DEBUG_LEVEL            0U

/* CDC class buffer sizes */
#define CDC_DATA_FS_MAX_PACKET_SIZE 64U
#define CDC_CMD_PACKET_SIZE         8U
#define CDC_DATA_FS_IN_PACKET_SIZE  CDC_DATA_FS_MAX_PACKET_SIZE
#define CDC_DATA_FS_OUT_PACKET_SIZE CDC_DATA_FS_MAX_PACKET_SIZE

/* USB LL callouts — implemented in usbd_conf.c */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd);
void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd);
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd);
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum);
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum);
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd);
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd);
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd);
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd);
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd);
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd);
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum);
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum);

void MX_USB_Device_Init(void);
extern USBD_HandleTypeDef hUsbDeviceFS;

#ifdef __cplusplus
}
#endif

#endif /* USBD_CONF_H */
