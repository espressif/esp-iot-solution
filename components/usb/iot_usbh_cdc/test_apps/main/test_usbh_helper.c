/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "unity.h"
#include "iot_usbh_cdc.h"
#include "esp_log.h"

#define TAG "cdc_helper_test"

int force_link;

TEST_CASE("usb helper match device", "[iot_usbh_cdc][auto][helper]")
{
    // Test device matching
    usb_device_desc_t dev_desc = {
        .bLength = sizeof(usb_device_desc_t),
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0x02,
        .bDeviceSubClass = 0x00,
        .bDeviceProtocol = 0x00,
        .bMaxPacketSize0 = 64,
        .idVendor = 0x0483,
        .idProduct = 0x5740,
        .bcdDevice = 0x0100,
        .iManufacturer = 0x01,
        .iProduct = 0x02,
        .iSerialNumber = 0x03,
        .bNumConfigurations = 0x01
    };

    // Test 1: Match vendor ID only
    usb_device_match_id_t id1 = {
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR,
        .idVendor = 0x0483
    };
    TEST_ASSERT_EQUAL(1, usbh_match_device(&dev_desc, &id1));

    // Test 2: Match vendor and product ID
    usb_device_match_id_t id2 = {
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
        .idVendor = 0x0483,
        .idProduct = 0x5740
    };
    TEST_ASSERT_EQUAL(1, usbh_match_device(&dev_desc, &id2));

    // Test 3: No match - wrong vendor ID
    usb_device_match_id_t id3 = {
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR,
        .idVendor = 0x0484
    };
    TEST_ASSERT_EQUAL(0, usbh_match_device(&dev_desc, &id3));

    // Test 4: Device class match
    usb_device_match_id_t id4 = {
        .match_flags = USB_DEVICE_ID_MATCH_DEV_CLASS,
        .bDeviceClass = 0x02
    };
    TEST_ASSERT_EQUAL(1, usbh_match_device(&dev_desc, &id4));

    // Test 5: Multiple flag match (vendor, product, class)
    usb_device_match_id_t id5 = {
        .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT | USB_DEVICE_ID_MATCH_DEV_CLASS,
        .idVendor = 0x0483,
        .idProduct = 0x5740,
        .bDeviceClass = 0x02
    };
    TEST_ASSERT_EQUAL(1, usbh_match_device(&dev_desc, &id5));

    // Test 6: BCD device version match
    usb_device_match_id_t id6 = {
        .match_flags = USB_DEVICE_ID_MATCH_DEV_BCD,
        .bcdDevice = 0x0100
    };
    TEST_ASSERT_EQUAL(1, usbh_match_device(&dev_desc, &id6));

    // Test 7: Invalid inputs
    TEST_ASSERT_EQUAL(0, usbh_match_device(NULL, &id1));
    TEST_ASSERT_EQUAL(0, usbh_match_device(&dev_desc, NULL));
}

TEST_CASE("usb helper match interface", "[iot_usbh_cdc][auto][helper]")
{
    usb_intf_desc_t intf_desc = {
        .bLength = sizeof(usb_intf_desc_t),
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = 0x0A,
        .bInterfaceSubClass = 0x00,
        .bInterfaceProtocol = 0x01,
        .iInterface = 0
    };

    // Test 1: Match interface class
    usb_device_match_id_t id1 = {
        .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS,
        .bInterfaceClass = 0x0A
    };
    TEST_ASSERT_EQUAL(1, usbh_match_interface(&intf_desc, &id1));

    // Test 2: Match interface number
    usb_device_match_id_t id2 = {
        .match_flags = USB_DEVICE_ID_MATCH_INT_NUMBER,
        .bInterfaceNumber = 0
    };
    TEST_ASSERT_EQUAL(1, usbh_match_interface(&intf_desc, &id2));

    // Test 3: No match - wrong interface class
    usb_device_match_id_t id3 = {
        .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS,
        .bInterfaceClass = 0x0B
    };
    TEST_ASSERT_EQUAL(0, usbh_match_interface(&intf_desc, &id3));

    // Test 4: Match multiple fields
    usb_device_match_id_t id4 = {
        .match_flags = USB_DEVICE_ID_MATCH_INT_CLASS | USB_DEVICE_ID_MATCH_INT_PROTOCOL,
        .bInterfaceClass = 0x0A,
        .bInterfaceProtocol = 0x01
    };
    TEST_ASSERT_EQUAL(1, usbh_match_interface(&intf_desc, &id4));

    // Test 5: Invalid inputs
    TEST_ASSERT_EQUAL(0, usbh_match_interface(NULL, &id1));
    TEST_ASSERT_EQUAL(0, usbh_match_interface(&intf_desc, NULL));
}

TEST_CASE("usb helper match from id list", "[iot_usbh_cdc][auto][helper]")
{
    usb_device_desc_t dev_desc = {
        .bLength = sizeof(usb_device_desc_t),
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_DEVICE,
        .idVendor = 0x0483,
        .idProduct = 0x5740,
        .bDeviceClass = 0x02
    };

    usb_device_match_id_t id_list[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR,
            .idVendor = 0x0484
        },
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT,
            .idVendor = 0x0483,
            .idProduct = 0x5740
        },
        {0},
    };

    // Test match from list
    TEST_ASSERT_EQUAL(1, usbh_match_device_from_id_list(&dev_desc, id_list));

    // Test no match from list
    id_list[1].idProduct = 0x5741;
    TEST_ASSERT_EQUAL(0, usbh_match_device_from_id_list(&dev_desc, id_list));

    // Test invalid inputs
    TEST_ASSERT_EQUAL(0, usbh_match_device_from_id_list(NULL, id_list));
    TEST_ASSERT_EQUAL(0, usbh_match_device_from_id_list(&dev_desc, NULL));
}

TEST_CASE("usb helper match complete configuration", "[iot_usbh_cdc][auto][helper]")
{
    // Create a test configuration descriptor with interface descriptors
    const uint8_t config_data[] = {
        0x09, 0x02, 0x27, 0x00, 0x01, 0x01, 0x00, 0x80, 0x31, 0x09, 0x04, 0x00, 0x00,
        0x03, 0xFF, 0x01, 0x02, 0x00, 0x07, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x07,
        0x05, 0x02, 0x02, 0x20, 0x00, 0x00, 0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x01
    };

    usb_config_desc_t *config_desc = (usb_config_desc_t *)config_data;
    usb_device_desc_t dev_desc = {
        .bLength = sizeof(usb_device_desc_t),
        .bDescriptorType = USB_B_DESCRIPTOR_TYPE_DEVICE,
        .bcdUSB = 0x110,
        .idVendor = 0x0483,
        .idProduct = 0x5740,
        .bDeviceClass = 0x02
    };

    usb_device_match_id_t id_list[] = {
        {
            .match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_INT_CLASS,
            .idVendor = 0x0483,
            .bInterfaceClass = 0xFF
        },
        {
            .match_flags = 0
        }
    };

    int match_intf = -1;
    // Test complete match
    TEST_ASSERT_EQUAL(1, usbh_match_id_from_list(&dev_desc, config_desc, id_list, &match_intf));
    TEST_ASSERT_EQUAL(0, match_intf);

    // Test no match - wrong interface class
    id_list[0].bInterfaceClass = 0x0B;
    TEST_ASSERT_EQUAL(0, usbh_match_id_from_list(&dev_desc, config_desc, id_list, &match_intf));

    // Test invalid inputs
    TEST_ASSERT_EQUAL(0, usbh_match_id_from_list(NULL, config_desc, id_list, &match_intf));
    TEST_ASSERT_EQUAL(0, usbh_match_id_from_list(&dev_desc, NULL, id_list, &match_intf));
    TEST_ASSERT_EQUAL(0, usbh_match_id_from_list(&dev_desc, config_desc, NULL, NULL));
}
