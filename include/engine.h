#pragma once
#include "state.h"

#ifdef __cplusplus
extern "C" {
#endif

void engine_init(void);
void engine_process(const controller_state_t *st);

#ifdef __cplusplus
}
#endif
