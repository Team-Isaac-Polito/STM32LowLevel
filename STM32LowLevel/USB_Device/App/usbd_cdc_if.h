/**
 * @file    usbd_cdc_if.h
 * @brief   USB CDC interface header (application callbacks).
 */

#ifndef USBD_CDC_IF_H
#define USBD_CDC_IF_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "usbd_cdc.h"

    /* Exported functions */
    uint8_t CDC_Init(void);
    uint8_t CDC_DeInit(void);
    uint8_t CDC_Control(uint8_t cmd, uint8_t* pbuf, uint16_t length);
    uint8_t CDC_Receive(uint8_t* Buf, uint32_t* Len);
    uint8_t CDC_TransmitCplt(uint8_t* Buf, uint32_t* Len, uint8_t epnum);

    /* Public API for debug output */
    uint8_t CDC_IsConnected(void);
    uint8_t CDC_Transmit(const uint8_t* buf, uint32_t len);
    void CDC_ForceTx(void);

    /* Wait for the USB host to connect and communicate.
     * Blocks until DTR is set and a USB OUT transaction completes,
     * indicating the host serial port is open and ready.
     * Called after USB init on software reset to pause boot until
     * the serial monitor reconnects. */
    void CDC_WaitForHost(void);

    /* CDC interface operations (for USB stack) */
    extern USBD_CDC_ItfTypeDef USBD_CDC_fops;

#ifdef __cplusplus
}
#endif

#endif /* USBD_CDC_IF_H */
