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
#include "esp_system.h"
#include "iot_wifi_conn.h"
#include "unity.h"

#define AP_SSID     CONFIG_AP_SSID
#define AP_PASSWORD CONFIG_AP_PASSWORD

extern "C" void wifi_obj_test()
{
    CWiFi *my_wifi = CWiFi::GetInstance(WIFI_MODE_STA);
    printf("connect wifi\n");
    my_wifi->Connect(AP_SSID, CONFIG_AP_PASSWORD, portMAX_DELAY);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    my_wifi->Disconnect();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    my_wifi->Connect(AP_SSID, CONFIG_AP_PASSWORD, portMAX_DELAY);
}

TEST_CASE("Wifi connect test", "[wifi_connect][iot][wifi]")
{
    wifi_obj_test();
}

