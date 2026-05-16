/**
 * @file    usbd_def.h
 * @brief   USB Device common definitions.
 */

#ifndef USBD_DEF_H
#define USBD_DEF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef LOBYTE
#define LOBYTE(x) ((uint8_t)((x) & 0xFFU))
#endif

#ifndef HIBYTE
#define HIBYTE(x) ((uint8_t)(((x) >> 8) & 0xFFU))
#endif

/* USB device status */
typedef enum
{
    USBD_OK = 0U,
    USBD_BUSY,
    USBD_FAIL,
    USBD_TIMEOUT
} USBD_StatusTypeDef;

/* USB device speed */
typedef enum
{
    USBD_SPEED_FULL = 0U,
    USBD_SPEED_HIGH = 1U
} USBD_SpeedTypeDef;

/* USB endpoint direction */
#define USBD_EP_OUT 0x00U
#define USBD_EP_IN  0x80U

/* USB setup request */
typedef struct
{
    uint8_t bmRequest;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USBD_SetupReqTypedef;

/* USB descriptors */
typedef struct
{
    uint8_t *(*GetDeviceDescriptor)(uint16_t *length);
    uint8_t *(*GetLangIDStrDescriptor)(uint16_t *length);
    uint8_t *(*GetManufacturerStrDescriptor)(uint16_t *length);
    uint8_t *(*GetProductStrDescriptor)(uint16_t *length);
    uint8_t *(*GetSerialStrDescriptor)(uint16_t *length);
    uint8_t *(*GetConfigurationStrDescriptor)(uint16_t *length);
    uint8_t *(*GetInterfaceStrDescriptor)(uint16_t *length);
} USBD_DescriptorsTypeDef;

/* Forward declaration needed because USBD_ClassTypeDef uses USBD_HandleTypeDef* */
typedef struct _USBD_HandleTypeDef USBD_HandleTypeDef;

/* USB class operations */
typedef struct
{
    uint8_t (*Init)(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
    uint8_t (*DeInit)(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
    uint8_t (*Setup)(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
    uint8_t (*EP0_TxSent)(USBD_HandleTypeDef *pdev);
    uint8_t (*EP0_RxReady)(USBD_HandleTypeDef *pdev);
    uint8_t (*DataIn)(USBD_HandleTypeDef *pdev, uint8_t epnum);
    uint8_t (*DataOut)(USBD_HandleTypeDef *pdev, uint8_t epnum);
    uint8_t (*SOF)(USBD_HandleTypeDef *pdev);
    uint8_t (*IsoINIncomplete)(USBD_HandleTypeDef *pdev, uint8_t epnum);
    uint8_t (*IsoOUTIncomplete)(USBD_HandleTypeDef *pdev, uint8_t epnum);
    uint8_t *(*GetHSConfigDescriptor)(uint16_t *length);
    uint8_t *(*GetFSConfigDescriptor)(uint16_t *length);
    uint8_t *(*GetOtherSpeedConfigDescriptor)(uint16_t *length);
    uint8_t *(*GetDeviceQualifierDescriptor)(uint16_t *length);
} USBD_ClassTypeDef;

/* USB device handle */
typedef struct
{
    uint32_t status;
    uint32_t total_length;
    uint32_t rem_length;
    uint32_t maxpacket;
    uint16_t is_used;
    uint16_t bInterval;
} USBD_EndpointTypeDef;

struct _USBD_HandleTypeDef
{
    uint8_t dev_config;
    uint8_t dev_default_config;
    uint8_t dev_config_status;
    USBD_SpeedTypeDef dev_speed;
    USBD_EndpointTypeDef ep_in[16];
    USBD_EndpointTypeDef ep_out[16];
    uint32_t ep0_state;
    uint32_t ep0_data_len;
    uint8_t dev_state;
    uint8_t dev_old_state;
    uint8_t dev_address;
    uint8_t dev_connection_status;
    uint8_t dev_test_mode;
    uint32_t dev_remote_wakeup;
    USBD_SetupReqTypedef request;
    USBD_DescriptorsTypeDef *pDesc;
    USBD_ClassTypeDef *pClass;
    void *pClassData;
    void *pUserData;
    void *pData;  /* PCD handle (used by LL layer) */
    uint8_t id;
};

/* USB device states */
#define USBD_DEFAULT                          1U
#define USBD_ADDRESSED                        2U
#define USBD_CONFIGURED                       3U
#define USBD_SUSPENDED                        4U

/* EP0 states */
#define USBD_EP0_IDLE                         0U
#define USBD_EP0_SETUP                        1U
#define USBD_EP0_DATA_IN                      2U
#define USBD_EP0_DATA_OUT                     3U
#define USBD_EP0_STATUS_IN                    4U
#define USBD_EP0_STATUS_OUT                   5U
#define USBD_EP0_STALL                        6U

/* USB request types */
#define USB_REQ_TYPE_STANDARD                 0x00U
#define USB_REQ_TYPE_CLASS                    0x20U
#define USB_REQ_TYPE_VENDOR                   0x40U
#define USB_REQ_TYPE_MASK                     0x60U

#define USB_REQ_RECIPIENT_DEVICE              0x00U
#define USB_REQ_RECIPIENT_INTERFACE           0x01U
#define USB_REQ_RECIPIENT_ENDPOINT            0x02U
#define USB_REQ_RECIPIENT_MASK                0x1FU

/* Standard USB requests */
#define USB_REQ_GET_STATUS                    0x00U
#define USB_REQ_CLEAR_FEATURE                 0x01U
#define USB_REQ_SET_FEATURE                   0x03U
#define USB_REQ_SET_ADDRESS                   0x05U
#define USB_REQ_GET_DESCRIPTOR                0x06U
#define USB_REQ_SET_DESCRIPTOR                0x07U
#define USB_REQ_GET_CONFIGURATION             0x08U
#define USB_REQ_SET_CONFIGURATION             0x09U
#define USB_REQ_GET_INTERFACE                 0x0AU
#define USB_REQ_SET_INTERFACE                 0x0BU
#define USB_REQ_SYNCH_FRAME                   0x0CU

/* Descriptor types */
#define USB_DESC_TYPE_DEVICE                  0x01U
#define USB_DESC_TYPE_CONFIGURATION           0x02U
#define USB_DESC_TYPE_STRING                  0x03U
#define USB_DESC_TYPE_INTERFACE               0x04U
#define USB_DESC_TYPE_ENDPOINT                0x05U
#define USB_DESC_TYPE_DEVICE_QUALIFIER        0x06U
#define USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION 0x07U

/* USB configuration descriptor attributes */
#define USB_CONFIG_POWERED_MASK               0x40U
#define USB_CONFIG_BUS_POWERED                0x80U
#define USB_CONFIG_SELF_POWERED               0xC0U
#define USB_CONFIG_REMOTE_WAKEUP              0x20U

/* Endpoint types */
#define USB_EP_TYPE_CTRL                      0x00U
#define USB_EP_TYPE_ISOC                      0x01U
#define USB_EP_TYPE_BULK                      0x02U
#define USB_EP_TYPE_INTR                      0x03U

/* USB features */
#define USB_FEATURE_ENDPOINT_STALL            0x00U
#define USB_FEATURE_REMOTE_WAKEUP             0x01U

#define USBD_MAX_NUM_INTERFACES               2U
#define USBD_MAX_NUM_CONFIGURATION            1U
#define USBD_MAX_STR_DESC_SIZ                 0x100U
#define USBD_SELF_POWERED                     1U
#define USBD_SUPPORT_USER_STRING_DESC         0U
#define USB_MAX_EP0_SIZE                      64U

#define USBD_DEBUG_LEVEL                      0U

/* Align memory to 4 bytes for USB DMA */
#define USBD_MEM_ALIGNED_BUF(addr) ((uint32_t)(addr) & 0x3U)

#ifdef __cplusplus
}
#endif

#endif /* USBD_DEF_H */
