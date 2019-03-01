// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
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

/* c includes */
#include <string.h>

/* freertos includes */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/* esp includes */
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "esp_event_loop.h"
#include "esp_log.h"

/* other includes */
#include "iot_blufi.h"
#include "nvs_flash.h"
#include "unity.h"
static const char *TAG = "iot_blufi_test";

#define IOT_BLUFI_TEST_EN 1
#if IOT_BLUFI_TEST_EN

/*
 * user should not set system_event_cb with esp_event_loop_init() or esp_event_loop_set_cb() durating Blufi configuration,
 * because internal system_event_cb in iot_blufi will do reconnection when wifi disconnected and report sta connect success.
 */
static void initialise_wifi(void)
{
    tcpip_adapter_init();
    esp_event_loop_init(NULL, NULL);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void iot_blufi_block_test()
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    initialise_wifi();

    ret = iot_blufi_start(true, 30 * 1000 / portTICK_PERIOD_MS);

    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "Iot Blufi start fail");
    } else {
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGI(TAG, "Iot Blufi start fail, timeout[30000ms]...");
        } else {
            wifi_config_t sta_config = {0};
            esp_wifi_get_config(ESP_IF_WIFI_STA, &sta_config);
            ESP_LOGI(TAG, "Iot Blufi Config: SSID %s, pwd %s, bssid "MACSTR,
                     (char *)sta_config.sta.ssid, (char *)sta_config.sta.password, MAC2STR(sta_config.sta.bssid));
        }

        iot_blufi_stop(false);
    }
}

void iot_blufi_nonblock_test()
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    initialise_wifi();

    if (iot_blufi_start(true, 0) != ESP_OK) {
        return;
    }

    while (1) {
        ESP_LOGI(TAG, "Iot Blufi status: %d", iot_blufi_get_status());

        /* user can refer to esp_blufi_cb_event_t to select other event value to exit Blufi */
        if (iot_blufi_get_status() == ESP_BLUFI_EVENT_BLE_DISCONNECT) {
            wifi_config_t sta_config = {0};
            esp_wifi_get_config(ESP_IF_WIFI_STA, &sta_config);
            ESP_LOGI(TAG, "Iot Blufi Config: SSID %s, pwd %s, bssid "MACSTR,
                     (char *)sta_config.sta.ssid, (char *)sta_config.sta.password, MAC2STR(sta_config.sta.bssid));
            break;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    iot_blufi_stop(false);
}


TEST_CASE("BLUFI block test", "[blufi][iot]")
{
    iot_blufi_block_test();
}

TEST_CASE("BLUFI nonblock test", "[blufi][iot]")
{
    iot_blufi_nonblock_test();
}

#endif
