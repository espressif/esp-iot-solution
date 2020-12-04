// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "servo.h"
#include "unity.h"
#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define SERVO_CH0_PIN 32
#define SERVO_CH1_PIN 25
#define SERVO_CH2_PIN 26
#define SERVO_CH3_PIN 27
#define SERVO_CH4_PIN 14
#define SERVO_CH5_PIN 12
#define SERVO_CH6_PIN 13
#define SERVO_CH7_PIN 15
#elif CONFIG_IDF_TARGET_ESP32S2
#define SERVO_CH0_PIN 1
#define SERVO_CH1_PIN 2
#define SERVO_CH2_PIN 3
#define SERVO_CH3_PIN 4
#define SERVO_CH4_PIN 5
#define SERVO_CH5_PIN 6
#define SERVO_CH6_PIN 7
#define SERVO_CH7_PIN 8
#endif

TEST_CASE("Servo_motor test", "[servo][iot]")
{
    servo_config_t servo_cfg = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .freq = 50,
        .timer_number = LEDC_TIMER_0,
        .channels = {
            .servo_pin = {
                SERVO_CH0_PIN,
                SERVO_CH1_PIN,
                SERVO_CH2_PIN,
                SERVO_CH3_PIN,
                SERVO_CH4_PIN,
                SERVO_CH5_PIN,
                SERVO_CH6_PIN,
                SERVO_CH7_PIN,
            },
            .ch = {
                LEDC_CHANNEL_0,
                LEDC_CHANNEL_1,
                LEDC_CHANNEL_2,
                LEDC_CHANNEL_3,
                LEDC_CHANNEL_4,
                LEDC_CHANNEL_5,
                LEDC_CHANNEL_6,
                LEDC_CHANNEL_7,
            },
        },
        .channel_number = 8,
    } ;
    TEST_ASSERT(ESP_OK == servo_init(&servo_cfg));

    size_t i;
    uint8_t times = 2;
    while (times--) {
        for (i = 0; i <= 18; i++) {
            ESP_LOGI("servo", "aet angle: %d", 10 * i);
            servo_write_angle(0, 10 * i);
            servo_write_angle(1, 10 * i);
            servo_write_angle(2, 10 * i);
            servo_write_angle(3, 10 * i);
            servo_write_angle(4, 10 * i);
            servo_write_angle(5, 10 * i);
            servo_write_angle(6, 10 * i);
            servo_write_angle(7, 10 * i);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        for (; i > 0; i--) {
            ESP_LOGI("servo", "aet angle: %d", 10 * i);
            servo_write_angle(0, 10 * i);
            servo_write_angle(1, 10 * i);
            servo_write_angle(2, 10 * i);
            servo_write_angle(3, 10 * i);
            servo_write_angle(4, 10 * i);
            servo_write_angle(5, 10 * i);
            servo_write_angle(6, 10 * i);
            servo_write_angle(7, 10 * i);
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
    servo_deinit();
}
