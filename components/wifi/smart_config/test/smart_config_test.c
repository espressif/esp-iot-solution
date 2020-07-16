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
#include "iot_smartconfig.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "unity.h"

#define SMART_CONFIG_TEST_EN 1
#if SMART_CONFIG_TEST_EN


void sc_test()
{
    esp_err_t res;
    iot_sc_setup(SC_TYPE_ESPTOUCH, WIFI_MODE_STA, 0);
    while (1) {
        res = iot_sc_start(20000 / portTICK_PERIOD_MS);
        if (res == ESP_OK) {
            printf("connected\n");
            break;
        } else if (res == ESP_ERR_TIMEOUT) {
            printf("smart config timeout\n");
        } else if (res == ESP_FAIL) {
            printf("smart config stopped\n");
        }
    }

}

TEST_CASE("ESPTOUCH test", "[esptouch][iot]")
{
    sc_test();
}

#endif
