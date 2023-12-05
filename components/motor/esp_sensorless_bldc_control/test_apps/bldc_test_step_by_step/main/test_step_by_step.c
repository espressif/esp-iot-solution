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
#include "bldc_user_cfg.h"
#include "bldc_control_param.h"

#define SPEED_CLOSED_LOOP 1

static const char *TAG = "test_adc";

bldc_control_handle_t bldc_control_handle = NULL;

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
#if SPEED_CLOSED_LOOP == 1
    static int speed_rpm = 200;
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

typedef struct {
    gptimer_handle_t gptimer;
    SemaphoreHandle_t xSemaphore;     /*!< semaphore handle */
    control_param_t control_param;
    control_mode_t control_mode;      /*!< control mode */
    speed_mode_t speed_mode;          /*!< speed mode */
    pid_ctrl_block_handle_t pid;
    uint8_t (*change_phase)(void *handle);
    void *change_phase_handle;
    uint8_t (*zero_cross)(void *handle);
    void *zero_cross_handle;
    esp_err_t (*control_operation)(void *handle);
    int delayCnt;                     /*!< Delay count, each plus one is an interrupt function time */
} bldc_control_t;

typedef esp_err_t (*debug_operation)(void *handle);

static esp_err_t motor_init(debug_operation operation)
{
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

    bldc_zero_cross_adc_config_t zero_cross_adc_config = {
        .adc_handle = NULL,
        .adc_unit = ADC_UNIT_1,
        .chan_cfg = {
            .bitwidth = ADC_BITWIDTH_12,
            .atten = ADC_ATTEN_DB_0,
        },
        .adc_channel = {ADC_CHANNEL_3, ADC_CHANNEL_4, ADC_CHANNEL_5, ADC_CHANNEL_0, ADC_CHANNEL_1},
    };

    bool if_debug = false;
    if (operation != NULL) {
        if_debug = true;
    }

    bldc_control_config_t config = {
        .speed_mode = SPEED_CLOSED_LOOP,
        .control_mode = BLDC_SIX_STEP,
        .alignment_mode = ALIGNMENT_ADC,
        .six_step_config = {
            .lower_switch_active_level = 0,
            .upper_switch_config = upper_switch_config,
            .lower_switch_config = lower_switch_config,
            .mos_en_config.has_enable = false,
        },
        .zero_cross_adc_config = zero_cross_adc_config,
        .debug_config = {
            .if_debug = if_debug,
            .debug_operation = operation,
        },
    };

    TEST_ASSERT(ESP_OK == bldc_control_init(&bldc_control_handle, &config));
    return ESP_OK;
}

static esp_err_t debug_operation_inject(void *args)
{
    bldc_control_t *control = (bldc_control_t *)args;

    switch (control->control_param.status) {
    case INJECT:
        if (--control->delayCnt <= 0) {
            if (control->change_phase(control->change_phase_handle) == 1) {
                control->control_param.status = ALIGNMENT;
            }
            control->delayCnt = control->control_param.charge_time;
        }
        break;
    default:
        break;
    }

    return ESP_OK;
}

TEST_CASE("1. bldc test inject ", "[inject]")
{
    /*!< Please enable INJECT_ENABLE first */
    esp_log_level_set("bldc_six_step", ESP_LOG_DEBUG);
    esp_event_loop_create_default();
    motor_init(&debug_operation_inject);

    while (1) {
        TEST_ASSERT(ESP_OK == bldc_control_start(bldc_control_handle, 300));
        vTaskDelay(300 / portTICK_PERIOD_MS);
        TEST_ASSERT(ESP_OK == bldc_control_stop(bldc_control_handle));
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static esp_err_t debug_operation_six_step(void *args)
{
    bldc_control_t *control = (bldc_control_t *)args;

    if (control->control_param.status == INJECT) {
        /*!< Forces a jump to ALIGNMENT */
        control->control_param.status = ALIGNMENT;
    }

    switch (control->control_param.status) {
    case ALIGNMENT:
        control->change_phase(control->change_phase_handle);
        control->control_param.status = DRAG;
        /*!< To fix strong dragging to a certain phase, a delay of ALIGNMENTNMS is needed. */
        control->delayCnt = ALIGNMENTNMS;
        break;
    case DRAG:
        if (--control->delayCnt <= 0) {
            control->change_phase(control->change_phase_handle);
            control->delayCnt = control->control_param.drag_time;
        }

        if (control->control_param.filter_failed_count > 15000) {
            control->control_param.filter_failed_count = 0;
            control->control_param.speed_rpm = 0;
            control->control_param.duty = 0;
            /*!< Motor blocked */
            control->control_param.status = BLOCKED;
        }
        break;
    default:
        break;
    }

    return ESP_OK;
}

TEST_CASE("2. bldc test six step", "[six step]")
{
    esp_event_loop_create_default();
    motor_init(&debug_operation_six_step);
    TEST_ASSERT(ESP_OK == bldc_control_set_dir(bldc_control_handle, CCW));
    TEST_ASSERT(ESP_OK == bldc_control_start(bldc_control_handle, 300));

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

TEST_CASE("3. bldc test close loop", "[close loop]")
{
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(BLDC_CONTROL_EVENT, ESP_EVENT_ANY_ID, &bldc_control_event_handler, NULL));
    motor_init(NULL);
    TEST_ASSERT(ESP_OK == bldc_control_set_dir(bldc_control_handle, CCW));
    TEST_ASSERT(ESP_OK == bldc_control_start(bldc_control_handle, 400));

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
