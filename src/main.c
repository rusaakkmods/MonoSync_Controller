#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

#include "bsp/board.h"
#include "tusb.h"
#include "class/hid/hid.h"   // for hid_gamepad_report_t
#include "usb_descriptors.h"

// =====================================================================
// CONFIGURATION SECTION
// =====================================================================

// ----------------- Gamepad GPIO pins (active LOW) ---------------------
#define GPIO_BTN_UP         2
#define GPIO_BTN_DOWN       3
#define GPIO_BTN_LEFT       4
#define GPIO_BTN_RIGHT      5
#define GPIO_BTN_A          6
#define GPIO_BTN_B          7
#define GPIO_BTN_START      8
#define GPIO_BTN_SELECT     9

// --------------------- Potentiometer GPIO pins ------------------------
#define GPIO_POT0           26      // ADC0
#define GPIO_POT1           27      // ADC1
#define GPIO_POT2           28      // ADC2
#define GPIO_POT3           29      // ADC3

// ------------------------ ADC parameters ------------------------------
#define ADC_RESOLUTION_BITS         12
#define ADC_MAX_VALUE               ((1u << ADC_RESOLUTION_BITS) - 1u)

#define ADC_SETTLE_DELAY_US         5       // delay after switching channel

// --------------------- Pot sampling & filtering ----------------------
#define POT_SCAN_INTERVAL_MS        50      // how often to scan pots
#define POT_OVERSAMPLE_COUNT        32      // samples per pot read

// Virtual extended MIDI range before clamping
#define POT_VIRTUAL_MIN_VALUE       (-16.0f)
#define POT_VIRTUAL_MAX_VALUE       (143.0f)

// MIDI final range
#define MIDI_MIN_VALUE              (0.0f)
#define MIDI_MAX_VALUE              (127.0f)

// Minimum change in MIDI value (in steps) required to update stable value
#define POT_CHANGE_THRESHOLD_STEPS  (2.0f)

// --------------------------- MIDI config ------------------------------
#define MIDI_CHANNEL_INDEX          0       // channel 1 (0-based)
#define MIDI_CABLE_NUMBER           0       // only 1 virtual cable

// CC numbers for the four pots
#define POT_COUNT                   4
static const uint8_t POT_MIDI_CC[POT_COUNT] = { 20, 21, 22, 23 };

// Value used to force "first send" for MIDI
#define MIDI_UNINITIALIZED_VALUE    255u

// ------------------------ Gamepad HID config --------------------------
#define HID_REPORT_ID_GAMEPAD       1

// Axes full-scale values
#define GAMEPAD_AXIS_MIN            (-127)
#define GAMEPAD_AXIS_MAX            (127)

// Bit positions in our internal button bitfield
#define BTN_BIT_UP                  0
#define BTN_BIT_DOWN                1
#define BTN_BIT_LEFT                2
#define BTN_BIT_RIGHT               3
#define BTN_BIT_A                   4
#define BTN_BIT_B                   5
#define BTN_BIT_START               6
#define BTN_BIT_SELECT              7

// Map which game buttons become HID "buttons" (0..15)
#define HID_BTN_INDEX_A             0
#define HID_BTN_INDEX_B             1
#define HID_BTN_INDEX_START         2
#define HID_BTN_INDEX_SELECT        3

// =====================================================================
// TYPES & GLOBAL STATE
// =====================================================================

typedef struct {
    float   midi_stable_f;      // last accepted MIDI value (float 0..127)
    uint8_t midi_last_sent;     // last MIDI integer value actually sent
} pot_state_t;

static pot_state_t pots[POT_COUNT];

// =====================================================================
// BUTTONS
// =====================================================================

static void init_buttons(void)
{
    const uint btn_pins[] = {
        GPIO_BTN_UP,
        GPIO_BTN_DOWN,
        GPIO_BTN_LEFT,
        GPIO_BTN_RIGHT,
        GPIO_BTN_A,
        GPIO_BTN_B,
        GPIO_BTN_START,
        GPIO_BTN_SELECT
    };

    for (size_t i = 0; i < (sizeof btn_pins / sizeof btn_pins[0]); ++i)
    {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);  // active LOW
    }
}

// Return bitfield using BTN_BIT_* constants
static uint8_t read_buttons_bitfield(void)
{
    uint8_t buttons = 0;

    buttons |= (!gpio_get(GPIO_BTN_UP)     ? (1u << BTN_BIT_UP)     : 0u);
    buttons |= (!gpio_get(GPIO_BTN_DOWN)   ? (1u << BTN_BIT_DOWN)   : 0u);
    buttons |= (!gpio_get(GPIO_BTN_LEFT)   ? (1u << BTN_BIT_LEFT)   : 0u);
    buttons |= (!gpio_get(GPIO_BTN_RIGHT)  ? (1u << BTN_BIT_RIGHT)  : 0u);
    buttons |= (!gpio_get(GPIO_BTN_A)      ? (1u << BTN_BIT_A)      : 0u);
    buttons |= (!gpio_get(GPIO_BTN_B)      ? (1u << BTN_BIT_B)      : 0u);
    buttons |= (!gpio_get(GPIO_BTN_START)  ? (1u << BTN_BIT_START)  : 0u);
    buttons |= (!gpio_get(GPIO_BTN_SELECT) ? (1u << BTN_BIT_SELECT) : 0u);

    return buttons;
}

// =====================================================================
// POTS / ADC
// =====================================================================

static void init_pots(void)
{
    adc_init();
    adc_gpio_init(GPIO_POT0);
    adc_gpio_init(GPIO_POT1);
    adc_gpio_init(GPIO_POT2);
    adc_gpio_init(GPIO_POT3);

    for (int i = 0; i < POT_COUNT; ++i)
    {
        pots[i].midi_stable_f  = MIDI_MIN_VALUE;
        pots[i].midi_last_sent = MIDI_UNINITIALIZED_VALUE;
    }
}

// Single ADC read on given channel (0..3)
static uint16_t adc_read_once(uint8_t channel_index)
{
    adc_select_input(channel_index);
    (void)adc_read();              // dummy read
    sleep_us(ADC_SETTLE_DELAY_US);
    return adc_read();
}

// Oversampled average
static float adc_read_oversampled(uint8_t channel_index, uint8_t sample_count)
{
    uint32_t accumulator = 0;

    for (uint8_t i = 0; i < sample_count; ++i)
    {
        accumulator += adc_read_once(channel_index);
    }

    float avg = (float)accumulator / (float)sample_count;
    if (avg < 0.0f) avg = 0.0f;
    if (avg > (float)ADC_MAX_VALUE) avg = (float)ADC_MAX_VALUE;
    return avg;
}

// Map ADC raw -> virtual range -> clamp to MIDI_MIN..MIDI_MAX
static float adc_to_virtual_midi(float raw_adc)
{
    const float adc_max_f    = (float)ADC_MAX_VALUE;
    const float virtual_span = (POT_VIRTUAL_MAX_VALUE - POT_VIRTUAL_MIN_VALUE);

    float t = raw_adc / adc_max_f; // 0..1
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float virtual_val = POT_VIRTUAL_MIN_VALUE + t * virtual_span;

    if (virtual_val < MIDI_MIN_VALUE)  virtual_val = MIDI_MIN_VALUE;
    if (virtual_val > MIDI_MAX_VALUE)  virtual_val = MIDI_MAX_VALUE;

    return virtual_val;
}

// Periodically update pots' stable MIDI values
static void pots_update_task(void)
{
    static uint32_t last_update_ms = 0;
    const uint32_t interval_ms = POT_SCAN_INTERVAL_MS;

    uint32_t now = board_millis();
    if ((now - last_update_ms) < interval_ms)
        return;

    last_update_ms = now;

    // All 4 channels on ADC0..3
    float raw[] = {
        adc_read_oversampled(0, POT_OVERSAMPLE_COUNT),
        adc_read_oversampled(1, POT_OVERSAMPLE_COUNT),
        adc_read_oversampled(2, POT_OVERSAMPLE_COUNT),
        adc_read_oversampled(3, POT_OVERSAMPLE_COUNT)
    };

    for (int i = 0; i < POT_COUNT; ++i)
    {
        float new_midi = adc_to_virtual_midi(raw[i]);
        float diff     = new_midi - pots[i].midi_stable_f;
        if (diff < 0.0f) diff = -diff;

        if (diff > POT_CHANGE_THRESHOLD_STEPS)
        {
            pots[i].midi_stable_f = new_midi;
        }
    }
}

// =====================================================================
// MIDI
// =====================================================================

static void send_midi_cc(uint8_t cc_number, uint8_t value)
{
    if (!tud_midi_mounted())
        return;

    uint8_t packet[4];

    // USB-MIDI event for Control Change: CIN=0xB
    const uint8_t cin_control_change = 0x0B;

    packet[0] = (uint8_t)((MIDI_CABLE_NUMBER << 4) | cin_control_change);
    packet[1] = (uint8_t)(0xB0 | (MIDI_CHANNEL_INDEX & 0x0F));
    packet[2] = cc_number;
    packet[3] = value;

    tud_midi_stream_write(MIDI_CABLE_NUMBER, packet, sizeof packet);
}

// Send CC messages for pots whose *stable* value changed
static void pots_send_midi_task(void)
{
    for (int i = 0; i < POT_COUNT; ++i)
    {
        // Round stable float to int MIDI value
        uint8_t rounded_midi = (uint8_t)(pots[i].midi_stable_f + 0.5f);

        if (rounded_midi != pots[i].midi_last_sent)
        {
            pots[i].midi_last_sent = rounded_midi;
            send_midi_cc(POT_MIDI_CC[i], rounded_midi);
        }
    }
}

// =====================================================================
// HID GAMEPAD
// =====================================================================

static void send_gamepad_report(void)
{
    if (!tud_hid_ready())
        return;

    uint8_t buttons_bitfield = read_buttons_bitfield();

    const uint8_t up    = (buttons_bitfield & (1u << BTN_BIT_UP))     ? 1u : 0u;
    const uint8_t down  = (buttons_bitfield & (1u << BTN_BIT_DOWN))   ? 1u : 0u;
    const uint8_t left  = (buttons_bitfield & (1u << BTN_BIT_LEFT))   ? 1u : 0u;
    const uint8_t right = (buttons_bitfield & (1u << BTN_BIT_RIGHT))  ? 1u : 0u;
    const uint8_t btn_a = (buttons_bitfield & (1u << BTN_BIT_A))      ? 1u : 0u;
    const uint8_t btn_b = (buttons_bitfield & (1u << BTN_BIT_B))      ? 1u : 0u;
    const uint8_t start = (buttons_bitfield & (1u << BTN_BIT_START))  ? 1u : 0u;
    const uint8_t select= (buttons_bitfield & (1u << BTN_BIT_SELECT)) ? 1u : 0u;

    hid_gamepad_report_t report;
    memset(&report, 0, sizeof(report));

    // D-pad mapped to analog axes
    if (left)  report.x = GAMEPAD_AXIS_MIN;
    if (right) report.x = GAMEPAD_AXIS_MAX;
    if (up)    report.y = GAMEPAD_AXIS_MIN;
    if (down)  report.y = GAMEPAD_AXIS_MAX;

    uint16_t hid_buttons = 0;
    if (btn_a)     hid_buttons |= (1u << HID_BTN_INDEX_A);
    if (btn_b)     hid_buttons |= (1u << HID_BTN_INDEX_B);
    if (start)     hid_buttons |= (1u << HID_BTN_INDEX_START);
    if (select)    hid_buttons |= (1u << HID_BTN_INDEX_SELECT);

    report.buttons = hid_buttons;

    tud_hid_report(HID_REPORT_ID_GAMEPAD, &report, sizeof(report));
}

// =====================================================================
// TinyUSB callbacks (minimal stubs)
// =====================================================================

void tud_mount_cb(void)           {}
void tud_umount_cb(void)          {}
void tud_suspend_cb(bool rw)      {(void)rw;}
void tud_resume_cb(void)          {}

void tud_hid_report_complete_cb(uint8_t instance,
                                uint8_t const *report,
                                uint16_t len)
{
    (void) instance;
    (void) report;
    (void) len;
}

uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

// =====================================================================
// MAIN
// =====================================================================

int main(void)
{
    board_init();
    tusb_init();

    init_buttons();
    init_pots();

    while (true)
    {
        tud_task();              // TinyUSB device task

        send_gamepad_report();   // Gamepad HID
        pots_update_task();      // ADC + stable filtering
        pots_send_midi_task();   // MIDI CC on change
    }

    return 0;
}
