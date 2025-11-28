#pragma once
#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_gamepad_init(void);
void usb_gamepad_update(const controller_state_t *st);

#ifdef __cplusplus
}
#endif
