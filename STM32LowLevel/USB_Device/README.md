# USB Device Stack — CDC ACM Virtual COM Port

## Purpose

This directory contains a **custom USB Device stack** that implements a **CDC ACM (Abstract Control Model) Virtual COM Port** on the STM32G474. It provides a USB-based debug console that behaves like a serial port.

When the board is connected to a PC via USB, it enumerates as a standard CDC COM port (e.g. `COM5`). Any terminal program can open it at 115200 baud to see debug output.

## Concept

### What is CDC ACM?

CDC (Communications Device Class) is a USB standard for communication devices. ACM is a subclass that emulates a serial port over USB. The host OS loads a generic `usbser.sys` driver and exposes a COM port — no custom driver needed.

### How it works

```
┌─────────────┐     USB      ┌──────────────┐     COM port     ┌──────────┐
│  STM32G474  │ ◄──────────► │  Host (PC)   │ ◄──────────────► │ Terminal │
│  CDC Device │   D+ / D-    │  usbser.sys  │   COMx, 115200   │  PuTTY   │
└─────────────┘              └──────────────┘                  └──────────┘
```

1. **Physical layer**: USB D+ (PA12) and D- (PA11) differential pair.
2. **USB Peripheral (PCD)**: The STM32G474's built-in USB peripheral handles low-level packet framing, NRZI encoding, and SOF tokens.
3. **USB Device Stack**: The custom stack in this directory handles enumeration, standard requests, and CDC class requests.
4. **Application interface**: `CDC_Transmit()` and `CDC_IsConnected()` in `usbd_cdc_if.c` are the API the rest of the firmware uses.

### Enumeration sequence

```
Host ──► Reset ──► Get Device Descriptor ──► Set Address ──►
Get Config Descriptor ──► Set Configuration ──► CDC ready
```

The stack must respond correctly to every step. A failure at any point produces Windows errors like:
- `Device Descriptor Request Failed` — PCD not running or IRQs not wired
- `Configuration Descriptor Request Failed` — descriptor malformed or EP0 transfer broken

## Architecture

The stack is organized in three layers:

```
USB_Device/
├── Core/           ← Generic USB Device stack (class-agnostic)
│   ├── Inc/        ← Headers: usbd_core.h, usbd_ctlreq.h, usbd_def.h, usbd_ioreq.h
│   └── Src/        ← Implementation: usbd_core.c, usbd_ctlreq.c, usbd_ioreq.c
│
├── Class/CDC/      ← CDC ACM class implementation
│   ├── Inc/        ← usbd_cdc.h (endpoint addresses, class request IDs, handle struct)
│   └── Src/        ← usbd_cdc.c (descriptors, class callbacks, Init/DeInit/Setup/DataIn/DataOut)
│
└── App/            ← Application glue (board-specific)
    ├── usbd_conf.c     ← PCD callbacks, endpoint config, NVIC, MX_USB_Device_Init()
    ├── usbd_conf.h     ← Stack configuration (max interfaces, max packet size)
    ├── usbd_desc.c     ← Device/string descriptors (VID, PID, manufacturer, serial)
    ├── usbd_desc.h     ← Descriptor size constant
    ├── usbd_cdc_if.c   ← CDC application callbacks (TX ring buffer, RX, line coding, DTR)
    └── usbd_cdc_if.h   ← Public API: CDC_Transmit(), CDC_IsConnected()
```

### Core layer (`USB_Device/Core/`)

| File | Role |
|---|---|
| `usbd_core.c` | State machine: `USBD_Init`, `USBD_Start`, `USBD_Reset`, EP0 setup/data stages, class dispatch |
| `usbd_ctlreq.c` | Standard request handler: `GET_DESCRIPTOR`, `SET_ADDRESS`, `SET_CONFIGURATION`, string descriptors |
| `usbd_ioreq.c` | EP0 data helpers: `USBD_CtlSendData`, `USBD_CtlPrepareRx`, `USBD_CtlSendStatus` |
| `usbd_def.h` | Types, constants, `USBD_HandleTypeDef`, `USBD_ClassTypeDef` |

### Class layer (`USB_Device/Class/CDC/`)

| File | Role |
|---|---|
| `usbd_cdc.c` | CDC class callbacks: `Init`, `DeInit`, `Setup`, `DataIn`, `DataOut`, descriptor getters |
| `usbd_cdc.h` | Endpoint addresses (`CDC_IN_EP 0x81`, `CDC_OUT_EP 0x01`, `CDC_CMD_EP 0x82`), packet sizes, line coding struct |

The CDC configuration descriptor (75 bytes) contains:
- Configuration descriptor
- Interface Association Descriptor (IAD)
- Communication Interface (CDC header, call management, ACM, union functional descriptors)
- Notification endpoint (EP2 IN, interrupt)
- Data Interface (CDC data class)
- Bulk IN endpoint (EP1 IN)
- Bulk OUT endpoint (EP1 OUT)

### Application layer (`USB_Device/App/`)

| File | Role |
|---|---|
| `usbd_conf.c` | PCD ↔ stack bridge: all `HAL_PCD_*` callbacks, `USBD_LL_Init/Start/Stop/OpenEP/CloseEP/Transmit/Receive`, NVIC configuration |
| `usbd_desc.c` | Device descriptor (VID 0x0483, PID 0x5740), string descriptors (manufacturer, product, serial from unique chip ID) |
| `usbd_cdc_if.c` | TX ring buffer (1024 bytes), RX buffer, line coding (115200 8N1), DTR connection tracking |
| `usbd_cdc_if.h` | Public API for firmware: `CDC_Transmit(buf, len)`, `CDC_IsConnected()` |

## Integration with Debug Library

The `Debug` library (`Lib/Debug/`) outputs to **both** UART5 and USB CDC:

```cpp
// Debug.cpp — putchar sends to UART5 always, and USB CDC when connected
void SerialDebug::putchar(char ch) {
    // UART5 (always)
    while (!LL_USART_IsActiveFlag_TXE(UART5)) {}
    LL_USART_TransmitData8(UART5, ch);

    // USB CDC (only when host has DTR set)
    if (CDC_IsConnected()) {
        CDC_Transmit((const uint8_t *)&ch, 1U);
    }
}
```

This means:
- **UART5** (PC12/TX, PD2/RX) works as the primary debug console via a USB-to-serial adapter
- **USB CDC** works as a secondary debug console when connected directly via USB
- `printf()` is retargeted to the same output via `_write` syscall override

## Usage

### Building

The USB device library is compiled as part of the normal build. No special flags needed:

```bash
cd STM32LowLevel/STM32LowLevel
cmake --preset MK2_MOD1
cmake --build build/MK2_MOD1
```

### Flashing

```bash
arm-none-eabi-objcopy -O binary build/MK2_MOD1/STM32LowLevel.elf build/MK2_MOD1/STM32LowLevel.bin
dfu-util -d 0483:df11 -a 0 --dfuse-address 0x08000000 -D build/MK2_MOD1/STM32LowLevel.bin
```

### Connecting

1. Plug the board into a PC via USB
2. Wait for enumeration (a new COM port appears)
3. Find the port:
   ```powershell
   [System.IO.Ports.SerialPort]::GetPortNames()
   ```
4. Open a terminal at 115200 baud, 8N1

### From firmware

```cpp
#include "usbd_cdc_if.h"

// Check if host is connected (DTR set)
if (CDC_IsConnected()) {
    CDC_Transmit((const uint8_t *)"hello\r\n", 7U);
}
```

Or simply use `Debug.log()` / `printf()` — they route through automatically.

## Technical Reference

- **USB 2.0 Specification**: [usb.org/documents](https://www.usb.org/documents)
- **CDC ACM Specification**: USB Class Definitions for Communications Devices, Rev 1.2
- **STM32G474 Reference Manual**: RM0440, Chapter 35 (USB full-speed device)
- **STM32 USB Device Library**: Based on ST's USB Device middleware, simplified for CDC-only use
