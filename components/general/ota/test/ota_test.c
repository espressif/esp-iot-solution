// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "iot_ota.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "iot_wifi_conn.h"
#include "unity.h"

#define OTA_SERVER_IP      CONFIG_OTA_SERVER_IP
#define OTA_SERVER_PORT    CONFIG_OTA_SERVER_PORT
#define OTA_FILE_NAME      CONFIG_OTA_SERVER_URI
#define TAG     "OTA_TEST"

#define AP_SSID     CONFIG_AP_SSID
#define AP_PASSWORD CONFIG_AP_PASSWORD

static void ota_task(void *arg)
{
    ESP_LOGI(TAG, "ota task test mutex");
    iot_ota_start(OTA_SERVER_IP, OTA_SERVER_PORT, OTA_FILE_NAME, portMAX_DELAY);
    vTaskDelete(NULL);
}

void ota_test()
{
    iot_wifi_setup(WIFI_MODE_STA);
    iot_wifi_connect(AP_SSID, AP_PASSWORD, portMAX_DELAY);
    ESP_LOGI(TAG, "free heap size before ota: %d", esp_get_free_heap_size());
    xTaskCreate(ota_task, "ota_task", 1024 * 8, NULL, 5, NULL);
    while (iot_ota_get_ratio() < 100) {
        ESP_LOGI(TAG, "OTA progress: %d %%", iot_ota_get_ratio());
        vTaskDelay(500 / portTICK_RATE_MS);
    }
    ESP_LOGI(TAG, "OTA done: %d %%", iot_ota_get_ratio());
    vTaskDelay(1000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "free heap size after ota: %d", esp_get_free_heap_size());
    esp_restart();
} 

TEST_CASE("OTA test", "[ota][iot]")
{
    ota_test();
}
