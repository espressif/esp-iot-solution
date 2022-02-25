// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "esp_log.h"
#include "descriptors_control.h"
#include "dfu_device.h"

static const char *TAG = "tusb_desc";
static tusb_desc_device_t s_descriptor;
static char *s_str_descriptor[USB_STRING_DESCRIPTOR_ARRAY_SIZE];
static uint8_t *s_config_descriptor = NULL;
#define MAX_DESC_BUF_SIZE 32

#if CFG_TUD_HID //HID Report Descriptor
uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE))
};
#endif

#define FUNC_ATTRS (DFU_ATTR_CAN_UPLOAD | DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

uint8_t const desc_configuration[] = {
#if CONFIG_TINYUSB_NET_ECM
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, ALT_CONFIG_TOTAL_LEN, 0, 100),
#else
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, 0, 100),
#endif
#if CFG_TUD_BTH
    // BT Primary controller descriptor
    // Interface number, string index, attributes, event endpoint, event endpoint size, interval, data in, data out, data endpoint size, iso endpoint sizes
    TUD_BTH_DESCRIPTOR(ITF_NUM_BTH, 0 /* STRID_BTH_INTERFACE */, (0x80 | EPNUM_BT_EVT), 16, 1, (0x80 | EPNUM_BT_BULK_OUT), EPNUM_BT_BULK_OUT, 64, 0, 9, 17, 25, 33, 49),
#endif

#if CFG_TUD_CDC
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC_INTERFACE, (0x80 | EPNUM_CDC_NOTIF), 8, EPNUM_CDC_DATA, (0x80 | EPNUM_CDC_DATA), 64),
#endif

#if CFG_TUD_NET
#if CONFIG_TINYUSB_NET_ECM
    // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
    TUD_CDC_ECM_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, STRID_MAC, (0x80 | EPNUM_NET_NOTIF), 64, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU),
#elif CONFIG_TINYUSB_NET_RNDIS
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_RNDIS_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, (0x80 | EPNUM_NET_NOTIF), 8, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), CFG_TUD_NET_ENDPOINT_SIZE),
#endif
#endif

#if CFG_TUD_VENDOR
    // Interface number, string index, EP Out & IN address, EP size
    TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, STRID_WEBUSB_INTERFACE, EPNUM_VENDOR, 0x80 | EPNUM_VENDOR, 64),
#endif

#if CFG_TUD_MSC
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, STRID_MSC_INTERFACE, EPNUM_MSC_DATA, (0x80 | EPNUM_MSC_DATA), 64), // highspeed 512
#endif
#if CFG_TUD_HID
    // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, STRID_HID_INTERFACE, HID_PROTOCOL_NONE, sizeof(desc_hid_report), (0x80 | EPNUM_HID_DATA), 16, 10)
#endif

#if CFG_TUD_DFU
    // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, DFU_ALT_COUNT, STRID_DFU_INTERFACE, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
#endif

};

// =============================================================================
// CALLBACKS
// =============================================================================

/**
 * @brief Invoked when received GET DEVICE DESCRIPTOR.
 * Application returns pointer to descriptor
 *
 * @return uint8_t const*
 */
uint8_t const *tud_descriptor_device_cb(void)
{
    return (uint8_t const *)&s_descriptor;
}

/**
 * @brief Invoked when received GET CONFIGURATION DESCRIPTOR.
 * Descriptor contents must exist long enough for transfer to complete
 *
 * @param index
 * @return uint8_t const* Application return pointer to descriptor
 */
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index; // for multiple configurations
    return s_config_descriptor;
}

static uint16_t _desc_str[MAX_DESC_BUF_SIZE];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;

    uint8_t chr_count;

    if ( index == 0) {
        memcpy(&_desc_str[1], s_str_descriptor[0], 2);
        chr_count = 1;
    }  
#if CONFIG_TINYUSB_NET_ECM
    else if (STRID_MAC == index)
    {
        // Convert MAC address into UTF-16

        for (unsigned i=0; i<sizeof(tud_network_mac_address); i++)
        {
        _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 4) & 0xf];
        _desc_str[1+chr_count++] = "0123456789ABCDEF"[(tud_network_mac_address[i] >> 0) & 0xf];
        }
    } 
#endif
    else 
    {
        // Convert ASCII string into UTF-16

        if ( index >= sizeof(s_str_descriptor) / sizeof(s_str_descriptor[0]) ) {
            return NULL;
        }

        const char *str = s_str_descriptor[index];

        // Cap at max char
        chr_count = strlen(str);
        if ( chr_count > MAX_DESC_BUF_SIZE - 1 ) {
            chr_count = MAX_DESC_BUF_SIZE - 1;
        }

        for (uint8_t i = 0; i < chr_count; i++) {
            _desc_str[1 + i] = str[i];
        }
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2 * chr_count + 2);

    return _desc_str;
}

/**
 * @brief Invoked when received GET HID REPORT DESCRIPTOR
 * Application returns pointer to descriptor. Descriptor contents must exist
 * long enough for transfer to complete
 *
 * @return uint8_t const*
 */
#if CFG_TUD_HID
uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
    (void)itf;
    return desc_hid_report;
}
#endif

// =============================================================================
// Driver functions
// =============================================================================

void tusb_set_descriptor(tusb_desc_device_t *dev_desc, const char **str_desc)
{
    ESP_LOGI(TAG, "\n"
             "┌─────────────────────────────────┐\n"
             "│  USB Device Descriptor Summary  │\n"
             "├───────────────────┬─────────────┤\n"
             "│bDeviceClass       │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│bDeviceSubClass    │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│bDeviceProtocol    │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│bMaxPacketSize0    │ %-4u        │\n"
             "├───────────────────┼─────────────┤\n"
             "│idVendor           │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│idProduct          │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│bcdDevice          │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│iManufacturer      │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│iProduct           │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│iSerialNumber      │ %-#10x  │\n"
             "├───────────────────┼─────────────┤\n"
             "│bNumConfigurations │ %-#10x  │\n"
             "└───────────────────┴─────────────┘",
             dev_desc->bDeviceClass, dev_desc->bDeviceSubClass,
             dev_desc->bDeviceProtocol, dev_desc->bMaxPacketSize0,
             dev_desc->idVendor, dev_desc->idProduct, dev_desc->bcdDevice,
             dev_desc->iManufacturer, dev_desc->iProduct, dev_desc->iSerialNumber,
             dev_desc->bNumConfigurations);
    s_descriptor = *dev_desc;

    if (str_desc != NULL) {
        memcpy(s_str_descriptor, str_desc,
               sizeof(s_str_descriptor[0])*USB_STRING_DESCRIPTOR_ARRAY_SIZE);
    }
    tusb_desc_set = true;
}

void tusb_set_config_descriptor(const uint8_t *config_desc)
{
    size_t length = 0;
    const uint8_t *config_descriptor = NULL; 
    if (config_desc == NULL) {
        config_descriptor = desc_configuration;
        ESP_LOGI(TAG, "using default config desc");
    } else {
        config_descriptor = config_desc;
        ESP_LOGI(TAG, "using custom config desc");
    }
    length = (config_descriptor[3]<<8) + config_descriptor[2];
    ESP_LOGI(TAG, "config desc size=%d", length);
    s_config_descriptor = realloc(s_config_descriptor, length);
    memcpy(s_config_descriptor, config_descriptor, length);
}

tusb_desc_device_t *tusb_get_active_desc(void)
{
    return &s_descriptor;
}

char **tusb_get_active_str_desc(void)
{
    return s_str_descriptor;
}

void tusb_clear_descriptor(void)
{
    memset(&s_descriptor, 0, sizeof(s_descriptor));
    memset(&s_str_descriptor, 0, sizeof(s_str_descriptor));
    free(s_config_descriptor);
    s_config_descriptor = NULL;
    tusb_desc_set = false;
}
