
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

/*
Device Descriptor:
  bLength                18
  bDescriptorType         1
  bcdUSB               2.00
  bDeviceClass            0 (Defined at Interface level)
  bDeviceSubClass         0
  bDeviceProtocol         0
  bMaxPacketSize0         8
  idVendor           0x04b4 Cypress Semiconductor Corp.
  idProduct          0x0003
  bcdDevice            0.00
  iManufacturer           1 Rainforest Automation, Inc.
  iProduct                2 RFA-Z105-2 HW2.7.3 EMU-2
  iSerial                 0
  bNumConfigurations      1
  Configuration Descriptor:
    bLength                 9
    bDescriptorType         2
    wTotalLength           84
    bNumInterfaces          3
    bConfigurationValue     1
    iConfiguration          0
    bmAttributes         0x80
      (Bus Powered)
    MaxPower              500mA
    Interface Association:
      bLength                 8
      bDescriptorType        11
      bFirstInterface         0
      bInterfaceCount         2
      bFunctionClass          2 Communications
      bFunctionSubClass       2 Abstract (modem)
      bFunctionProtocol       1 AT-commands (v.25ter)
      iFunction               0
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        0
      bAlternateSetting       0
      bNumEndpoints           1
      bInterfaceClass         2 Communications
      bInterfaceSubClass      2 Abstract (modem)
      bInterfaceProtocol      1 AT-commands (v.25ter)
      iInterface              0
      CDC Header:
        bcdCDC               1.10
      CDC ACM:
        bmCapabilities       0x02
          line coding and serial state
      CDC Union:
        bMasterInterface        0
        bSlaveInterface         1
      CDC Call Management:
        bmCapabilities       0x00
        bDataInterface          1
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x83  EP 3 IN
        bmAttributes            3
          Transfer Type            Interrupt
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval              10
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        1
      bAlternateSetting       0
      bNumEndpoints           2
      bInterfaceClass        10 CDC Data
      bInterfaceSubClass      0 Unused
      bInterfaceProtocol      0
      iInterface              0
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x01  EP 1 OUT
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval               0
      Endpoint Descriptor:
        bLength                 7
        bDescriptorType         5
        bEndpointAddress     0x82  EP 2 IN
        bmAttributes            2
          Transfer Type            Bulk
          Synch Type               None
          Usage Type               Data
        wMaxPacketSize     0x0040  1x 64 bytes
        bInterval               0
    Interface Descriptor:
      bLength                 9
      bDescriptorType         4
      bInterfaceNumber        2
      bAlternateSetting       0
      bNumEndpoints           0
      bInterfaceClass       255 Vendor Specific Class
      bInterfaceSubClass      5
      bInterfaceProtocol      0
      iInterface              0
Device Status:     0x0000
  (Bus Powered)
  */

namespace cypress_rfa {
const uint8_t dev_desc[] = {
    0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08, 0xb4, 0x04, 0x03, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x01
};
const uint8_t cfg_desc[] = {
    0x09, 0x02, 0x54, 0x00, 0x03, 0x01, 0x00, 0x80, 0x32, //CFG

    0x08, 0x0B, 0x00, 0x02, 0x02, 0x01, 0x00, 0x00, // IAD

    0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00, // INTF

    0x05, 0x24, 0x00, 0x10, 0x01, // CDC Header
    0x04, 0x24, 0x02, 0x02,       // CDC ACM
    0x05, 0x24, 0x06, 0x00, 0x01, // CDC Union
    0x05, 0x24, 0x01, 0x00, 0x01, // CDC Call

    0x07, 0x05, 0x83, 0x03, 0x40, 0x00, 0x0A, // EP
    0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00, // INTF
    0x07, 0x05, 0x01, 0x02, 0x40, 0x00, 0x00, // EP
    0x07, 0x05, 0x82, 0x02, 0x40, 0x00, 0x00, // EP

    0x09, 0x04, 0x02, 0x00, 0x00, 0xFF, 0x05, 0x00, 0x00, // INTF
};
}
