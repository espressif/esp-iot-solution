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
#include "iot_led.h"
#include "unity.h"

extern "C" void led_obj_test()
{
    CLED my_led_1(16, LED_DARK_LOW);
    CLED* my_led_2 = new CLED(17, LED_DARK_LOW);
    CControllable* con_objs[] = {&my_led_1, my_led_2};

    printf("open led_1, close led_2\n");
    my_led_1.on();
    my_led_2->off();
    
    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("led1 quick blink, led2 slow blink\n");
    my_led_1.quick_blink();
    my_led_2->slow_blink();

    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("change quick blink freqency and slow blink freqency to 10 and 2\n");
    CLED::blink_freq_write(10, 2);

    vTaskDelay(10000 / portTICK_RATE_MS);
    printf("open all\n");
    for (int i = 0; i < sizeof(con_objs)/sizeof(con_objs[0]); i++) {
        con_objs[i]->on();
    }

    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("set led1 to night mode\n");
    CLED::night_duty_write(50);
    my_led_1.mode_write(LED_NIGHT_MODE);

    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("close all\n");
    for (int i = 0; i < sizeof(con_objs)/sizeof(con_objs[0]); i++) {
        con_objs[i]->off();
    }
}

TEST_CASE("LED status test", "[led][iot]")
{
    led_obj_test();
}

