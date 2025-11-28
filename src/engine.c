#include "engine.h"
#include "usb_gamepad.h"
#include "usb_midi.h"

#include "pico/stdlib.h"

#define LOOP_DELAY_MS      5u
#define BUTTON_SENTINEL    0xFFu
#define POT_SENTINEL       0xFFu

#define DEFAULT_CC_CHANNEL 1

#define CTL1_CC        20
#define CTL2_CC        21
#define CTL3_CC        22
#define CTL4_CC        23

static controller_state_t last_state;

static const uint8_t pot_cc_map[NUM_POTS] = { CTL1_CC, CTL2_CC, CTL3_CC, CTL4_CC };
static const uint8_t midi_channel         = DEFAULT_CC_CHANNEL;

void engine_init(void)
{
    last_state.buttons = BUTTON_SENTINEL;
    for (int i = 0; i < NUM_POTS; ++i) {
        last_state.pots[i] = POT_SENTINEL;
    }

    usb_gamepad_init();
    usb_midi_init();
}

void engine_process(const controller_state_t *st)
{
    bool changed = false;
    usb_gamepad_update(st);

    if (st->buttons != last_state.buttons) {
        changed = true;
    } else {
        for (int i = 0; i < NUM_POTS; ++i) {
            if (st->pots[i] != last_state.pots[i]) {
                changed = true;
                break;
            }
        }
    }

    if (!changed) {
        sleep_ms(LOOP_DELAY_MS);
        return;
    }

    for (int i = 0; i < NUM_POTS; ++i) {
        if (st->pots[i] != last_state.pots[i]) {
            usb_midi_send_cc(midi_channel, pot_cc_map[i], st->pots[i]);
        }
    }

    last_state = *st;
    sleep_ms(LOOP_DELAY_MS);
}
