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
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/rtc_io.h"
#include "iot_relay.h"
#include "unity.h"

#define RELAY_0_D_IO_NUM    2
#define RELAY_0_CP_IO_NUM   4
#define RELAY_1_D_IO_NUM    12
#define RELAY_1_CP_IO_NUM   13
#define TAG "relay"

static relay_handle_t relay_0, relay_1;

void relay_test()
{
    relay_io_t relay_io_0 = {
        .flip_io = {
            .d_io_num = RELAY_0_D_IO_NUM,
            .cp_io_num = RELAY_0_CP_IO_NUM,
        },
    };
    relay_io_t relay_io_1 = {
        .single_io = {
            .ctl_io_num = RELAY_1_D_IO_NUM,
        },
    };
    relay_0 = iot_relay_create(relay_io_0, RELAY_CLOSE_HIGH, RELAY_DFLIP_CONTROL, RELAY_IO_RTC);
    relay_1 = iot_relay_create(relay_io_1, RELAY_CLOSE_HIGH, RELAY_DFLIP_CONTROL, RELAY_IO_RTC);
    iot_relay_state_write(relay_0, RELAY_STATUS_CLOSE);
    iot_relay_state_write(relay_1, RELAY_STATUS_OPEN);
    ESP_LOGI(TAG, "relay0 state:%d", iot_relay_state_read(relay_0));
    ESP_LOGI(TAG, "relay1 state:%d", iot_relay_state_read(relay_1));
    vTaskDelay(1 / portTICK_RATE_MS);
    iot_relay_state_write(relay_0, RELAY_STATUS_OPEN);
    iot_relay_state_write(relay_1, RELAY_STATUS_CLOSE);
    ESP_LOGI(TAG, "relay0 state:%d", iot_relay_state_read(relay_0));
    ESP_LOGI(TAG, "relay1 state:%d", iot_relay_state_read(relay_1));
    vTaskDelay(1 / portTICK_RATE_MS);
    iot_relay_state_write(relay_0, RELAY_STATUS_CLOSE);
    iot_relay_state_write(relay_1, RELAY_STATUS_OPEN);
    ESP_LOGI(TAG, "relay0 state:%d", iot_relay_state_read(relay_0));
    ESP_LOGI(TAG, "relay1 state:%d", iot_relay_state_read(relay_1));
}

TEST_CASE("Relay test", "[relay][iot]")
{
    relay_test();
}
