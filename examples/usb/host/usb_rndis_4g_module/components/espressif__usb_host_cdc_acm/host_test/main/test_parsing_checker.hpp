/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @brief Helper to check parsing result of CDC compliant devices
 *
 * CDC compliant device usually provides:
 * #. Notification interface
 * #. Data interface
 * #. CDC specific descriptors
 */
#define REQUIRE_CDC_COMPLIANT(ret, parsed_result, nb_of_cdc_specific) \
    REQUIRE(ESP_OK == ret); \
    REQUIRE((parsed_result).notif_intf != nullptr); \
    REQUIRE((parsed_result).notif_ep != nullptr); \
    REQUIRE((parsed_result).data_intf != nullptr); \
    REQUIRE((parsed_result).in_ep != nullptr); \
    REQUIRE((parsed_result).out_ep != nullptr); \
    REQUIRE((parsed_result).func != nullptr); \
    REQUIRE((parsed_result).func_cnt == nb_of_cdc_specific);

/**
 * @brief Helper to check parsing result of CDC non-compliant devices
 *
 * CDC non-compliant device usually provides only Data interface
 */
#define REQUIRE_CDC_NONCOMPLIANT(ret, parsed_result) \
    REQUIRE(ESP_OK == ret); \
    REQUIRE((parsed_result).notif_intf == nullptr); \
    REQUIRE((parsed_result).notif_ep == nullptr); \
    REQUIRE((parsed_result).data_intf != nullptr); \
    REQUIRE((parsed_result).in_ep != nullptr); \
    REQUIRE((parsed_result).out_ep != nullptr); \
    REQUIRE((parsed_result).func == nullptr); \
    REQUIRE((parsed_result).func_cnt == 0);

/**
 * @brief Helper to check parsing result of CDC non-compliant devices
 *
 * Some CDC non-compliant devices also provide Notification interface
 */
#define REQUIRE_CDC_NONCOMPLIANT_WITH_NOTIFICATION(ret, parsed_result) \
    REQUIRE(ESP_OK == ret); \
    REQUIRE((parsed_result).notif_intf != nullptr); \
    REQUIRE((parsed_result).notif_ep != nullptr); \
    REQUIRE((parsed_result).data_intf != nullptr); \
    REQUIRE((parsed_result).in_ep != nullptr); \
    REQUIRE((parsed_result).out_ep != nullptr); \
    REQUIRE((parsed_result).func == nullptr); \
    REQUIRE((parsed_result).func_cnt == 0);
