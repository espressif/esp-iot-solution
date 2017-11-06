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
