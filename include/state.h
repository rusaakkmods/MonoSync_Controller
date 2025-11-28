#pragma once
#include <stdint.h>

#define NUM_BUTTONS 8
#define NUM_POTS    4

// bit0: UP
// bit1: DOWN
// bit2: LEFT
// bit3: RIGHT
// bit4: A
// bit5: B
// bit6: START
// bit7: SELECT

typedef struct {
    uint8_t buttons;             // 8-bit button field: this mean only 8 buttons supported
    uint8_t pots[NUM_POTS];      // stable MIDI values 0..127
} controller_state_t;
