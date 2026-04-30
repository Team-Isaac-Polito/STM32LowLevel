/**
 * @file CanWrapper.h
 * @brief FDCAN2 register-level CAN wrapper for STM32G474 (PB12 RX, PB13 TX).
 *
 * Runtime TX: writes directly to FDCAN2->TXBAR after filling message RAM.
 * Runtime RX: reads directly from FDCAN2->RXF0S.
 *
 * CAN_STBY (PB15): HIGH on boot → driven LOW in begin() after MX_FDCAN2_Init.
 * CAN_SHDN (PB11): held LOW at all times (driven LOW in gpio.c and in begin()).
 *
 * 29-bit extended CAN frame format:
 *   bits[23:16] = msgType  (PDU format / packet identifier)
 *   bits[15:8]  = destination address (CAN_ID of receiving module)
 *   bits[7:0]   = source address      (CAN_ID of sending module)
 */

#ifndef CAN_WRAPPER_H
#define CAN_WRAPPER_H

#include <cstdint>

class CanWrapper {
public:
    CanWrapper() {}

    /**
     * @brief Configure FDCAN2 filter, start peripheral and enable transceiver.
     *
     * Must be called once after MX_FDCAN2_Init().  Configures one extended-ID
     * mask filter (bits[15:8] == CAN_ID), rejects non-matching frames, starts
     * the FDCAN peripheral, and drives CAN_STBY (PB15) LOW to put the
     * TCAN3414 transceiver into normal operating mode.
     */
    void begin();

    /**
     * @brief Transmit a CAN frame (register-level, non-blocking).
     *
     * Writes directly to message RAM and sets the corresponding bit in
     * FDCAN2->TXBAR to request transmission.
     *
     * @param msgType PDU format byte (packet identifier, e.g. MOTOR_SETPOINT).
     * @param data    Pointer to payload bytes.
     * @param length  Payload length in bytes (max 8).
     * @return True if the message was placed in the TX FIFO, false if full.
     */
    bool sendMessage(uint8_t msgType, const void *data, uint8_t length);

    /**
     * @brief Receive a CAN frame from RX FIFO 0 (register-level, non-blocking).
     *
     * Reads directly from message RAM and acknowledges the FIFO entry via
     * FDCAN2->RXF0A.
     *
     * @param[out] msgType Receives the PDU format byte (bits[23:16] of ext ID).
     * @param[out] data    Pointer to a buffer of at least 8 bytes for payload.
     * @return True if a message was available and read, false if FIFO empty.
     */
    bool readMessage(uint8_t *msgType, uint8_t *data);
};

#endif // CAN_WRAPPER_H
