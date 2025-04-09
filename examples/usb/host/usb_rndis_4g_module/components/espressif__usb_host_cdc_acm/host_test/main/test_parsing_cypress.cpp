/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <catch2/catch_test_macros.hpp>

#include "descriptors/cypress_rfa.hpp"
#include "test_parsing_checker.hpp"
#include "cdc_host_descriptor_parsing.h"
#include "usb/cdc_acm_host.h"

using namespace cypress_rfa;

SCENARIO("Custom descriptor parsing: Cypress", "[cypress][rfa]")
{
    GIVEN("RFA") {
        const usb_device_desc_t *dev = (const usb_device_desc_t *)dev_desc;
        const usb_config_desc_t *cfg = (const usb_config_desc_t *)cfg_desc;

        SECTION("Interface 0") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev, cfg, 0, &parsed_result);
            REQUIRE_CDC_COMPLIANT(ret, parsed_result, 4);
        }

        SECTION("Interface 2") {
            cdc_parsed_info_t parsed_result = {};
            esp_err_t ret = cdc_parse_interface_descriptor(dev, cfg, 2, &parsed_result);
            REQUIRE_FALSE(ESP_OK == ret);
        }
    }
}
