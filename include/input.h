#pragma once
#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

void input_init(void);
void input_update(controller_state_t *st);

#ifdef __cplusplus
}
#endif
