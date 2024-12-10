/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_servo.h"
#include "unity.h"
#include "unity_config.h"
#include "sdkconfig.h"

#define SERVO_CH0_PIN 1
#define SERVO_CH1_PIN 2
#define SERVO_CH2_PIN 3
#define SERVO_CH3_PIN 4

#define TEST_MEMORY_LEAK_THRESHOLD (-500)

static size_t before_free_8bit;
static size_t before_free_32bit;

static void _set_angle(ledc_mode_t speed_mode, float angle)
{
    for (size_t i = 0; i < 4; i++) {
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
            },
            .ch = {
                LEDC_CHANNEL_0,
                LEDC_CHANNEL_1,
                LEDC_CHANNEL_2,
                LEDC_CHANNEL_3,
            },
        },
        .channel_number = 4,
    } ;
    TEST_ASSERT(ESP_OK == iot_servo_init(LEDC_LOW_SPEED_MODE, &servo_cfg_ls));

    size_t i;
    float angle_ls, angle_hs;
    for (i = 0; i <= 180; i++) {
        _set_angle(LEDC_LOW_SPEED_MODE, i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        iot_servo_read_angle(LEDC_LOW_SPEED_MODE, 0, &angle_ls);
        ESP_LOGI("servo", "[%d|%.2f]", i, angle_ls);
        (void)angle_hs;
    }

    iot_servo_deinit(LEDC_LOW_SPEED_MODE);
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

void app_main(void)
{
    printf("SERVO TEST \n");
    unity_run_menu();
}
