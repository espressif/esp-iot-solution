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
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "iot_relay.h"
#include "unity.h"

#define RELAY_0_D_IO_NUM    GPIO_NUM_2
#define RELAY_0_CP_IO_NUM   GPIO_NUM_4
#define RELAY_1_D_IO_NUM    GPIO_NUM_12
#define RELAY_1_CP_IO_NUM   GPIO_NUM_13

extern "C" void relay_obj_test()
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
    CRelay relay_0(relay_io_0, RELAY_CLOSE_HIGH, RELAY_DFLIP_CONTROL, RELAY_IO_NORMAL);
    CRelay relay_1(relay_io_1, RELAY_CLOSE_HIGH, RELAY_GPIO_CONTROL, RELAY_IO_NORMAL);


    relay_0.on();
    printf("relay[0]: on, status: %d\n", relay_0.status());
    vTaskDelay(2000 / portTICK_RATE_MS);

    relay_1.off();
    printf("relay[1]: off, status: %d\n", relay_1.status());
    vTaskDelay(2000 / portTICK_RATE_MS);

    relay_0.off();
    printf("relay[0]: off, status: %d\n", relay_0.status());
    vTaskDelay(2000 / portTICK_RATE_MS);

    relay_1.on();
    printf("relay[1]: on, status: %d\n", relay_1.status());
    vTaskDelay(2000 / portTICK_RATE_MS);

    relay_0.on();
    printf("relay[0]: on, status: %d\n", relay_0.status());
    vTaskDelay(2000 / portTICK_RATE_MS);

    relay_1.off();
    printf("relay[1]: off, status: %d\n", relay_1.status());
    vTaskDelay(2000 / portTICK_RATE_MS);

    relay_0.off();
    printf("relay[0]: off, status: %d\n", relay_0.status());
}

TEST_CASE("Relay cpp test", "[relay_cpp][iot]")
{
    relay_obj_test();
}
