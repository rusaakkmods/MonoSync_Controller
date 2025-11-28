#include "input.h"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// ---------------------
// GPIO MAPPING
// ---------------------

#define GPIO_BTN_UP      2
#define GPIO_BTN_DOWN    3
#define GPIO_BTN_LEFT    4
#define GPIO_BTN_RIGHT   5
#define GPIO_BTN_A       6
#define GPIO_BTN_B       7
#define GPIO_BTN_START   8
#define GPIO_BTN_SELECT  9

#define GPIO_POT1        26   // ADC0
#define GPIO_POT2        27   // ADC1
#define GPIO_POT3        28   // ADC2
#define GPIO_POT4        29   // ADC3

// ---------------------
// POT STABILIZER CONFIG
// ---------------------

// ADC characteristics
#define ADC_MAX_VALUE            4095.0f

// Oversampling
#define POT_OVERSAMPLE_COUNT     32

// Virtual extended MIDI range, to kill edge twitching
#define POT_VIRTUAL_MIN         -16.0f    // below 0
#define POT_VIRTUAL_MAX         143.0f    // above 127

// EMA smoothing factor (0 < alpha <= 1)
#define POT_SMOOTH_ALPHA         0.10f    // heavier smoothing

// Minimum change (in MIDI steps) before new stable value is accepted
#define POT_CHANGE_THRESHOLD     1.0f

typedef struct {
    float   filtered_f;      // EMA-smoothed virtual value
    float   stable_f;        // last accepted stable value
} pot_internal_t;

static pot_internal_t pots_internal[NUM_POTS];

// ---------------------
// INTERNAL HELPERS
// ---------------------

static void init_buttons(void)
{
    const uint btn_pins[NUM_BUTTONS] = {
        GPIO_BTN_UP, GPIO_BTN_DOWN, GPIO_BTN_LEFT,  GPIO_BTN_RIGHT,
        GPIO_BTN_A,  GPIO_BTN_B,    GPIO_BTN_START, GPIO_BTN_SELECT
    };

    for (int i = 0; i < NUM_BUTTONS; ++i)
    {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);   // active LOW
    }
}

static void init_pots(void)
{
    adc_init();

    adc_gpio_init(GPIO_POT1);
    adc_gpio_init(GPIO_POT2);
    adc_gpio_init(GPIO_POT3);
    adc_gpio_init(GPIO_POT4);

    for (int i = 0; i < NUM_POTS; ++i)
    {
        pots_internal[i].filtered_f = 0.0f;
        pots_internal[i].stable_f   = 0.0f;
    }
}

// Oversampled ADC read
static uint16_t adc_read_oversampled(uint channel)
{
    adc_select_input(channel);
    uint32_t acc = 0;

    for (int i = 0; i < POT_OVERSAMPLE_COUNT; ++i) {
        acc += adc_read();
    }

    return (uint16_t)(acc / POT_OVERSAMPLE_COUNT);
}

// Map raw ADC → virtual extended range → clamp to 0..127
static float adc_to_virtual(uint16_t raw)
{
    float norm = (float)raw / ADC_MAX_VALUE;  // 0..1

    float v = POT_VIRTUAL_MIN +
              norm * (POT_VIRTUAL_MAX - POT_VIRTUAL_MIN);

    if (v < 0.0f)   v = 0.0f;
    if (v > 127.0f) v = 127.0f;

    return v;
}

// ---------------------
// PUBLIC API
// ---------------------

void input_init(void)
{
    init_buttons();
    init_pots();
}

void input_update(controller_state_t *st)
{
    // --- buttons ---
    uint8_t buttons = 0;
    buttons |= (!gpio_get(GPIO_BTN_UP)     ? (1u << 0) : 0);
    buttons |= (!gpio_get(GPIO_BTN_DOWN)   ? (1u << 1) : 0);
    buttons |= (!gpio_get(GPIO_BTN_LEFT)   ? (1u << 2) : 0);
    buttons |= (!gpio_get(GPIO_BTN_RIGHT)  ? (1u << 3) : 0);
    buttons |= (!gpio_get(GPIO_BTN_A)      ? (1u << 4) : 0);
    buttons |= (!gpio_get(GPIO_BTN_B)      ? (1u << 5) : 0);
    buttons |= (!gpio_get(GPIO_BTN_START)  ? (1u << 6) : 0);
    buttons |= (!gpio_get(GPIO_BTN_SELECT) ? (1u << 7) : 0);

    st->buttons = buttons;

    // --- pots ---
    uint16_t raw0 = adc_read_oversampled(0);
    uint16_t raw1 = adc_read_oversampled(1);
    uint16_t raw2 = adc_read_oversampled(2);
    uint16_t raw3 = adc_read_oversampled(3);

    uint16_t raws[NUM_POTS] = { raw0, raw1, raw2, raw3 };

    for (int i = 0; i < NUM_POTS; ++i)
    {
        float v = adc_to_virtual(raws[i]);

        // first-time init
        if (pots_internal[i].filtered_f == 0.0f &&
            pots_internal[i].stable_f   == 0.0f)
        {
            pots_internal[i].filtered_f = v;
            pots_internal[i].stable_f   = v;
        }
        else
        {
            // EMA smoothing
            float diff = v - pots_internal[i].filtered_f;
            pots_internal[i].filtered_f += diff * POT_SMOOTH_ALPHA;
        }

        // Hysteresis in MIDI step space
        float delta = pots_internal[i].filtered_f - pots_internal[i].stable_f;
        if (delta >= POT_CHANGE_THRESHOLD || delta <= -POT_CHANGE_THRESHOLD)
        {
            pots_internal[i].stable_f = pots_internal[i].filtered_f;
        }

        // Quantize to 0..127
        float stable = pots_internal[i].stable_f;
        if (stable < 0.0f)   stable = 0.0f;
        if (stable > 127.0f) stable = 127.0f;

        uint8_t midi_val = (uint8_t)(stable + 0.5f);
        st->pots[i] = midi_val;
    }
}
