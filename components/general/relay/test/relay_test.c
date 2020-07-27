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

    for (size_t i = 0; i < 8; i++)
    {
        iot_relay_state_write(relay_0, RELAY_STATUS_OPEN);
        iot_relay_state_write(relay_1, RELAY_STATUS_OPEN);
        ESP_LOGI(TAG, "relay0 state:%d", iot_relay_state_read(relay_0));
        ESP_LOGI(TAG, "relay1 state:%d", iot_relay_state_read(relay_1));
        vTaskDelay(1000 / portTICK_RATE_MS);
        iot_relay_state_write(relay_0, RELAY_STATUS_CLOSE);
        iot_relay_state_write(relay_1, RELAY_STATUS_CLOSE);
        ESP_LOGI(TAG, "relay0 state:%d", iot_relay_state_read(relay_0));
        ESP_LOGI(TAG, "relay1 state:%d", iot_relay_state_read(relay_1));
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
    
    iot_relay_delete(relay_0);
    iot_relay_delete(relay_1);
    
}

TEST_CASE("Relay test", "[relay][iot]")
{
    relay_test();
}
