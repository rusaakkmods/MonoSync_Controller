#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"

#include "state.h"
#include "input.h"
#include "engine.h"

int main(void)
{
    // Initialize board (clocks, USB pins, etc)
    board_init();

    // Initialize TinyUSB device stack
    tusb_init();

    // Initialize our own modules
    input_init();
    engine_init();

    controller_state_t st = {0};

    while (true)
    {
        // TinyUSB device task - must be called frequently
        tud_task();

        // Read GPIO/ADC and update controller state
        input_update(&st);

        // Send HID + MIDI if anything changed
        engine_process(&st);
    }

    return 0;
}
