#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// RP2040: only RHPort0 in device mode at full speed
#define CFG_TUSB_RHPORT0_MODE      (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

// Control endpoint size
#define CFG_TUD_ENDPOINT0_SIZE     64

// Memory alignment (RP2040 is fine with this)
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN         __attribute__ ((aligned(4)))

//--------------------------------------------------------------------
// DEVICE CLASS CONFIGURATION
//--------------------------------------------------------------------

// We want HID + MIDI, nothing else.
#define CFG_TUD_CDC                0
#define CFG_TUD_MSC                0
#define CFG_TUD_HID                1
#define CFG_TUD_MIDI               1
#define CFG_TUD_VENDOR             0
#define CFG_TUD_AUDIO              0

// HID
#define CFG_TUD_HID_EP_BUFSIZE     64

// MIDI
// Endpoint buffer (max packet size)
#define CFG_TUD_MIDI_EP_BUFSIZE    64

// FIFO sizes used by class/midi/midi_device.c
// 64 bytes is plenty for full-speed, single cable
#define CFG_TUD_MIDI_RX_BUFSIZE    64
#define CFG_TUD_MIDI_TX_BUFSIZE    64

#ifdef __cplusplus
}
#endif
