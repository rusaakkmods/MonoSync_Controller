#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// Device descriptor
//--------------------------------------------------------------------+

tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    .bDeviceClass       = 0x00,  // Each interface specifies its own class
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = STRID_MANUFACTURER,
    .iProduct           = STRID_PRODUCT,
    .iSerialNumber      = STRID_SERIAL,

    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// HID report descriptor (Gamepad)
//--------------------------------------------------------------------+

// Use TinyUSB's built-in gamepad report descriptor.
uint8_t const desc_hid_report[] =
{
    TUD_HID_REPORT_DESC_GAMEPAD( HID_REPORT_ID(1) )
};

// Callback: return HID report descriptor
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    return desc_hid_report;
}

//--------------------------------------------------------------------+
// Configuration descriptor
//--------------------------------------------------------------------+

// Interface numbers
enum
{
    ITF_NUM_HID = 0,
    ITF_NUM_MIDI,
    ITF_NUM_MIDI_STREAMING,
    ITF_NUM_TOTAL
};

// Endpoint numbers
//  - 0x81: HID IN
//  - 0x02: MIDI OUT
//  - 0x83: MIDI IN
enum
{
    EPNUM_HID      = 0x81,
    EPNUM_MIDI_OUT = 0x02,
    EPNUM_MIDI_IN  = 0x83,
};

// Total length: config + HID + MIDI
#define CONFIG_TOTAL_LEN   (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_MIDI_DESC_LEN)

uint8_t const desc_fs_configuration[] =
{
    // Config number, interface count, string index, total length,
    // attributes, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // HID Gamepad interface
    // itfnum, stridx, protocol, report_desc_len, ep_in_iso_addr,
    // size, polling interval (ms)
    TUD_HID_DESCRIPTOR(ITF_NUM_HID,
                       STRID_HID,
                       HID_ITF_PROTOCOL_NONE,        // <-- fixed here
                       sizeof(desc_hid_report),
                       EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE,
                       1),

    // MIDI interface
    // itfnum, stridx, ep_out, ep_in, epsize
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI,
                        STRID_MIDI,
                        EPNUM_MIDI_OUT,
                        EPNUM_MIDI_IN,
                        64),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // Only one config
    return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String descriptors
//--------------------------------------------------------------------+

static uint16_t desc_str[32];

static char const *string_desc_arr[STRID_COUNT] =
{
    (const char[]){ 0x09, 0x04 },    // 0: LANGID (English)
    "rusaaKKMODS",                   // 1: Manufacturer
    "MonoSync Lite",                 // 2: Product
    "0001",                          // 3: Serial
    "MonoSync Gamepad",              // 4: HID interface
    "MonoSync Lite MIDI",            // 5: MIDI interface
};

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;

    uint8_t chr_count;

    if (index == STRID_LANGID)
    {
        desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2);
        desc_str[1] = 0x0409; // English
        return desc_str;
    }

    if (index >= STRID_COUNT)
        return NULL;

    const char *str = string_desc_arr[index];

    chr_count = (uint8_t) strlen(str);
    if (chr_count > 31) chr_count = 31;

    desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + 2 * chr_count);

    for (uint8_t i = 0; i < chr_count; i++)
    {
        desc_str[1 + i] = (uint8_t) str[i];
    }

    return desc_str;
}
