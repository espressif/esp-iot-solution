/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usb_descriptors.h"
#include "sdkconfig.h"
#include "tinyusb_types.h"

/*
 * A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_TUSB_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
    _PID_MAP(MIDI, 3) ) //| _PID_MAP(AUDIO, 4) | _PID_MAP(VENDOR, 5) )

/**** Kconfig driven Descriptor ****/

//------------- Device Descriptor -------------//
const tusb_desc_device_t descriptor_dev_default = {
    .bLength = sizeof(descriptor_dev_default),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

#if CFG_TUD_CDC
    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
#else
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
#endif

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

#if CONFIG_TINYUSB_DESC_USE_ESPRESSIF_VID
    .idVendor = USB_ESPRESSIF_VID,
#else
    .idVendor = CONFIG_TINYUSB_DESC_CUSTOM_VID,
#endif

#if CONFIG_TINYUSB_DESC_USE_DEFAULT_PID
    .idProduct = USB_TUSB_PID,
#else
    .idProduct = CONFIG_TINYUSB_DESC_CUSTOM_PID,
#endif

    .bcdDevice = CONFIG_TINYUSB_DESC_BCD_DEVICE,

    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,

    .bNumConfigurations = 0x01
};

#if (TUD_OPT_HIGH_SPEED)
const tusb_desc_device_qualifier_t descriptor_qualifier_default = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = 0x0200,

#if CFG_TUD_CDC
    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and protocol must be IAD (1)
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
#else
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
#endif

    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0
};
#endif // TUD_OPT_HIGH_SPEED

//------------- Array of String Descriptors -------------//
const char *descriptor_str_default[] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},                // 0: is supported language is English (0x0409)
    CONFIG_TINYUSB_DESC_MANUFACTURER_STRING, // 1: Manufacturer
    CONFIG_TINYUSB_DESC_PRODUCT_STRING,      // 2: Product
    CONFIG_TINYUSB_DESC_SERIAL_STRING,       // 3: Serials, should use chip ID

#if CONFIG_TINYUSB_CDC_ENABLED
    CONFIG_TINYUSB_DESC_CDC_STRING,          // 4: CDC Interface
#endif

#if CONFIG_TINYUSB_BTH_ENABLED
    CONFIG_TINYUSB_DESC_BTH_STRING,          // 6: BTH Interface
#endif

#if CONFIG_TINYUSB_DFU_ENABLED
    "FLASH",                       // 4: DFU Partition 1
    "EEPROM",                      // 5: DFU Partition 2
#endif

#if CFG_TUD_NCM || CFG_TUD_ECM_RNDIS
    "USB net",                               // 8. NET Interface
    "",                                      // 9. MAC
#endif

    NULL                                     // NULL: Must be last. Indicates end of array
};

//------------- Interfaces enumeration -------------//
enum {

#if CFG_TUD_NCM || CFG_TUD_ECM_RNDIS
    ITF_NUM_NET = 0,
    ITF_NUM_NET_DATA,
#endif

#if CFG_TUD_CDC
    ITF_NUM_CDC,
    ITF_NUM_CDC_DATA,
#endif

#if CFG_TUD_CDC > 1
    ITF_NUM_CDC1,
    ITF_NUM_CDC1_DATA,
#endif

#if CFG_TUD_BTH
    ITF_NUM_BTH,
#endif

#if CFG_TUD_DFU
    ITF_NUM_DFU,
#endif

    ITF_NUM_TOTAL
};

#define DFU_ALT_COUNT   2
#define FUNC_ATTRS (DFU_ATTR_CAN_UPLOAD | DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

enum {
    TUSB_DESC_TOTAL_LEN = TUD_CONFIG_DESC_LEN +
                          CFG_TUD_CDC * TUD_CDC_DESC_LEN +
                          CFG_TUD_NCM * TUD_CDC_NCM_DESC_LEN +
                          CFG_TUD_BTH * TUD_BTH_DESC_LEN +
                          CFG_TUD_DFU * TUD_DFU_DESC_LEN(DFU_ALT_COUNT) +
                          CONFIG_TINYUSB_NET_MODE_ECM * TUD_CDC_ECM_DESC_LEN +
                          CONFIG_TINYUSB_NET_MODE_RNDIS * TUD_RNDIS_DESC_LEN
};

//------------- USB Endpoint numbers -------------//
enum {
    // Available USB Endpoints: 5 IN/OUT EPs and 1 IN EP
    EP_EMPTY = 0,

#if CFG_TUD_NCM || CFG_TUD_ECM_RNDIS
    EPNUM_NET_NOTIF,
    EPNUM_NET_DATA,
#endif

#if CFG_TUD_CDC
    EPNUM_0_CDC_NOTIF,
    EPNUM_0_CDC,
#endif

#if CFG_TUD_CDC > 1
    EPNUM_1_CDC_NOTIF,
    EPNUM_1_CDC,
#endif

#if CFG_TUD_BTH
    EPNUM_BT_EVT,
    EPNUM_BT_BULK_OUT,
#endif

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

#if CFG_TUD_BTH
    STRID_BTH_INTERFACE,
#endif

#if CFG_TUD_DFU
    STRID_DFU_INTERFACE,
#endif

#if CFG_TUD_NCM || CFG_TUD_ECM_RNDIS
    STRID_NET_INTERFACE,
    STRID_MAC,
#endif

};

//------------- Configuration Descriptor -------------//
uint8_t const descriptor_fs_cfg_default[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

#if CFG_TUD_NCM
    // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, STRID_MAC, (0x80 | EPNUM_NET_NOTIF), 64, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), 64, CFG_TUD_NET_MTU),
#endif

#if CFG_TUD_ECM_RNDIS
#if CONFIG_TINYUSB_NET_MODE_RNDIS
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_RNDIS_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, (0x80 | EPNUM_NET_NOTIF), 8, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), 64),
#elif CONFIG_TINYUSB_NET_MODE_ECM
    // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
    TUD_CDC_ECM_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, STRID_MAC, (0x80 | EPNUM_NET_NOTIF), 64, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), 64, CFG_TUD_NET_MTU),
#endif
#endif

#if CFG_TUD_CDC
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC_INTERFACE, 0x80 | EPNUM_0_CDC_NOTIF, 8, EPNUM_0_CDC, 0x80 | EPNUM_0_CDC, 64),
#endif

#if CFG_TUD_CDC > 1
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC1, STRID_CDC_INTERFACE, 0x80 | EPNUM_1_CDC_NOTIF, 8, EPNUM_1_CDC, 0x80 | EPNUM_1_CDC, 64),
#endif

#if CFG_TUD_BTH
    // BT Primary controller descriptor
    // Interface number, string index, attributes, event endpoint, event endpoint size, interval, data in, data out, data endpoint size, iso endpoint sizes
    TUD_BTH_DESCRIPTOR(ITF_NUM_BTH, STRID_BTH_INTERFACE, (0x80 | EPNUM_BT_EVT), 16, 1, (0x80 | EPNUM_BT_BULK_OUT), EPNUM_BT_BULK_OUT, 64, 64, 64),
#endif

#if CFG_TUD_DFU
    // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, DFU_ALT_COUNT, STRID_DFU_INTERFACE, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
#endif

};

#if (TUD_OPT_HIGH_SPEED)
uint8_t const descriptor_hs_cfg_default[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

#if CFG_TUD_NCM
    // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
    TUD_CDC_NCM_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, STRID_MAC, (0x80 | EPNUM_NET_NOTIF), 512, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), 512, CFG_TUD_NET_MTU),
#endif

#if CFG_TUD_ECM_RNDIS
#if CONFIG_TINYUSB_NET_MODE_RNDIS
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_RNDIS_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, (0x80 | EPNUM_NET_NOTIF), 8, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), 512),
#elif CONFIG_TINYUSB_NET_MODE_ECM
    // Interface number, description string index, MAC address string index, EP notification address and size, EP data address (out, in), and size, max segment size.
    TUD_CDC_ECM_DESCRIPTOR(ITF_NUM_NET, STRID_NET_INTERFACE, STRID_MAC, (0x80 | EPNUM_NET_NOTIF), 512, EPNUM_NET_DATA, (0x80 | EPNUM_NET_DATA), 512, CFG_TUD_NET_MTU),
#endif
#endif

#if CFG_TUD_CDC
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, STRID_CDC_INTERFACE, 0x80 | EPNUM_0_CDC_NOTIF, 8, EPNUM_0_CDC, 0x80 | EPNUM_0_CDC, 512),
#endif

#if CFG_TUD_CDC > 1
    // Interface number, string index, EP notification address and size, EP data address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC1, STRID_CDC_INTERFACE, 0x80 | EPNUM_1_CDC_NOTIF, 8, EPNUM_1_CDC, 0x80 | EPNUM_1_CDC, 512),
#endif

#if CFG_TUD_BTH
    // BT Primary controller descriptor
    // Interface number, string index, attributes, event endpoint, event endpoint size, interval, data in, data out, data endpoint size, iso endpoint sizes
    TUD_BTH_DESCRIPTOR(ITF_NUM_BTH, STRID_BTH_INTERFACE, (0x80 | EPNUM_BT_EVT), 16, 1, (0x80 | EPNUM_BT_BULK_OUT), EPNUM_BT_BULK_OUT, 512, 1024, 1024),
#endif

#if CFG_TUD_DFU
    // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, DFU_ALT_COUNT, STRID_DFU_INTERFACE, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
#endif

};
#endif // TUD_OPT_HIGH_SPEED

#if CFG_TUD_NCM || CFG_TUD_ECM_RNDIS
uint8_t tusb_get_mac_string_id(void)
{
    return STRID_MAC;
}
#endif

/* End of Kconfig driven Descriptor */
