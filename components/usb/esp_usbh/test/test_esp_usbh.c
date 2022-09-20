// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "test_utils.h"
#include "esp_log.h"
#include "esp_usbh.h"

TEST_CASE("esp_usbh init/deinit", "[esp_usbh]")
{
    for (size_t i = 0; i < 5; i++) {
        usbh_port_config_t config = DEFAULT_USBH_PORT_CONFIG();
        usbh_port_handle_t port_hdl = esp_usbh_port_init(&config);
        TEST_ASSERT_NOT_NULL(port_hdl);
        TEST_ASSERT_EQUAL(ESP_OK, esp_usbh_port_start(port_hdl));
        vTaskDelay(pdMS_TO_TICKS(2000));
        TEST_ASSERT_EQUAL(ESP_OK, esp_usbh_port_deinit(port_hdl));
    }

    usbh_port_config_t config = DEFAULT_USBH_PORT_CONFIG();
    usbh_port_handle_t port_hdl = esp_usbh_port_init(&config);
    TEST_ASSERT_NOT_NULL(port_hdl);
    TEST_ASSERT_EQUAL(ESP_OK, esp_usbh_port_start(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(2000));
    TEST_ASSERT_EQUAL(ESP_OK, esp_usbh_port_stop(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(5000));
    TEST_ASSERT_EQUAL(ESP_OK, esp_usbh_port_start(port_hdl));
    vTaskDelay(pdMS_TO_TICKS(10000));
    TEST_ASSERT_EQUAL(ESP_OK, esp_usbh_port_deinit(port_hdl));
}