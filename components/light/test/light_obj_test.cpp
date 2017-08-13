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

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "light.h"
#include "unity.h"

#define CHANNEL_ID_R    0
#define CHANNEL_ID_G    1
#define CHANNEL_ID_B    2
#define CHANNEL_R_IO    GPIO_NUM_15
#define CHANNEL_G_IO    GPIO_NUM_18
#define CHANNEL_B_IO    GPIO_NUM_19

extern "C" void light_obj_test()
{
    const char* TAG = "light_obj_test";
    ESP_LOGI(TAG, "create a light with 3 channels");
    CLight my_light(LIGHT_CH_NUM_3);

    my_light.red.init(CHANNEL_R_IO, LEDC_CHANNEL_0);
    my_light.green.init(CHANNEL_G_IO, LEDC_CHANNEL_1);
    my_light.blue.init(CHANNEL_B_IO, LEDC_CHANNEL_2);

    ESP_LOGI(TAG, "turn on the light");
    my_light.on();
    vTaskDelay(1000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "turn off the light");
    my_light.off();
    vTaskDelay(1000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "set different duty");
    my_light.red.duty(my_light.get_full_duty());
    my_light.green.duty((my_light.get_full_duty() / 3));
    my_light.blue.duty((my_light.get_full_duty() / 2));
    vTaskDelay(2000 / portTICK_RATE_MS);

    my_light.red.duty(0);
    my_light.green.duty(0);
    my_light.blue.duty(0);
    vTaskDelay(100 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "make light blink");
    my_light.blink_start(1 << CHANNEL_ID_R, 100);
    vTaskDelay(2000 / portTICK_RATE_MS);
    my_light.blink_start(1 << CHANNEL_ID_G, 100);
    vTaskDelay(2000 / portTICK_RATE_MS);
    my_light.blink_start(1 << CHANNEL_ID_B, 100);
    vTaskDelay(2000 / portTICK_RATE_MS);
    my_light.blink_start((1 << CHANNEL_ID_R) | (1 << CHANNEL_ID_G), 100);
    vTaskDelay(2000 / portTICK_RATE_MS);
    my_light.blink_start((1 << CHANNEL_ID_G) | (1 << CHANNEL_ID_B), 100);
    vTaskDelay(2000 / portTICK_RATE_MS);
    my_light.blink_start((1 << CHANNEL_ID_B) | (1 << CHANNEL_ID_R), 100);
    vTaskDelay(2000 / portTICK_RATE_MS);
    my_light.blink_stop();

    my_light.red.duty(my_light.get_full_duty());
    my_light.green.duty(0);
    my_light.blue.duty(0);
    vTaskDelay(1000 / portTICK_RATE_MS);
    my_light.red.duty(0);
    my_light.green.duty(my_light.get_full_duty());
    my_light.blue.duty(0);
    vTaskDelay(1000 / portTICK_RATE_MS);
    my_light.red.duty(0);
    my_light.green.duty(0);
    my_light.blue.duty(my_light.get_full_duty());
    vTaskDelay(1000 / portTICK_RATE_MS);
    my_light.blue.duty(0);

    ESP_LOGI(TAG, "set CHANNEL_R to breath mode");
    my_light.red.breath(1000);
    vTaskDelay(3000 / portTICK_RATE_MS);
    my_light.red.duty(0);
    ESP_LOGI(TAG, "set CHANNEL_G to breath mode");
    my_light.green.breath(1000);
    vTaskDelay(3000 / portTICK_RATE_MS);
    my_light.green.duty(0);
    ESP_LOGI(TAG, "set CHANNEL_B to breath mode");
    my_light.blue.breath(1000);
    vTaskDelay(3000 / portTICK_RATE_MS);
    my_light.blue.duty(0);

    ESP_LOGI(TAG, "set CHANNEL_R to breath mode");
    my_light.red.breath(1000);
    my_light.green.breath(1000);
    vTaskDelay(3000 / portTICK_RATE_MS);
    my_light.red.duty(0);
    ESP_LOGI(TAG, "set CHANNEL_G to breath mode");
    my_light.green.breath(1000);
    my_light.blue.breath(1000);
    vTaskDelay(3000 / portTICK_RATE_MS);
    my_light.green.duty(0);
    ESP_LOGI(TAG, "set CHANNEL_B to breath mode");
    my_light.blue.breath(1000);
    my_light.red.breath(1000);
    vTaskDelay(3000 / portTICK_RATE_MS);
    my_light.blue.duty(0);
}

TEST_CASE("LIGHT obj test", "[light_cpp][iot]")
{
    light_obj_test();
}
