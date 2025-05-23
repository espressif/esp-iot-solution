/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tusb.h"
#include "tusb_config.h"
#include "uac_config.h"
#include "usb_descriptors.h"
#include "uac_descriptors.h"
#include "sdkconfig.h"

/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]  VIDEO | AUDIO | MIDI | HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#ifndef USB_PID
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
    _PID_MAP(MIDI, 3) | _PID_MAP(AUDIO, 4) | _PID_MAP(VIDEO, 5) | _PID_MAP(VENDOR, 6) )
#endif

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for Video
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_UNSPECIFIED,
    .bDeviceSubClass    = TUSB_CLASS_UNSPECIFIED,
    .bDeviceProtocol    = TUSB_CLASS_UNSPECIFIED,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0101,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

#if CFG_TUD_HID
//--------------------------------------------------------------------+
// HID Report Descriptor
//--------------------------------------------------------------------+
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_TOUCH_SCREEN(REPORT_ID_TOUCH, CONFIG_USB_EXTEND_SCREEN_HEIGHT, CONFIG_USB_EXTEND_SCREEN_WIDTH),
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void) instance;
    return desc_hid_report;
}
#endif

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN * CFG_TUD_HID + \
                             TUD_VENDOR_DESC_LEN * CFG_TUD_VENDOR + \
                             TUD_AUDIO_DEVICE_DESC_LEN * CFG_TUD_AUDIO)

uint8_t const desc_fs_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    // Interface number, string index, EP Out & IN address, EP size
#if CFG_TUD_VENDOR
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 4, EPNUM_VENDOR, 0x80 | EPNUM_VENDOR, CFG_TUD_VENDOR_EPSIZE),
#endif
#if CFG_TUD_HID
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 5, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report), (0x80 | EPNUM_HID_DATA), CFG_TUD_HID_EP_BUFSIZE, 10),
#endif
#if CFG_TUD_AUDIO
    TUD_AUDIO_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 6, EPNUM_AUDIO_OUT, (0x80 | EPNUM_AUDIO_IN), (0x80 | EPNUM_AUDIO_FB)),
#endif
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index; // for multiple configurations
    return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

#define _STRINGIFY(x)   #x
#define STRINGIFY(s)    _STRINGIFY(s)

#define VENDOR_STR \
        CONFIG_IDF_TARGET \
        "udisp0_" \
        "R" \
        STRINGIFY(CONFIG_USB_EXTEND_SCREEN_HEIGHT) \
        "x" \
        STRINGIFY(CONFIG_USB_EXTEND_SCREEN_WIDTH) \
        "_" \
        "Ejpg" \
        STRINGIFY(CONFIG_USB_EXYEEND_SCREEN_JEPG_QUALITY) \
        "_Fps" \
        STRINGIFY(CONFIG_USB_EXTEND_SCREEN_MAX_FPS) \
        "_Bl" \
        STRINGIFY(CONFIG_USB_EXTEND_SCREEN_FRAME_LIMIT_B)

// array of pointer to string descriptors
char const *string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 },    // 0: is supported language is English (0x0409)
    USB_MANUFACTURER,                 // 1: Manufacturer
    "ESP_Extern_Screen",              // 2: Product
    "012-2021",                       // 3: Serials, should use chip ID
    VENDOR_STR,                       // 4: Vendor Interface
#if CFG_TUD_HID
    "touch",                          // 5: HID Interface
#endif
#if CFG_TUD_AUDIO
    "esp uac",                        // 6: UAC Interface
    "speaker",                        // 7: UAC Interface
    "mic",                            // 8: UAC Interface
#endif
};

static uint16_t _desc_str[64];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;

    uint8_t chr_count;

    if (index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) {
            return NULL;
        }

        const char *str = (char *)string_desc_arr[index];

        // Cap at max char
        chr_count = (uint8_t) strlen(str);
        if (chr_count > 63) {
            chr_count = 63;
        }

        // Convert ASCII string into UTF-16
        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}
