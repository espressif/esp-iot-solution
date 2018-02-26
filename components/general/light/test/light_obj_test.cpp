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
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_light.h"
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
