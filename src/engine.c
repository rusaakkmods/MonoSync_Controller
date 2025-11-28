#include "engine.h"
#include "usb_gamepad.h"
#include "usb_midi.h"

#include "pico/stdlib.h"

#define LOOP_DELAY_MS      5u
#define BUTTON_SENTINEL    0xFFu
#define POT_SENTINEL       0xFFu

static controller_state_t last_state;

// CC mapping
static const uint8_t pot_cc_map[NUM_POTS] = { 20, 21, 22, 23 };
static const uint8_t midi_channel         = 1; // 1..16

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

    // Gamepad: send every loop (internal TU can compress)
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

    // MIDI: send CC for pots that changed
    for (int i = 0; i < NUM_POTS; ++i) {
        if (st->pots[i] != last_state.pots[i]) {
            usb_midi_send_cc(midi_channel, pot_cc_map[i], st->pots[i]);
        }
    }

    last_state = *st;
    sleep_ms(LOOP_DELAY_MS);
}
