/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "test_utils.h"
#include "esp_log.h"
#include "iot_usbh.h"

TEST_CASE("iot_usbh init/deinit", "[iot_usbh]")
{
    for (size_t i = 0; i < 5; i++) {
        usbh_port_config_t config = DEFAULT_USBH_PORT_CONFIG();
        usbh_port_handle_t port_hdl = iot_usbh_port_init(&config);
        TEST_ASSERT_NOT_NULL(port_hdl);
        TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_start(port_hdl));
        vTaskDelay(pdMS_TO_TICKS(2000));
        TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_deinit(port_hdl));
    }

    usbh_port_config_t config = DEFAULT_USBH_PORT_CONFIG();
    usbh_port_handle_t port_hdl = iot_usbh_port_init(&config);
    TEST_ASSERT_NOT_NULL(port_hdl);
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_start(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(2000));
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_stop(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(5000));
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_start(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(10000));
    TEST_ASSERT_EQUAL(ESP_OK, iot_usbh_port_deinit(port_hdl));
}