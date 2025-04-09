/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <catch2/catch_test_macros.hpp>
#include "usb/usb_helpers.h"
// #include "usb/usb_host.h"

#include "descriptors/cdc_descriptors.hpp"
#include "test_parsing_checker.hpp"
#include "cdc_host_descriptor_parsing.h"
#include "usb/cdc_acm_host.h"

// extern "C" {
// #include "Mockusb_host.h"
// }

SCENARIO("USB-UART converters descriptor parsing: CP210x", "[uart][CP210x]")
{
    GIVEN("Silicon Labs CP210x Full-Speed") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)cp210x_device_desc;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)cp210x_config_desc;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }

        /*
        //@todo Run this section when we the USB host mock is complete
        SECTION("Device opening") {
            int num_of_devices = 1;
            usb_device_handle_t dev_handle = 11;
            usb_host_device_addr_list_fill_ExpectAnyArgsAndReturn(ESP_OK);
            usb_host_device_addr_list_fill_ReturnThruPtr_num_dev_ret(&num_of_devices);

            usb_host_device_open_ExpectAnyArgsAndReturn(ESP_OK);
            usb_host_device_open_ReturnThruPtr_dev_hdl_ret(&dev_handle);

            usb_host_get_device_descriptor_ExpectAnyArgsAndReturn(ESP_OK);
            usb_host_get_device_descriptor_ReturnThruPtr_device_desc(&dev_desc);

            usb_host_device_close_ExpectAnyArgsAndReturn(ESP_OK);

            usb_host_get_device_descriptor_ExpectAnyArgsAndReturn(ESP_OK);
            usb_host_get_device_descriptor_ReturnThruPtr_device_desc(&dev_desc);

            usb_host_get_active_config_descriptor_ExpectAnyArgsAndReturn(ESP_OK);
            usb_host_get_active_config_descriptor_ReturnThruPtr_config_desc(&cfg_desc);

            cdc_acm_dev_hdl_t dev = nullptr;
            cdc_acm_host_device_config_t config = {
                .connection_timeout_ms = 1000,
                .out_buffer_size = 100,
                .in_buffer_size = 100,
                .event_cb = nullptr,
                .data_cb = nullptr,
                .user_arg = nullptr,
            };
            esp_err_t ret = cdc_acm_host_open(0x10C4, 0xEA60, 0, &config, &dev);

            REQUIRE(ESP_OK == ret);
            REQUIRE(dev != nullptr);
        }
        */
    }
}

SCENARIO("USB-UART converters descriptor parsing: FTDI", "[uart][FTDI]")
{
    GIVEN("FTDI chip FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)ftdi_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)ftdi_config_desc_fs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }
    }

    GIVEN("FTDI chip HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)ftdi_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)ftdi_config_desc_hs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }

        SECTION("Interface 1") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 1, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }
    }
}

SCENARIO("USB-UART converters descriptor parsing: TTL232", "[uart][TTL232]")
{
    GIVEN("TTL232") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)ttl232_device_desc;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)ttl232_config_desc;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }
    }
}

SCENARIO("USB-UART converters descriptor parsing: CH340", "[uart][CH340]")
{
    GIVEN("CH340") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)ch340_device_desc;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)ch340_config_desc;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }
}
