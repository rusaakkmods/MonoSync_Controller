#include "usb_midi.h"
#include "tusb.h"

#define MIDI_CABLE_NUM   0
#define MIDI_STATUS_CC   0xB0

void usb_midi_init(void)
{
    // nothing special, TinyUSB device init happens in main via tusb_init()
}

void usb_midi_send_cc(uint8_t channel, uint8_t cc, uint8_t value)
{
    if (!tud_midi_mounted()) return;

    if (channel < 1)  channel = 1;
    if (channel > 16) channel = 16;

    uint8_t status = MIDI_STATUS_CC | ((channel - 1) & 0x0F);

    uint8_t msg[3] = { status, cc, value };
    tud_midi_stream_write(MIDI_CABLE_NUM, msg, 3);
}
