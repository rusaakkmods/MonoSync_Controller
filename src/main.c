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
    board_init();
    tusb_init();

    input_init();
    engine_init();

    controller_state_t st = {0};

    while (true)
    {
        tud_task();

        input_update(&st);
        engine_process(&st);
    }

    return 0;
}
