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

SCENARIO("Ethernet converters descriptor parsing: AX88772A", "[ethernet][AX88772A]")
{
    GIVEN("ASIX AX88772A FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)premium_cord_device_desc_fs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)premium_cord_config_desc_fs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }

    GIVEN("ASIX AX88772A HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)premium_cord_device_desc_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)premium_cord_config_desc_hs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }
}

SCENARIO("Ethernet converters descriptor parsing: AX88772B", "[ethernet][AX88772B]")
{
    GIVEN("ASIX AX88772B FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)i_tec_device_desc_fs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)i_tec_config_desc_fs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            (void)ret;
            //@todo this is probably a bug in AX88772B descriptor
            // It uses CTRL endpoint instead of Interrupt for notification element
            // Should be solved during implementation of Ethernet drivers
            //REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }

    GIVEN("ASIX AX88772B HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)i_tec_device_desc_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)i_tec_config_desc_hs;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }
}

SCENARIO("Ethernet converters descriptor parsing: RTL8153", "[ethernet][RTL8153]")
{
    GIVEN("Realtek RTL8153 CFG 1 FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)axagon_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)axagon_config_desc_fs_1;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }

    GIVEN("Realtek RTL8153 CFG 2 FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)axagon_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)axagon_config_desc_fs_2;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 3);
        }
    }

    GIVEN("Realtek RTL8153 CFG 1 HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)axagon_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)axagon_config_desc_hs_1;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }

    GIVEN("Realtek RTL8153 CFG 2 HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)axagon_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)axagon_config_desc_hs_2;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 3);
        }
    }
}
