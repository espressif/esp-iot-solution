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
#include "iot_servo.h"
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
#define SERVO_CH8_PIN 2
#define SERVO_CH9_PIN 0
#define SERVO_CH10_PIN 4
#define SERVO_CH11_PIN 5
#define SERVO_CH12_PIN 18
#define SERVO_CH13_PIN 19
#define SERVO_CH14_PIN 21
#define SERVO_CH15_PIN 22
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

static void _set_angle(ledc_mode_t speed_mode, float angle)
{
    for (size_t i = 0; i < 8; i++) {
        iot_servo_write_angle(speed_mode, i, angle);
    }
}

TEST_CASE("Servo_motor test", "[servo][iot]")
{
    servo_config_t servo_cfg_ls = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
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
    TEST_ASSERT(ESP_OK == iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg_ls));

/**
 * Only ESP32 has the high speed mode
 */
#ifdef CONFIG_IDF_TARGET_ESP32
    servo_config_t servo_cfg_hs = {
        .max_angle = 180,
        .min_width_us = 500,
        .max_width_us = 2500,
        .freq = 100,
        .timer_number = LEDC_TIMER_0,
        .channels = {
            .servo_pin = {
                SERVO_CH8_PIN,
                SERVO_CH9_PIN,
                SERVO_CH10_PIN,
                SERVO_CH11_PIN,
                SERVO_CH12_PIN,
                SERVO_CH13_PIN,
                SERVO_CH14_PIN,
                SERVO_CH15_PIN,
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
    TEST_ASSERT(ESP_OK == iot_servo_init(LEDC_HIGH_SPEED_MODE, &servo_cfg_hs));
#endif

    size_t i;
    float angle_ls, angle_hs;
    for (i = 0; i <= 180; i++) {
        _set_angle(LEDC_LOW_SPEED_MODE, i);
#ifdef CONFIG_IDF_TARGET_ESP32
        _set_angle(LEDC_HIGH_SPEED_MODE, (180 - i));
#endif
        vTaskDelay(50 / portTICK_PERIOD_MS);
        iot_servo_read_angle(LEDC_LOW_SPEED_MODE, 0, &angle_ls);
#ifdef CONFIG_IDF_TARGET_ESP32
        iot_servo_read_angle(LEDC_HIGH_SPEED_MODE, 0, &angle_hs);
#endif

#ifdef CONFIG_IDF_TARGET_ESP32
        ESP_LOGI("servo", "[%d|%.2f], [%d|%.2f]", i, angle_ls, (180 - i), angle_hs);
#else
        ESP_LOGI("servo", "[%d|%.2f]", i, angle_ls);
        (void)angle_hs;
#endif
    }

    iot_servo_deinit(LEDC_LOW_SPEED_MODE);
#ifdef CONFIG_IDF_TARGET_ESP32
    iot_servo_deinit(LEDC_HIGH_SPEED_MODE);
#endif
}
