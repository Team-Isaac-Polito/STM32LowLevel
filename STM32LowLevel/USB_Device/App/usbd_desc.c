/**
 * @file    usbd_desc.c
 * @brief   USB Device descriptors (CDC ACM VCP).
 */

#include "usbd_desc.h"
#include "usbd_core.h"
#include "usbd_conf.h"

#define USB_LEN_DEV_DESC 18U
#define USB_LEN_LANGID_STR_DESC 4U

#define USBD_VID 0x0483U           /* STMicroelectronics */
#define USBD_PID 0x5740U           /* STM32 Virtual COM Port */
#define USBD_LANGID_STRING 0x0409U /* English (US) */
#define USBD_MANUFACTURER_STRING "Team Isaac"
#define USBD_PRODUCT_STRING "ReseQ Debug VCP"
#define USBD_CONFIGURATION_STRING "CDC Config"
#define USBD_INTERFACE_STRING "CDC Interface"

/* USB Standard Device Descriptor */
__ALIGN_BEGIN static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,                 /* bLength */
    USB_DESC_TYPE_DEVICE, /* bDescriptorType */
    0x00,
    0x02,             /* bcdUSB (2.00) */
    0x02,             /* bDeviceClass (CDC) */
    0x02,             /* bDeviceSubClass */
    0x00,             /* bDeviceProtocol */
    USB_MAX_EP0_SIZE, /* bMaxPacketSize0 */
    LOBYTE(USBD_VID), /* idVendor */
    HIBYTE(USBD_VID),
    LOBYTE(USBD_PID), /* idProduct */
    HIBYTE(USBD_PID),
    0x00,
    0x01, /* bcdDevice (1.00) */
    1,    /* iManufacturer */
    2,    /* iProduct */
    3,    /* iSerialNumber */
    1,    /* bNumConfigurations */
};

/* USB LangID Descriptor */
__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

/* USB String Descriptors */
static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ];

/* Serial number — use unique chip ID */
static void Get_SerialNum(void);

/**
 * @brief  Converts a C string to a USB Unicode string descriptor.
 */
static void USBD_GetString(uint8_t* desc, uint8_t* unicode, uint16_t* len)
{
    uint8_t idx = 0U;
    uint8_t length = 0U;
    uint8_t* tmp = desc;

    while (*tmp++ != '\0')
        length++;

    *len = (uint16_t)(length * 2U + 2U);
    unicode[idx++] = (uint8_t)*len;
    unicode[idx++] = USB_DESC_TYPE_STRING;
    while (*desc != '\0')
    {
        unicode[idx++] = *desc++;
        unicode[idx++] = 0x00U;
    }
}

/**
 * @brief  Returns the device descriptor.
 */
uint8_t* USBD_DeviceDescriptor(uint16_t* length)
{
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

/**
 * @brief  Returns the LangID string descriptor.
 */
uint8_t* USBD_LangIDStrDescriptor(uint16_t* length)
{
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

/**
 * @brief  Returns the manufacturer string descriptor.
 */
uint8_t* USBD_ManufacturerStrDescriptor(uint16_t* length)
{
    USBD_GetString((uint8_t*)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

/**
 * @brief  Returns the product string descriptor.
 */
uint8_t* USBD_ProductStrDescriptor(uint16_t* length)
{
    USBD_GetString((uint8_t*)USBD_PRODUCT_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

/**
 * @brief  Returns the serial number string descriptor.
 */
uint8_t* USBD_SerialStrDescriptor(uint16_t* length)
{
    Get_SerialNum();
    return (uint8_t*)USBD_StrDesc;
}

/**
 * @brief  Returns the configuration string descriptor.
 */
uint8_t* USBD_ConfigurationStrDescriptor(uint16_t* length)
{
    USBD_GetString((uint8_t*)USBD_CONFIGURATION_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

/**
 * @brief  Returns the interface string descriptor.
 */
uint8_t* USBD_InterfaceStrDescriptor(uint16_t* length)
{
    USBD_GetString((uint8_t*)USBD_INTERFACE_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

/**
 * @brief  Creates the serial number string descriptor using the unique chip ID.
 */
static void Get_SerialNum(void)
{
    uint32_t deviceserial0 = *(uint32_t*)0x1FFF7590U; /* STM32G4 unique ID */
    uint32_t deviceserial1 = *(uint32_t*)0x1FFF7594U;
    uint32_t deviceserial2 = *(uint32_t*)0x1FFF7598U;

    deviceserial0 += deviceserial2;

    /* 16 hex chars (8 from each word) => UTF-16LE string descriptor of 34 bytes */
    USBD_StrDesc[0] = 34U;
    USBD_StrDesc[1] = USB_DESC_TYPE_STRING;

    for (uint8_t i = 0; i < 8U; i++)
    {
        uint8_t nibble = (uint8_t)((deviceserial0 >> (28U - (i * 4U))) & 0x0FU);
        USBD_StrDesc[2U + (i * 2U)] = (uint8_t)(nibble < 10U ? ('0' + nibble) : ('A' + nibble - 10U));
        USBD_StrDesc[3U + (i * 2U)] = 0x00U;
    }

    for (uint8_t i = 0; i < 8U; i++)
    {
        uint8_t nibble = (uint8_t)((deviceserial1 >> (28U - (i * 4U))) & 0x0FU);
        USBD_StrDesc[18U + (i * 2U)] = (uint8_t)(nibble < 10U ? ('0' + nibble) : ('A' + nibble - 10U));
        USBD_StrDesc[19U + (i * 2U)] = 0x00U;
    }
}

/* Descriptor operations */
USBD_DescriptorsTypeDef CDC_Desc = {
    USBD_DeviceDescriptor,
    USBD_LangIDStrDescriptor,
    USBD_ManufacturerStrDescriptor,
    USBD_ProductStrDescriptor,
    USBD_SerialStrDescriptor,
    USBD_ConfigurationStrDescriptor,
    USBD_InterfaceStrDescriptor,
};
