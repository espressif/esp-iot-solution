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
#include "led.h"

extern "C" void led_obj_test()
{
    led my_led_1(16, LED_DARK_LOW);
    led* my_led_2 = new led(17, LED_DARK_LOW);
    controllable_obj* con_objs[] = {&my_led_1, my_led_2};

    printf("open led_1, close led_2\n");
    my_led_1.on();
    my_led_2->off();
    
    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("led1 quick blink, led2 slow blink\n");
    my_led_1.quick_blink();
    my_led_2->slow_blink();

    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("change quick blink freqency and slow blink freqency to 10 and 2\n");
    led::blink_freq_write(10, 2);

    vTaskDelay(10000 / portTICK_RATE_MS);
    printf("open all\n");
    for (int i = 0; i < sizeof(con_objs)/sizeof(con_objs[0]); i++) {
        con_objs[i]->on();
    }

    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("set led1 to night mode\n");
    led::night_duty_write(50);
    my_led_1.mode_write(LED_NIGHT_MODE);

    vTaskDelay(5000 / portTICK_RATE_MS);
    printf("close all\n");
    for (int i = 0; i < sizeof(con_objs)/sizeof(con_objs[0]); i++) {
        con_objs[i]->off();
    }
}
