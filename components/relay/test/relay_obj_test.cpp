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
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "relay.h"

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
    relay relay_0(relay_io_0, RELAY_CLOSE_HIGH, RELAY_DFLIP_CONTROL, RELAY_IO_NORMAL);
    relay relay_1(relay_io_1, RELAY_CLOSE_HIGH, RELAY_GPIO_CONTROL, RELAY_IO_NORMAL);


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
