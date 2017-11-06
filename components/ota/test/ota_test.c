/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "ota.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "iot_wifi.h"
#include "unity.h"

#define OTA_SERVER_IP      "192.168.1.3"
#define OTA_SERVER_PORT    8070
#define OTA_FILE_NAME      "/Desktop/iot.bin"
#define TAG     "OTA_TEST"

#define AP_SSID     CONFIG_AP_SSID
#define AP_PASSWORD CONFIG_AP_PASSWORD

static void ota_task(void *arg)
{
    ESP_LOGI(TAG, "ota task test mutex");
    ota_start(OTA_SERVER_IP, OTA_SERVER_PORT, OTA_FILE_NAME, portMAX_DELAY);
    vTaskDelete(NULL);
}

void ota_test()
{
    iot_wifi_setup(WIFI_MODE_STA);
    iot_wifi_connect(AP_SSID, AP_PASSWORD, portMAX_DELAY);
    ESP_LOGI(TAG, "free heap size before ota: %d", esp_get_free_heap_size());
    xTaskCreate(ota_task, "ota_task", 1024 * 8, NULL, 5, NULL);
    ota_start(OTA_SERVER_IP, OTA_SERVER_PORT, OTA_FILE_NAME, 5000 / portTICK_RATE_MS);
    vTaskDelay(10000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "free heap size after ota: %d", esp_get_free_heap_size());
    esp_restart();
} 

TEST_CASE("OTA test", "[ota][iot]")
{
    ota_test();
}
