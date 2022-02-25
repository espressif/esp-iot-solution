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

#pragma once

#include <string.h>
#include "usb_descriptors.h"


/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]       NET | VENDOR | MIDI | HID | MSC | CDC          [LSB]
 */

#ifdef __cplusplus
extern "C" {
#endif

//------------- EndPoint Descriptor -------------//
enum {
    EPNUM_DEFAULT = 0,
#   if CFG_TUD_BTH
    EPNUM_BT_EVT,
    EPNUM_BT_BULK_OUT,
#   endif

#   if CFG_TUD_CDC
    EPNUM_CDC_NOTIF,
    EPNUM_CDC_DATA,
#   endif

#   if CFG_TUD_VENDOR
    EPNUM_VENDOR,
#   endif

#   if CFG_TUD_NET
    EPNUM_NET_NOTIF,
    EPNUM_NET_DATA,
#   endif

#   if CFG_TUD_MSC
    EPNUM_MSC_DATA,
#   endif

#   if CFG_TUD_HID
    EPNUM_HID_DATA,
#   endif
};

#if ((CFG_TUD_BTH * 2) + (CFG_TUD_NET * 2) + (CFG_TUD_CDC * 2) + CFG_TUD_MSC + CFG_TUD_HID) > 4
#error "USB endpoint number not be more than 5"
#endif

//------------- HID Report Descriptor -------------//
#if CFG_TUD_HID
enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE
};
#endif

//------------- Configuration Descriptor -------------//
enum {
#   if CFG_TUD_BTH
    ITF_NUM_BTH = 0,
    ITF_NUM_BTH_DATA,
#   endif

#   if CFG_TUD_NET
    ITF_NUM_NET,
    ITF_NUM_NET_DATA,
#   endif

#   if CFG_TUD_CDC
    ITF_NUM_CDC,
    ITF_NUM_CDC_DATA,
#   endif

#   if CFG_TUD_VENDOR
    ITF_NUM_VENDOR,
#   endif

#   if CFG_TUD_MSC
    ITF_NUM_MSC,
#   endif

#   if CFG_TUD_HID
    ITF_NUM_HID,
#   endif

#   if CFG_TUD_DFU
    ITF_NUM_DFU,
#   endif
    ITF_NUM_TOTAL
};

//------------- STRID -------------//
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
#if CFG_TUD_CDC
    STRID_CDC_INTERFACE,
#endif
#if CFG_TUD_NET
    STRID_NET_INTERFACE,
    STRID_MAC,
#endif
#if CFG_TUD_VENDOR
    STRID_WEBUSB_INTERFACE,
#endif
#if CFG_TUD_MSC
    STRID_MSC_INTERFACE,
#endif
#if CFG_TUD_HID
    STRID_HID_INTERFACE,
#endif
#if CFG_TUD_BTH
    STRID_BTH_INTERFACE,
#endif
#if CFG_TUD_DFU
    STRID_DFU_INTERFACE,
#endif
};

#define DFU_ALT_COUNT   2
#define TUD_DFU_DESC_LEN(_alt_count)    (9 + (_alt_count) * 9)

enum {
    TUSB_DESC_TOTAL_LEN = TUD_CONFIG_DESC_LEN + 
                          TUD_CDC_DESC_LEN * CFG_TUD_CDC + 
                          TUD_RNDIS_DESC_LEN * CFG_TUD_NET +
                          TUD_VENDOR_DESC_LEN * CFG_TUD_VENDOR + 
                          TUD_MSC_DESC_LEN * CFG_TUD_MSC + 
                          TUD_HID_DESC_LEN * CFG_TUD_HID +
                          TUD_BTH_DESC_LEN * CFG_TUD_BTH +
                          TUD_DFU_DESC_LEN(DFU_ALT_COUNT) * CFG_TUD_DFU,

    ALT_CONFIG_TOTAL_LEN = TUD_CONFIG_DESC_LEN + 
                           TUD_CDC_ECM_DESC_LEN * CFG_TUD_NET + 
                           TUD_CDC_DESC_LEN * CFG_TUD_CDC +
                           TUD_VENDOR_DESC_LEN * CFG_TUD_VENDOR +
                           TUD_MSC_DESC_LEN * CFG_TUD_MSC + 
                           TUD_HID_DESC_LEN * CFG_TUD_HID +
                           TUD_BTH_DESC_LEN * CFG_TUD_BTH +
                           TUD_DFU_DESC_LEN(DFU_ALT_COUNT) * CFG_TUD_DFU
};

bool tusb_desc_set;
void tusb_set_descriptor(tusb_desc_device_t *desc, const char **str_desc);
void tusb_set_config_descriptor(const uint8_t *config_desc);
tusb_desc_device_t *tusb_get_active_desc(void);
char **tusb_get_active_str_desc(void);
void tusb_clear_descriptor(void);

#ifdef __cplusplus
}
#endif
