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
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "iot_debugs.h"
#include "unity.h"

static const char *TAG = "debugs_test";

static void debug_123(void* arg)
{
    ESP_LOGI(TAG, "123");
}

static void debug_deepsleep(void *arg)
{
    ESP_LOGI(TAG, "sleep for 10 seconds");
    esp_sleep_enable_timer_wakeup(10 * 1000 * 1000);
    esp_deep_sleep_start();
}

TEST_CASE("Debug cmd test", "[debug][iot]")
{
    iot_debug_init(UART_NUM_1, 115200, 16, 17);
    debug_cmd_info_t cmd_group[] = {
        {"123", debug_123, NULL},
        {"deep sleep", debug_deepsleep, NULL}
    };
    iot_debug_add_cmd_group(cmd_group, sizeof(cmd_group)/sizeof(cmd_group[0]));
    iot_debug_add_cmd("wifi", DEBUG_CMD_WIFI_INFO);
    iot_debug_add_cmd("wakeup", DEBUG_CMD_WAKEUP_INFO);
    iot_debug_add_cmd("restart", DEBUG_CMD_RESTART);
}
