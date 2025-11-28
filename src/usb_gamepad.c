#include "usb_gamepad.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include <string.h>

#define GAMEPAD_REPORT_ID  1

#define GAMEPAD_AXIS_MIN   (-127)
#define GAMEPAD_AXIS_MAX   (127)

void usb_gamepad_init(void)
{
    // Placeholder for initialization (Decorator)
    // TinyUSB is initialized in main.c
}

void usb_gamepad_update(const controller_state_t *st)
{
    if (!tud_hid_ready()) return;

    hid_gamepad_report_t report;
    memset(&report, 0, sizeof(report));

    report.x  = 0;
    report.y  = 0;
    report.z  = 0;
    report.rz = 0;
    report.rx = 0;
    report.ry = 0;

    // We don't use hat in POC-style mapping
    report.hat = 0x0F;   // "no direction" / null

    uint8_t b = st->buttons;

    // Logical bits in st->buttons:
    //  0: UP
    //  1: DOWN
    //  2: LEFT
    //  3: RIGHT
    //  4: A
    //  5: B
    //  6: START
    //  7: SELECT

    const bool up     = b & (1u << 0);
    const bool down   = b & (1u << 1);
    const bool left   = b & (1u << 2);
    const bool right  = b & (1u << 3);
    const bool btn_a  = b & (1u << 4);
    const bool btn_b  = b & (1u << 5);
    const bool start  = b & (1u << 6);
    const bool select = b & (1u << 7);

    // -------------------------------
    // D-PAD AS AXES  (POC behavior)
    // -------------------------------
    if (left)  report.x = GAMEPAD_AXIS_MIN;
    if (right) report.x = GAMEPAD_AXIS_MAX;
    if (up)    report.y = GAMEPAD_AXIS_MIN;
    if (down)  report.y = GAMEPAD_AXIS_MAX;

    // -------------------------------
    // Buttons: keep the mapping you liked:
    //   A      -> HID Button 1
    //   B      -> HID Button 2
    //   SELECT -> HID Button 3
    //   START  -> HID Button 4
    // -------------------------------

    uint16_t hid_buttons = 0;

    if (btn_a)     hid_buttons |= (1u << 0);  // Button 1
    if (btn_b)     hid_buttons |= (1u << 1);  // Button 2
    if (select)    hid_buttons |= (1u << 2);  // Button 3
    if (start)     hid_buttons |= (1u << 3);  // Button 4

    report.buttons = hid_buttons;

    tud_hid_report(GAMEPAD_REPORT_ID, &report, sizeof(report));
}

// TinyUSB HID callbacks

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t* buffer, uint16_t reqlen)
{
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer, uint16_t bufsize)
{
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}
