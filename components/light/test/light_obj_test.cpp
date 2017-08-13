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
    light my_light(3);

    my_light.channel_regist(CHANNEL_ID_R, CHANNEL_R_IO, LEDC_CHANNEL_0);
    my_light.channel_regist(CHANNEL_ID_G, CHANNEL_G_IO, LEDC_CHANNEL_1);
    my_light.channel_regist(CHANNEL_ID_B, CHANNEL_B_IO, LEDC_CHANNEL_2);

    ESP_LOGI(TAG, "open the light");
    my_light.on();
    vTaskDelay(5000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "close the light");
    my_light.off();
    vTaskDelay(5000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "set different duty");
    my_light.duty_write(CHANNEL_ID_R, my_light.get_full_duty());
    my_light.duty_write(CHANNEL_ID_G, 0);
    my_light.duty_write(CHANNEL_ID_B, (my_light.get_full_duty() / 2));
    vTaskDelay(5000 / portTICK_RATE_MS);

    ESP_LOGI(TAG, "set CHANNEL_G to breath mode");
    my_light.breath_write(CHANNEL_ID_G, 4000);

    vTaskDelay(10000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "make light blink");
    my_light.blink_start((1<<CHANNEL_ID_R)|(1<<CHANNEL_ID_G)|(1<<CHANNEL_ID_B), 100);

    vTaskDelay(10000 / portTICK_RATE_MS);   //delay 10 seconds, or the light object would be delete automatically
}

class A
{
public:
    int a;
    A( int i ){
        a = i;
    }
};

class B
{
public:
    A a;
//    B(A t)
//    {
//        a = t;
//    }
    B(int t):a(t){}
//private:
};




TEST_CASE("LIGHT obj test", "[light_cpp][iot]")
{
    A a(5);
    B b(7);
    printf("test b.a.a: %d\n", b.a.a);
    printf("t: %d\n", a.a);

    light_obj_test();
}
