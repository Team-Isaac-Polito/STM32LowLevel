/**
 * @file    usbd_conf_template.h
 * @brief   USB Device configuration template.
 * @note    Copy this to usbd_conf.h and edit for your project.
 */

#ifndef USBD_CONF_H
#define USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* USB device configuration */
#define USBD_MAX_NUM_INTERFACES     1U
#define USBD_MAX_NUM_CONFIGURATION  1U
#define USBD_MAX_STR_DESC_SIZ       0x100U
#define USBD_SELF_POWERED           1U
#define USBD_DEBUG_LEVEL            0U

/* Memory alignment for USB DMA */
#define USBD_MEM_ALIGNED_BUF(addr)  ((uint32_t)(addr) & 0x3U)

/* CDC class TX/RX buffer sizes */
#define CDC_DATA_FS_MAX_PACKET_SIZE 64U
#define CDC_CMD_PACKET_SIZE         8U
#define CDC_DATA_FS_IN_PACKET_SIZE  CDC_DATA_FS_MAX_PACKET_SIZE
#define CDC_DATA_FS_OUT_PACKET_SIZE CDC_DATA_FS_MAX_PACKET_SIZE

#ifdef __cplusplus
}
#endif

#endif /* USBD_CONF_H */
