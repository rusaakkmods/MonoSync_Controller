#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// USB Product ID – change if you like
#define USB_VID         0xCafe
#define USB_PID         0x4001

// String descriptor indices
enum
{
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_HID,
    STRID_MIDI,
    STRID_COUNT
};

// HID report descriptor (implemented in usb_descriptors.c)
extern uint8_t const desc_hid_report[];

#ifdef __cplusplus
}
#endif
