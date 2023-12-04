/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "test_utils.h"
#include "unity.h"
#include "bldc_control.h"
#include "iot_button.h"

#define CLOSED_LOOP 1

static const char *TAG = "test_comparer";

static void bldc_control_event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
    switch (event_id) {
    case BLDC_CONTROL_START:
        ESP_LOGI(TAG, "BLDC_CONTROL_START");
        break;
    case BLDC_CONTROL_STOP:
        ESP_LOGI(TAG, "BLDC_CONTROL_STOP");
        break;
    case BLDC_CONTROL_ALIGNMENT:
        ESP_LOGI(TAG, "BLDC_CONTROL_ALIGNMENT");
        break;
    case BLDC_CONTROL_CLOSED_LOOP:
        ESP_LOGI(TAG, "BLDC_CONTROL_CLOSED_LOOP");
        break;
    case BLDC_CONTROL_DRAG:
        ESP_LOGI(TAG, "BLDC_CONTROL_DRAG");
        break;
    case BLDC_CONTROL_BLOCKED:
        ESP_LOGI(TAG, "BLDC_CONTROL_BLOCKED");
        break;
    }
}

static void button_press_down_cb(void *arg, void *data)
{
    bldc_control_handle_t bldc_control_handle = (bldc_control_handle_t) data;
#if CLOSED_LOOP == 1
    static int speed_rpm = 200;
    ESP_LOGI(TAG, "speed_rpm:%d", speed_rpm);
    bldc_control_set_speed_rpm(bldc_control_handle, speed_rpm);
    speed_rpm += 50;
    if (speed_rpm > 1000) {
        speed_rpm = 200;
    }
#else
    static int duty = 400;

    duty += 30;
    if (duty > 1000) {
        duty = 400;
    }
    bldc_control_set_duty(bldc_control_handle, duty);
#endif
}

void button_init(void *user_data)
{
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = 0,
            .active_level = 0,
        },
    };
    button_handle_t btn = iot_button_create(&cfg);
    iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_press_down_cb, user_data);
}

TEST_CASE("bldc comparer ledc test", "[bldc comparer][ledc]")
{
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(BLDC_CONTROL_EVENT, ESP_EVENT_ANY_ID, &bldc_control_event_handler, NULL));

    switch_config_t_t upper_switch_config = {
        .control_type = CONTROL_TYPE_LEDC,
        .bldc_ledc = {
            .ledc_channel = {0, 1, 2},
            .gpio_num = {17, 16, 15},
        },
    };

    switch_config_t_t lower_switch_config = {
        .control_type = CONTROL_TYPE_GPIO,
        .bldc_gpio[0] = {
            .gpio_num = 12,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[1] = {
            .gpio_num = 11,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[2] = {
            .gpio_num = 10,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
    };

    bldc_zero_cross_comparer_config_t zero_cross_comparer_config = {
        .comparer_gpio[0] = {
            .gpio_num = 3,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
        .comparer_gpio[1] = {
            .gpio_num = 46,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
        .comparer_gpio[2] = {
            .gpio_num = 9,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
    };

    bldc_control_config_t config = {
        .speed_mode = SPEED_CLOSED_LOOP,
        .control_mode = BLDC_SIX_STEP,
        .alignment_mode = ALIGNMENT_COMPARER,
        .six_step_config = {
            .lower_switch_active_level = 0,
            .upper_switch_config = upper_switch_config,
            .lower_switch_config = lower_switch_config,
            .mos_en_config.has_enable = false,
        },
        .zero_cross_comparer_config = zero_cross_comparer_config,
    };

    bldc_control_handle_t bldc_control_handle = NULL;

    TEST_ASSERT(ESP_OK == bldc_control_init(&bldc_control_handle, &config));

    TEST_ASSERT(ESP_OK == bldc_control_set_dir(bldc_control_handle, CCW));

    TEST_ASSERT(ESP_OK == bldc_control_start(bldc_control_handle, 300));

    button_init(bldc_control_handle);

    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

#if CONFIG_SOC_MCPWM_SUPPORTED
TEST_CASE("bldc comparer test mcpwm", "[bldc comparer][mcpwm]")
{
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(BLDC_CONTROL_EVENT, ESP_EVENT_ANY_ID, &bldc_control_event_handler, NULL));
    switch_config_t_t upper_switch_config = {
        .control_type = CONTROL_TYPE_MCPWM,
        .bldc_mcpwm = {
            .group_id = 0,
            .gpio_num = {17, 16, 15},
        },
    };

    switch_config_t_t lower_switch_config = {
        .control_type = CONTROL_TYPE_GPIO,
        .bldc_gpio[0] = {
            .gpio_num = 12,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[1] = {
            .gpio_num = 11,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[2] = {
            .gpio_num = 10,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
    };

    bldc_zero_cross_comparer_config_t zero_cross_comparer_config = {
        .comparer_gpio[0] = {
            .gpio_num = 3,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
        .comparer_gpio[1] = {
            .gpio_num = 46,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
        .comparer_gpio[2] = {
            .gpio_num = 9,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
    };

    bldc_control_config_t config = {
        .speed_mode = SPEED_CLOSED_LOOP,
        .control_mode = BLDC_SIX_STEP,
        .alignment_mode = ALIGNMENT_COMPARER,
        .six_step_config = {
            .lower_switch_active_level = 0,
            .upper_switch_config = upper_switch_config,
            .lower_switch_config = lower_switch_config,
            .mos_en_config.has_enable = false,
        },
        .zero_cross_comparer_config = zero_cross_comparer_config,
    };

    bldc_control_handle_t bldc_control_handle = NULL;

    TEST_ASSERT(ESP_OK == bldc_control_init(&bldc_control_handle, &config));

    TEST_ASSERT(ESP_OK == bldc_control_set_dir(bldc_control_handle, CCW));

    TEST_ASSERT(ESP_OK == bldc_control_start(bldc_control_handle, 300));

    button_init(bldc_control_handle);

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
#endif
