#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <stdio.h>

/* =========================
 *  CONFIGURATION
 * ========================= */

// Button pins (RP2040 GPIO numbers)
#define BTN_UP      2
#define BTN_DOWN    3
#define BTN_LEFT    4
#define BTN_RIGHT   5
#define BTN_A       6
#define BTN_B       7
#define BTN_START   8
#define BTN_SELECT  9

// Pot pins (ADC-capable GPIOs)
#define POT1_PIN    26  // ADC0
#define POT2_PIN    27  // ADC1
#define POT3_PIN    28  // ADC2
#define POT4_PIN    29  // ADC3

// ADC characteristics
#define ADC_BITS            12
#define ADC_MAX_VALUE       ((1u << ADC_BITS) - 1u)   // 4095 for 12-bit

// Pot reading behavior
#define POT_OVERSAMPLE_N                32      // number of samples per read
#define POT_CHANGE_THRESHOLD_STEPS      1.0f    // minimal MIDI-step change to accept

// MIDI range
#define MIDI_MIN_VALUE                  0.0f
#define MIDI_MAX_VALUE                  127.0f

// Virtual extended range for mapping (before clamping)
//  -16 .. 143 is what worked well for you
#define VIRTUAL_MIN_VALUE              -32.0f
#define VIRTUAL_MAX_VALUE              159.0f

/* =========================
 *  TYPES
 * ========================= */

typedef struct {
    uint8_t midi_stable;   // last accepted MIDI value (0..127)
    uint8_t last_printed;  // last value that was printed
} pot_state_t;

/* =========================
 *  GLOBAL STATE
 * ========================= */

static pot_state_t pots[4];

/* =========================
 *  INIT FUNCTIONS
 * ========================= */

static void init_buttons(void) {
    const uint btn_pins[] = {
        BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
        BTN_A, BTN_B, BTN_START, BTN_SELECT
    };

    for (int i = 0; i < 8; i++) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);   // buttons are active LOW
    }
}

static void init_pots(void) {
    adc_init();
    adc_gpio_init(POT1_PIN);
    adc_gpio_init(POT2_PIN);
    adc_gpio_init(POT3_PIN);
    adc_gpio_init(POT4_PIN);

    for (int i = 0; i < 4; i++) {
        pots[i].midi_stable  = 0;
        pots[i].last_printed = 255;  // force initial print
    }
}

/* =========================
 *  ADC / POT HELPERS
 * ========================= */

// Oversampled average read of a given ADC channel (0..3)
// Returns floating value in raw ADC units (0..ADC_MAX_VALUE).
static float read_pot_avg(uint8_t channel) {
    adc_select_input(channel);
    (void)adc_read();   // dummy read after channel switch to settle
    sleep_us(5);

    uint32_t acc = 0;

    for (int i = 0; i < POT_OVERSAMPLE_N; i++) {
        acc += adc_read();
        sleep_us(5);
    }

    float avg = (float)acc / (float)POT_OVERSAMPLE_N;
    if (avg < 0.0f) avg = 0.0f;
    if (avg > (float)ADC_MAX_VALUE) avg = (float)ADC_MAX_VALUE;
    return avg;
}

// Map raw ADC (0..ADC_MAX_VALUE) to MIDI using virtual extended range.
//
// 1. Normalize raw to 0..1
// 2. Scale to VIRTUAL_MIN_VALUE..VIRTUAL_MAX_VALUE
// 3. Clamp to MIDI_MIN_VALUE..MIDI_MAX_VALUE
// 4. Round to nearest integer
//
static uint8_t adc_to_midi(float raw_adc) {
    // Normalize ADC to 0..1
    const float adc_max_f     = (float)ADC_MAX_VALUE;
    const float virtual_span  = VIRTUAL_MAX_VALUE - VIRTUAL_MIN_VALUE;

    float t = raw_adc / adc_max_f;       // 0..1
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // Map to virtual extended range (e.g. -16..143)
    float virtual_val = VIRTUAL_MIN_VALUE + t * virtual_span;

    // Clamp to MIDI range (0..127)
    if (virtual_val < MIDI_MIN_VALUE) virtual_val = MIDI_MIN_VALUE;
    if (virtual_val > MIDI_MAX_VALUE) virtual_val = MIDI_MAX_VALUE;

    // Round to nearest integer
    return (uint8_t)(virtual_val + 0.5f);
}

/* =========================
 *  MAIN
 * ========================= */

int main() {
    stdio_init_all();
    sleep_ms(2000);  // let USB enumerate

    init_buttons();
    init_pots();

    uint8_t last_buttons_printed = 0xFF;

    while (true) {
        /* ---------- READ BUTTONS ---------- */
        uint8_t buttons = 0;
        buttons |= (!gpio_get(BTN_UP)     ? (1 << 0) : 0);
        buttons |= (!gpio_get(BTN_DOWN)   ? (1 << 1) : 0);
        buttons |= (!gpio_get(BTN_LEFT)   ? (1 << 2) : 0);
        buttons |= (!gpio_get(BTN_RIGHT)  ? (1 << 3) : 0);
        buttons |= (!gpio_get(BTN_A)      ? (1 << 4) : 0);
        buttons |= (!gpio_get(BTN_B)      ? (1 << 5) : 0);
        buttons |= (!gpio_get(BTN_START)  ? (1 << 6) : 0);
        buttons |= (!gpio_get(BTN_SELECT) ? (1 << 7) : 0);

        /* ---------- READ POTS (oversampled) ---------- */
        float raw0 = read_pot_avg(0); // ADC0 / GPIO26
        float raw1 = read_pot_avg(1); // ADC1 / GPIO27
        float raw2 = read_pot_avg(2); // ADC2 / GPIO28
        float raw3 = read_pot_avg(3); // ADC3 / GPIO29

        uint8_t new_vals[4];
        new_vals[0] = adc_to_midi(raw0);
        new_vals[1] = adc_to_midi(raw1);
        new_vals[2] = adc_to_midi(raw2);
        new_vals[3] = adc_to_midi(raw3);

        /* ---------- APPLY CHANGE THRESHOLD ---------- */

        for (int i = 0; i < 4; i++) {
            float diff = (float)new_vals[i] - (float)pots[i].midi_stable;
            if (diff < 0.0f) diff = -diff;

            if (diff > POT_CHANGE_THRESHOLD_STEPS) {
                pots[i].midi_stable = new_vals[i];
            }
        }

        /* ---------- DECIDE WHETHER TO PRINT ---------- */
        bool changed = false;

        if (buttons != last_buttons_printed) {
            changed = true;
        }

        for (int i = 0; i < 4; i++) {
            if (pots[i].midi_stable != pots[i].last_printed) {
                changed = true;
            }
        }

        /* ---------- PRINT ON CHANGE ---------- */
        if (changed) {
            last_buttons_printed = buttons;

            printf("BTN: ");
            for (int i = 0; i < 8; i++) {
                printf("%c", (buttons & (1 << i)) ? '1' : '0');
            }

            printf(" | POTS: %3u %3u %3u %3u\r\n",
                   pots[0].midi_stable,
                   pots[1].midi_stable,
                   pots[2].midi_stable,
                   pots[3].midi_stable);

            for (int i = 0; i < 4; i++) {
                pots[i].last_printed = pots[i].midi_stable;
            }
        }

        sleep_ms(1);
    }

    return 0;
}
