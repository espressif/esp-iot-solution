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

SCENARIO("Modems descriptor parsing: BG96", "[modem][BG96]")
{
    GIVEN("Quactel BG96 FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)bg96_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)bg96_config_desc_fs;

        // First 2 interfaces are without notification element
        for (int interface = 0; interface < 2; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
            }
        }

        // Interface 2-4 contain notification element
        for (int interface = 2; interface < 5; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }

    GIVEN("Quactel BG96 HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)bg96_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)bg96_config_desc_hs;

        // Interface 0-1  are without notification element
        for (int interface = 0; interface < 2; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
            }
        }

        // Interface 2-4 contain notification element
        for (int interface = 2; interface < 5; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }
}

SCENARIO("Modems descriptor parsing: 7600E", "[modem][7600E]")
{
    GIVEN("SimCom 7600E FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sim7600e_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sim7600e_config_desc_fs;

        // Interface 0 is without notification element
        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }

        // Interface 1-5 contain notification element
        for (int interface = 1; interface < 6; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }

    GIVEN("SimCom 7600E HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sim7600e_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sim7600e_config_desc_hs;

        // Interface 0 is without notification element
        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }

        // Interface 1-5 contain notification element
        for (int interface = 1; interface <= 5; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }
}

SCENARIO("Modems descriptor parsing: 7070G", "[modem][7070G]")
{
    GIVEN("SimCom 7070G FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sim7070G_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sim7070G_config_desc_fs;

        // Interface 0-4 do not contain notification element
        for (int interface = 0; interface <= 4; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
            }
        }

        // Interface 5 is with notification element
        SECTION("Interface 5") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 5, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }

    GIVEN("SimCom 7070G HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sim7070G_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sim7070G_config_desc_hs;

        // Interface 0-4 do not contain notification element
        for (int interface = 0; interface <= 4; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
            }
        }

        // Interface 5 is with notification element
        SECTION("Interface 5") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 5, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
        }
    }
}

SCENARIO("Modems descriptor parsing: 7000E", "[modem][7000E]")
{
    GIVEN("SimCom 7000E FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sim7000e_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sim7000e_config_desc_fs;

        // Interface 0-2 do not contain notification element
        for (int interface = 0; interface <= 2; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
            }
        }

        // Interface 3-5 contain notification element
        for (int interface = 3; interface <= 5; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }

    GIVEN("SimCom 7000E HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sim7000e_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sim7000e_config_desc_hs;

        // Interface 0-2 do not contain notification element
        for (int interface = 0; interface <= 2; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
            }
        }

        // Interface 3-5 contain notification element
        for (int interface = 3; interface <= 5; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }
}

SCENARIO("Modems descriptor parsing: A7672E", "[modem][A7672E]")
{
    GIVEN("SimCom A7672E FS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sima7672e_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sima7672e_config_desc_fs;

        // Interface 0-1 belongs USB_CLASS_WIRELESS_CONTROLLER class, expect no CDC interface
        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE(ret == ESP_ERR_NOT_FOUND);
        }

        // Diagnostic interface
        SECTION("Interface 2") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 2, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }

        // Interface 3: NMEA, Interface 4-5: AT
        for (int interface = 3; interface <= 5; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }

    GIVEN("SimCom A7672E HS") {
        const usb_device_desc_t *dev_desc = (const usb_device_desc_t *)sima7672e_device_desc_fs_hs;
        const usb_config_desc_t *cfg_desc = (const usb_config_desc_t *)sima7672e_config_desc_hs;

        // Interface 0-1 belongs USB_CLASS_WIRELESS_CONTROLLER class, expect no CDC interface
        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 0, &parsed_result);
            REQUIRE(ret == ESP_ERR_NOT_FOUND);
        }

        // Diagnostic interface
        SECTION("Interface 2") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, 2, &parsed_result);
            REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result);
        }

        // Interface 3: NMEA, Interface 4-5: AT
        for (int interface = 3; interface <= 5; ++interface) {
            SECTION("Interface " + std::to_string(interface)) {
                cdc_parsed_info_t parsed_result = {};
                esp_err_t ret = cdc_parse_interface_descriptor(dev_desc, cfg_desc, interface, &parsed_result);
                REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result);
            }
        }
    }
}
