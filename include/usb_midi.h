#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_midi_init(void);
void usb_midi_send_cc(uint8_t channel, uint8_t cc, uint8_t value);

#ifdef __cplusplus
}
#endif
