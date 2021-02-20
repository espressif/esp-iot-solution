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

#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "unity.h"
#include "iot_button.h"

static const char *TAG = "BUTTON TEST";

#define BUTTON_IO_NUM  0
#define BUTTON_ACTIVE_LEVEL   0
#define BUTTON_NUM 16

static button_handle_t g_btns[BUTTON_NUM] = {0};

static int get_btn_index(button_handle_t btn)
{
    for (size_t i = 0; i < BUTTON_NUM; i++) {
        if (btn == g_btns[i]) {
            return i;
        }
    }
    return -1;
}

static void button_press_down_cb(void *arg)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_PRESS_DOWN, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_DOWN", get_btn_index((button_handle_t)arg));
}

static void button_press_up_cb(void *arg)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_PRESS_UP, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_UP", get_btn_index((button_handle_t)arg));
}

static void button_press_repeat_cb(void *arg)
{
    ESP_LOGI(TAG, "BTN%d: BUTTON_PRESS_REPEAT[%d]", get_btn_index((button_handle_t)arg), iot_button_get_repeat((button_handle_t)arg));
}

static void button_single_click_cb(void *arg)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_SINGLE_CLICK, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_SINGLE_CLICK", get_btn_index((button_handle_t)arg));
}

static void button_double_click_cb(void *arg)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_DOUBLE_CLICK, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_DOUBLE_CLICK", get_btn_index((button_handle_t)arg));
}

static void button_long_press_start_cb(void *arg)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_LONG_PRESS_START, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_LONG_PRESS_START", get_btn_index((button_handle_t)arg));
}

static void button_long_press_hold_cb(void *arg)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_LONG_PRESS_HOLD, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BTN%d: BUTTON_LONG_PRESS_HOLD", get_btn_index((button_handle_t)arg));
}

static void print_button_event(button_handle_t btn)
{
    button_event_t evt = iot_button_get_event(btn);
    switch (evt) {
    case BUTTON_PRESS_DOWN:
        ESP_LOGI(TAG, "BUTTON_PRESS_DOWN");
        break;
    case BUTTON_PRESS_UP:
        ESP_LOGI(TAG, "BUTTON_PRESS_UP");
        break;
    case BUTTON_PRESS_REPEAT:
        ESP_LOGI(TAG, "BUTTON_PRESS_REPEAT");
        break;
    case BUTTON_SINGLE_CLICK:
        ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
        break;
    case BUTTON_DOUBLE_CLICK:
        ESP_LOGI(TAG, "BUTTON_DOUBLE_CLICK");
        break;
    case BUTTON_LONG_PRESS_START:
        ESP_LOGI(TAG, "BUTTON_LONG_PRESS_START");
        break;
    case BUTTON_LONG_PRESS_HOLD:
        ESP_LOGI(TAG, "BUTTON_LONG_PRESS_HOLD");
        break;

    default:
        break;
    }
}

TEST_CASE("gpio button test", "[button][iot]")
{
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    g_btns[0] = iot_button_create(&cfg);
    TEST_ASSERT_NOT_NULL(g_btns[0]);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_DOWN, button_press_down_cb);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_UP, button_press_up_cb);
    iot_button_register_cb(g_btns[0], BUTTON_PRESS_REPEAT, button_press_repeat_cb);
    iot_button_register_cb(g_btns[0], BUTTON_SINGLE_CLICK, button_single_click_cb);
    iot_button_register_cb(g_btns[0], BUTTON_DOUBLE_CLICK, button_double_click_cb);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_START, button_long_press_start_cb);
    iot_button_register_cb(g_btns[0], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    iot_button_delete(g_btns[0]);
}

TEST_CASE("adc button test", "[button][iot]")
{
    /** ESP32-LyraT-Mini board */
    const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};
    button_config_t cfg = {0};
    cfg.type = BUTTON_TYPE_ADC;
    for (size_t i = 0; i < 6; i++) {
        cfg.adc_button_config.adc_channel = 3,
        cfg.adc_button_config.button_index = i;
        if (i == 0) {
            cfg.adc_button_config.min = (0 + vol[i]) / 2;
        } else {
            cfg.adc_button_config.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5) {
            cfg.adc_button_config.max = (vol[i] + 3000) / 2;
        } else {
            cfg.adc_button_config.max = (vol[i] + vol[i + 1]) / 2;
        }

        g_btns[i] = iot_button_create(&cfg);
        TEST_ASSERT_NOT_NULL(g_btns[i]);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_DOWN, button_press_down_cb);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_UP, button_press_up_cb);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT, button_press_repeat_cb);
        iot_button_register_cb(g_btns[i], BUTTON_SINGLE_CLICK, button_single_click_cb);
        iot_button_register_cb(g_btns[i], BUTTON_DOUBLE_CLICK, button_double_click_cb);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_START, button_long_press_start_cb);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    for (size_t i = 0; i < 6; i++) {
        iot_button_delete(g_btns[i]);
    }
}

TEST_CASE("adc gpio button test", "[button][iot]")
{
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    g_btns[8] = iot_button_create(&cfg);
    TEST_ASSERT_NOT_NULL(g_btns[8]);
    iot_button_register_cb(g_btns[8], BUTTON_PRESS_DOWN, button_press_down_cb);
    iot_button_register_cb(g_btns[8], BUTTON_PRESS_UP, button_press_up_cb);
    iot_button_register_cb(g_btns[8], BUTTON_PRESS_REPEAT, button_press_repeat_cb);
    iot_button_register_cb(g_btns[8], BUTTON_SINGLE_CLICK, button_single_click_cb);
    iot_button_register_cb(g_btns[8], BUTTON_DOUBLE_CLICK, button_double_click_cb);
    iot_button_register_cb(g_btns[8], BUTTON_LONG_PRESS_START, button_long_press_start_cb);
    iot_button_register_cb(g_btns[8], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb);

    /** ESP32-LyraT-Mini board */
    const uint16_t vol[6] = {380, 820, 1180, 1570, 1980, 2410};
    // button_config_t cfg = {0};
    cfg.type = BUTTON_TYPE_ADC;
    for (size_t i = 0; i < 6; i++) {
        cfg.adc_button_config.adc_channel = 3,
        cfg.adc_button_config.button_index = i;
        if (i == 0) {
            cfg.adc_button_config.min = (0 + vol[i]) / 2;
        } else {
            cfg.adc_button_config.min = (vol[i - 1] + vol[i]) / 2;
        }

        if (i == 5) {
            cfg.adc_button_config.max = (vol[i] + 3000) / 2;
        } else {
            cfg.adc_button_config.max = (vol[i] + vol[i + 1]) / 2;
        }

        g_btns[i] = iot_button_create(&cfg);
        TEST_ASSERT_NOT_NULL(g_btns[i]);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_DOWN, button_press_down_cb);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_UP, button_press_up_cb);
        iot_button_register_cb(g_btns[i], BUTTON_PRESS_REPEAT, button_press_repeat_cb);
        iot_button_register_cb(g_btns[i], BUTTON_SINGLE_CLICK, button_single_click_cb);
        iot_button_register_cb(g_btns[i], BUTTON_DOUBLE_CLICK, button_double_click_cb);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_START, button_long_press_start_cb);
        iot_button_register_cb(g_btns[i], BUTTON_LONG_PRESS_HOLD, button_long_press_hold_cb);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    for (size_t i = 0; i < 6; i++) {
        iot_button_delete(g_btns[i]);
    }
}