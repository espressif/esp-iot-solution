/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
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

#define SERVO_CHANNEL_NUM 4
#define TEST_MEMORY_LEAK_THRESHOLD (-500)

static size_t before_free_8bit;
static size_t before_free_32bit;

static void _set_angle(servo_handle_t *servos, float angle)
{
    for (size_t i = 0; i < SERVO_CHANNEL_NUM; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, iot_servo_write_angle(servos[i], angle));
    }
}

TEST_CASE("Servo_motor test", "[servo][iot]")
{
    const gpio_num_t servo_pins[SERVO_CHANNEL_NUM] = {
        SERVO_CH0_PIN,
        SERVO_CH1_PIN,
        SERVO_CH2_PIN,
        SERVO_CH3_PIN,
    };
    const ledc_channel_t servo_channels[SERVO_CHANNEL_NUM] = {
        LEDC_CHANNEL_0,
        LEDC_CHANNEL_1,
        LEDC_CHANNEL_2,
        LEDC_CHANNEL_3,
    };
    servo_handle_t servos[SERVO_CHANNEL_NUM] = { NULL };
    servo_config_t servo_cfg = SERVO_CONFIG_DEFAULT(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_0, SERVO_CH0_PIN);

    for (size_t i = 0; i < SERVO_CHANNEL_NUM; i++) {
        servo_cfg.gpio_num = servo_pins[i];
        servo_cfg.channel = servo_channels[i];
        TEST_ASSERT_EQUAL(ESP_OK, iot_servo_new(&servo_cfg, &servos[i]));
    }

    float angle_ls;
    for (size_t i = 0; i <= 180; i++) {
        _set_angle(servos, i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        TEST_ASSERT_EQUAL(ESP_OK, iot_servo_read_angle(servos[0], &angle_ls));
        TEST_ASSERT_FLOAT_WITHIN(0.5f, (float)i, angle_ls);
        ESP_LOGI("servo", "[%d|%.2f]", i, angle_ls);
    }

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_write_angle(servos[0], -1.0f));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_write_angle(servos[0], 181.0f));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_read_angle(servos[0], NULL));

    for (size_t i = 0; i < SERVO_CHANNEL_NUM; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, iot_servo_del(servos[i]));
    }
}

TEST_CASE("Servo_motor invalid argument test", "[servo][iot]")
{
    servo_handle_t servo = NULL;
    float angle = 0;
    servo_config_t servo_cfg = SERVO_CONFIG_DEFAULT(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_0, SERVO_CH0_PIN);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_new(NULL, &servo));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_new(&servo_cfg, NULL));

    servo_cfg.freq = 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_new(&servo_cfg, &servo));
    servo_cfg.freq = 50;

    servo_cfg.gpio_num = GPIO_NUM_NC;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_new(&servo_cfg, &servo));
    servo_cfg.gpio_num = SERVO_CH0_PIN;

    servo_cfg.max_width_us = servo_cfg.min_width_us;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_new(&servo_cfg, &servo));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_del(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_write_angle(NULL, 0));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, iot_servo_read_angle(NULL, &angle));
}

TEST_CASE("Servo_motor ledc resource conflict test", "[servo][iot]")
{
    servo_handle_t servo0 = NULL;
    servo_handle_t servo1 = NULL;
    servo_config_t servo_cfg = SERVO_CONFIG_DEFAULT(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, LEDC_CHANNEL_0, SERVO_CH0_PIN);

    TEST_ASSERT_EQUAL(ESP_OK, iot_servo_new(&servo_cfg, &servo0));

    servo_cfg.gpio_num = SERVO_CH1_PIN;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, iot_servo_new(&servo_cfg, &servo1));

    servo_cfg.channel = LEDC_CHANNEL_1;
    servo_cfg.freq = 100;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, iot_servo_new(&servo_cfg, &servo1));

    servo_cfg.freq = 50;
    TEST_ASSERT_EQUAL(ESP_OK, iot_servo_new(&servo_cfg, &servo1));

    TEST_ASSERT_EQUAL(ESP_OK, iot_servo_del(servo0));
    TEST_ASSERT_EQUAL(ESP_OK, iot_servo_del(servo1));
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
