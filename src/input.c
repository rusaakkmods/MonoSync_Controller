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

// POT STABILIZER CONFIG
#define ADC_MAX_VALUE           4095.0f
#define POT_OVERSAMPLE_COUNT    32
#define MIDI_MIN_VALUE          0
#define MIDI_MAX_VALUE          127
#define POT_RANGE_OFFSET        16
#define POT_SMOOTH_ALPHA         0.10f
#define POT_CHANGE_THRESHOLD     1.0f

// Virtual extended MIDI range, to kill edge twitching
#define POT_VIRTUAL_MIN         (MIDI_MIN_VALUE - POT_RANGE_OFFSET)
#define POT_VIRTUAL_MAX         (MIDI_MAX_VALUE + POT_RANGE_OFFSET)

typedef struct {
    float   filtered_f;      // EMA-smoothed virtual value
    float   stable_f;        // last accepted stable value
} pot_internal_t;

static pot_internal_t pots_internal[NUM_POTS];

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
        gpio_pull_up(btn_pins[i]);
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

static uint16_t adc_read_oversampled(uint channel)
{
    adc_select_input(channel);
    uint32_t acc = 0;

    for (int i = 0; i < POT_OVERSAMPLE_COUNT; ++i) {
        acc += adc_read();
    }

    return (uint16_t)(acc / POT_OVERSAMPLE_COUNT);
}

static float adc_to_virtual(uint16_t raw)
{
    float norm = (float)raw / ADC_MAX_VALUE;

    float v = POT_VIRTUAL_MIN +
              norm * (POT_VIRTUAL_MAX - POT_VIRTUAL_MIN);

    if (v < MIDI_MIN_VALUE)   v = (float) MIDI_MIN_VALUE;
    if (v > MIDI_MAX_VALUE) v = (float) MIDI_MAX_VALUE;

    return v;
}

void input_init(void)
{
    init_buttons();
    init_pots();
}

void input_update(controller_state_t *st)
{
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

    uint16_t raw0 = adc_read_oversampled(0);
    uint16_t raw1 = adc_read_oversampled(1);
    uint16_t raw2 = adc_read_oversampled(2);
    uint16_t raw3 = adc_read_oversampled(3);
    uint16_t raws[NUM_POTS] = { raw0, raw1, raw2, raw3 };

    for (int i = 0; i < NUM_POTS; ++i)
    {
        float v = adc_to_virtual(raws[i]);

        // init
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

        // Hysteresis
        float delta = pots_internal[i].filtered_f - pots_internal[i].stable_f;
        if (delta >= POT_CHANGE_THRESHOLD || delta <= -POT_CHANGE_THRESHOLD)
        {
            pots_internal[i].stable_f = pots_internal[i].filtered_f;
        }

        // Quantize/clamp
        float stable = pots_internal[i].stable_f;
        if (stable < MIDI_MIN_VALUE)   stable = (float) MIDI_MIN_VALUE;
        if (stable > MIDI_MAX_VALUE) stable = (float) MIDI_MAX_VALUE;

        uint8_t midi_val = (uint8_t)(stable + 0.5f);
        st->pots[i] = midi_val;
    }
}
