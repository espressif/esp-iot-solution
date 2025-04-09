
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

/*
*** Device descriptor ***
bLength 18
bDescriptorType 1
bcdUSB 2.00
bDeviceClass 0x2
bDeviceSubClass 0x0
bDeviceProtocol 0x0
bMaxPacketSize0 64
idVendor 0x483
idProduct 0x5740
bcdDevice 2.00
iManufacturer 1
iProduct 2
iSerialNumber 3
bNumConfigurations 1
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 67
bNumInterfaces 2
bConfigurationValue 1
iConfiguration 0
bmAttributes 0x80
bMaxPower 100mA
    *** Interface descriptor ***
    bLength 9
    bDescriptorType 4
    bInterfaceNumber 0
    bAlternateSetting 0
    bNumEndpoints 1
    bInterfaceClass 0x2
    bInterfaceSubClass 0x2
    bInterfaceProtocol 0x1
    iInterface 0
    *** CDC Header Descriptor ***
    bcdCDC: 1.10
    *** CDC Call Descriptor ***
    bmCapabilities: 0x00
    bDataInterface: 1
    *** CDC ACM Descriptor ***
    bmCapabilities: 0x00
    *** CDC Union Descriptor ***
    bControlInterface: 0
    bSubordinateInterface[0]: 1
        *** Endpoint descriptor ***
        bLength 7
        bDescriptorType 5
        bEndpointAddress 0x83   EP 3 IN
        bmAttributes 0x3    INT
        wMaxPacketSize 16
        bInterval 255
    *** Interface descriptor ***
    bLength 9
    bDescriptorType 4
    bInterfaceNumber 1
    bAlternateSetting 0
    bNumEndpoints 2
    bInterfaceClass 0xa
    bInterfaceSubClass 0x0
    bInterfaceProtocol 0x0
    iInterface 0
        *** Endpoint descriptor ***
        bLength 7
        bDescriptorType 5
        bEndpointAddress 0x1    EP 1 OUT
        bmAttributes 0x2    BULK
        wMaxPacketSize 64
        bInterval 1
        *** Endpoint descriptor ***
        bLength 7
        bDescriptorType 5
        bEndpointAddress 0x82   EP 2 IN
        bmAttributes 0x2    BULK
        wMaxPacketSize 64
        bInterval 1
*/

namespace stm32_device {
const uint8_t dev_desc[] = {
    0x12, 0x01, 0x00, 0x02, 0x02, 0x00, 0x00, 0x40, 0x83, 0x04, 0x40, 0x57, 0x00, 0x02, 0x01, 0x02, 0x03, 0x01
};
const uint8_t cfg_desc[] = {
    0x09, 0x02, 0x43, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x04,

    0x05, 0x24, 0x00, 0x10, 0x01, // CDC Header
    0x05, 0x24, 0x01, 0x00, 0x01, // CDC Call
    0x04, 0x24, 0x02, 0x00,       // CDC ACM
    0x05, 0x24, 0x06, 0x00, 0x01, // CDC Union

    0x07, 0x05, 0x83, 0x03, 0x10, 0x00, 0xFF,
    0x09, 0x04, 0x01, 0x00, 0x02,
    0x0A, 0x00, 0x00, 0x00, 0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x01, 0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x01
};
}
