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
    ESP_LOGI(TAG, "KNOB%d: KNOB_LEFT Count is %d", get_knob_index((knob_handle_t)arg), iot_knob_get_count_value((knob_handle_t)arg));
}

static void knob_right_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_RIGHT, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_RIGHT Count is %d", get_knob_index((knob_handle_t)arg), iot_knob_get_count_value((knob_handle_t)arg));
}

static void knob_h_lim_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_H_LIM, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_H_LIM", get_knob_index((knob_handle_t)arg));
}

static void knob_l_lim_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_L_LIM, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_L_LIM", get_knob_index((knob_handle_t)arg));
}

static void knob_zero_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(KNOB_ZERO, iot_knob_get_event(arg));
    ESP_LOGI(TAG, "KNOB%d: KNOB_ZERO", get_knob_index((knob_handle_t)arg));
}

TEST_CASE("custom knob test", "[knob][iot]")
{
    knob_config_t *cfg = calloc(1, sizeof(knob_config_t));
    cfg->default_direction =0;
    cfg->gpio_encoder_a = GPIO_KNOB_A;
    cfg->gpio_encoder_b = GPIO_KNOB_B;

    for (int i = 0; i < KNOB_NUM; i++) {
        s_knob[i] = iot_knob_create(cfg);
        TEST_ASSERT_NOT_NULL(s_knob[i]);
        iot_knob_register_cb(s_knob[i], KNOB_LEFT, knob_left_cb, NULL);
        iot_knob_register_cb(s_knob[i], KNOB_RIGHT, knob_right_cb, NULL);
        iot_knob_register_cb(s_knob[i], KNOB_H_LIM, knob_h_lim_cb, NULL);
        iot_knob_register_cb(s_knob[i], KNOB_L_LIM, knob_l_lim_cb, NULL);
        iot_knob_register_cb(s_knob[i], KNOB_ZERO, knob_zero_cb, NULL);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    for (int i = 0; i < KNOB_NUM; i++) {
        iot_knob_delete(s_knob[i]);
    }
}