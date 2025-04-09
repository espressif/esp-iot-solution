/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <catch2/catch_test_macros.hpp>
#include "usb/usb_helpers.h"

#include "descriptors/cdc_descriptors.hpp"
#include "test_parsing_checker.hpp"
#include "cdc_host_descriptor_parsing.h"
#include "usb/cdc_acm_host.h"

SCENARIO("TinyUSB devices descriptor parsing", "[tinyusb]")
{
    GIVEN("TinyUSB single FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)tusb_serial_device_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)tusb_serial_device_config_desc_fs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }
    }

    GIVEN("TinyUSB single HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)tusb_serial_device_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)tusb_serial_device_config_desc_hs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }
    }

    GIVEN("TinyUSB dual FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)tusb_serial_device_dual_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)tusb_serial_device_dual_config_desc_fs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }

        SECTION("Interface 2") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 2, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }
    }

    GIVEN("TinyUSB dual HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)tusb_serial_device_dual_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)tusb_serial_device_dual_config_desc_hs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }

        SECTION("Interface 2") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 2, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }
    }

    GIVEN("TinyUSB CDC+MSC FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)tusb_composite_device_desc;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)tusb_composite_config_desc;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }
    }
}
