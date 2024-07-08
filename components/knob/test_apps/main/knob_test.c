/*
 * SPDX-FileCopyrightText: 2016-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_knob.h"
#include "sdkconfig.h"

static const char *TAG = "KNOB TEST";

#define TEST_MEMORY_LEAK_THRESHOLD (-400)

#define GPIO_KNOB_A 1
#define GPIO_KNOB_B 2
#define KNOB_NUM    3

static knob_handle_t s_knob[KNOB_NUM] = {0};

static int get_knob_index(knob_handle_t knob)
{
    for (size_t i = 0; i < KNOB_NUM; i++) {
        if (knob == s_knob[i]) {
            return i;
        }
    }
    return -1;
}

static void knob_left_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_LEFT, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB %d: KNOB_LEFT Count is %d usr_data: %s", get_knob_index((knob_handle_t)arg), iot_knob_get_count_value((knob_handle_t)arg), (char *)data);
}

static void knob_right_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_RIGHT, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_RIGHT Count is %d usr_data: %s", get_knob_index((knob_handle_t)arg), iot_knob_get_count_value((knob_handle_t)arg), (char *)data);
}

static void knob_h_lim_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_H_LIM, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_H_LIM usr_data: %s", get_knob_index((knob_handle_t)arg), (char *)data);
}

static void knob_l_lim_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_L_LIM, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_L_LIM usr_data: %s", get_knob_index((knob_handle_t)arg), (char *)data);
}

static void knob_zero_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_ZERO, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_ZERO usr_data: %s", get_knob_index((knob_handle_t)arg), (char *)data);
}

static const char *knob_name[] = {"knob_0",
                                  "knob_1",
                                  "knob_2"
                                 };

TEST_CASE("three knob test", "[knob][iot]")
{
    knob_config_t *cfg = calloc(1, sizeof(knob_config_t));
    cfg->default_direction = 0;
    cfg->gpio_encoder_a = GPIO_KNOB_A;
    cfg->gpio_encoder_b = GPIO_KNOB_B;

    for (int i = 0; i < KNOB_NUM; i++) {
        s_knob[i] = iot_knob_create(cfg);
        TEST_ASSERT_NOT_NULL(s_knob[i]);
        iot_knob_register_cb(s_knob[i], KNOB_LEFT, knob_left_cb, (void *)knob_name[i]);
        iot_knob_register_cb(s_knob[i], KNOB_RIGHT, knob_right_cb, (void *)knob_name[i]);
        iot_knob_register_cb(s_knob[i], KNOB_H_LIM, knob_h_lim_cb, (void *)knob_name[i]);
        iot_knob_register_cb(s_knob[i], KNOB_L_LIM, knob_l_lim_cb, (void *)knob_name[i]);
        iot_knob_register_cb(s_knob[i], KNOB_ZERO, knob_zero_cb, (void *)knob_name[i]);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    for (int i = 0; i < KNOB_NUM; i++) {
        iot_knob_delete(s_knob[i]);
    }
}

TEST_CASE("one knob test", "[knob][iot]")
{
    knob_config_t *cfg = calloc(1, sizeof(knob_config_t));
    cfg->default_direction = 0;
    cfg->gpio_encoder_a = GPIO_KNOB_A;
    cfg->gpio_encoder_b = GPIO_KNOB_B;

    s_knob[0] = iot_knob_create(cfg);
    TEST_ASSERT_NOT_NULL(s_knob[0]);
    iot_knob_register_cb(s_knob[0], KNOB_LEFT, knob_left_cb, (void *)knob_name[0]);
    iot_knob_register_cb(s_knob[0], KNOB_RIGHT, knob_right_cb, (void *)knob_name[0]);
    iot_knob_register_cb(s_knob[0], KNOB_H_LIM, knob_h_lim_cb, (void *)knob_name[0]);
    iot_knob_register_cb(s_knob[0], KNOB_L_LIM, knob_l_lim_cb, (void *)knob_name[0]);
    iot_knob_register_cb(s_knob[0], KNOB_ZERO, knob_zero_cb, (void *)knob_name[0]);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_knob_delete(s_knob[0]);
}

static size_t before_free_8bit;
static size_t before_free_32bit;

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
    printf("IOT KNOB TEST\n");
    unity_run_menu();
}
