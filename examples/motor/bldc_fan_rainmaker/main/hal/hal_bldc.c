/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "math.h"
#include "app_variable.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "hal_bldc.h"
#include "iot_button.h"
#include "esp_timer.h"
#include "bldc_fan_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_rainmaker.h"
#include "esp_rmaker_core.h"

#define PI 3.14159265f

#define LIMIT(VALUE, MIN, MAX) \
    ((VALUE) < (MIN) ? (MIN) : ((VALUE) > (MAX) ? (MAX) : (VALUE)))

static bldc_control_handle_t bldc_control_handle = NULL;
const static char *TAG = "hal_bldc";

static void hal_bldc_button_ctrl(void *arg, void *data)
{
    motor_parameter.is_start = !motor_parameter.is_start;
}

static int hal_bldc_natural_speed(int min_speed, int max_speed, float noise_level, int t)
{
    double k = (1 + sin(t * 2 * PI / 16)) / 2;
    if (k >= noise_level) {
        k = noise_level;
    } else if (k <= 1 - noise_level) {
        k = 1 - noise_level;
    }
    k = (k - (1 - noise_level)) / (noise_level - (1 - noise_level));
    int speed = k * (max_speed - min_speed) + min_speed;
    return speed;
}

static void hal_bldc_timer_cb(void *args)
{
    static int t = 0;
    if (++t > 15) {
        t = 0;
    }
    if (motor_parameter.is_start && motor_parameter.is_natural) {
        motor_parameter.target_speed = hal_bldc_natural_speed(motor_parameter.min_speed, motor_parameter.max_speed - 200, 0.8, t);
        bldc_control_set_speed_rpm(bldc_control_handle, motor_parameter.target_speed);
    }
}

esp_err_t hal_bldc_button_ctrl_init(gpio_num_t pin)
{
    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = pin,
            .active_level = 0,
        },
    };
    button_handle_t btn = iot_button_create(&cfg);

    if (iot_button_register_cb(btn, BUTTON_PRESS_DOWN, hal_bldc_button_ctrl, &btn) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void bldc_control_event_handler(void *arg, esp_event_base_t event_base,
                                       int32_t event_id, void *event_data)
{
    esp_rmaker_param_t *rmaker_param = NULL;
    switch (event_id) {
    case BLDC_CONTROL_START:
        ESP_LOGI(TAG, "BLDC_CONTROL_START");
        break;
    case BLDC_CONTROL_STOP:
        ESP_LOGI(TAG, "BLDC_CONTROL_STOP");
        motor_parameter.is_start = false;
        motor_parameter.target_speed = BLDC_MIN_SPEED;
        break;
    case BLDC_CONTROL_ALIGNMENT:
        ESP_LOGI(TAG, "BLDC_CONTROL_ALIGNMENT");
        break;
    case BLDC_CONTROL_CLOSED_LOOP:
        motor_parameter.is_start = true;
        ESP_LOGI(TAG, "BLDC_CONTROL_CLOSED_LOOP");
        break;
    case BLDC_CONTROL_DRAG:
        ESP_LOGI(TAG, "BLDC_CONTROL_DRAG");
        break;
    case BLDC_CONTROL_BLOCKED:
        ESP_LOGI(TAG, "BLDC_CONTROL_BLOCKED");
        motor_parameter.is_start = false;
        hal_bldc_start_stop(0);
        if (++motor_parameter.restart_count < BLDC_MAX_RESTART_COUNT) {
            vTaskDelay(500 / portTICK_PERIOD_MS);
            motor_parameter.target_speed = BLDC_MIN_SPEED;
            hal_bldc_start_stop(1);
            motor_parameter.is_start = true;
        }
        break;
    }

    rmaker_param = app_rainmaker_get_param("Power");
    if (rmaker_param != NULL) {
        esp_rmaker_param_val_t val = {0};
        val.val.b = motor_parameter.is_start;
        val.type = RMAKER_VAL_TYPE_BOOLEAN;
        esp_rmaker_param_update_and_report(rmaker_param, val);        /*!< Update current motor start status */
    }

    rmaker_param = app_rainmaker_get_param("Speed");
    if (rmaker_param != NULL) {
        esp_rmaker_param_val_t val = {0};
        val.val.i = motor_parameter.target_speed;
        val.type = RMAKER_VAL_TYPE_INTEGER;
        esp_rmaker_param_update_and_report(rmaker_param, val);        /*!< Update current motor speed */
    }
}

esp_err_t hal_bldc_init(dir_enum_t direction)
{
    esp_event_loop_create_default();
    ESP_ERROR_CHECK(esp_event_handler_register(BLDC_CONTROL_EVENT, ESP_EVENT_ANY_ID, &bldc_control_event_handler, NULL));

    switch_config_t_t upper_switch_config = {
        .control_type = CONTROL_TYPE_MCPWM,
        .bldc_mcpwm = {
            .group_id = 0,
            .gpio_num = {UPPER_SWITCH_PIN_U, UPPER_SWITCH_PIN_V, UPPER_SWITCH_PIN_W},
        },
    };

    switch_config_t_t lower_switch_config = {
        .control_type = CONTROL_TYPE_GPIO,
        .bldc_gpio[0] = {
            .gpio_num = LOWER_SWITCH_PIN_U,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[1] = {
            .gpio_num = LOWER_SWITCH_PIN_V,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
        .bldc_gpio[2] = {
            .gpio_num = LOWER_SWITCH_PIN_W,
            .gpio_mode = GPIO_MODE_OUTPUT,
        },
    };

    bldc_zero_cross_comparer_config_t zero_cross_comparer_config = {
        .comparer_gpio[0] = {
            .gpio_num = COMPARER_PIN_U,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
        .comparer_gpio[1] = {
            .gpio_num = COMPARER_PIN_V,
            .gpio_mode = GPIO_MODE_INPUT,
            .active_level = 0,
        },
        .comparer_gpio[2] = {
            .gpio_num = COMPARER_PIN_W,
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

    if (bldc_control_init(&bldc_control_handle, &config) != ESP_OK) {
        return ESP_FAIL;
    }

    if (bldc_control_set_dir(bldc_control_handle, direction) != ESP_OK) {
        return ESP_FAIL;
    }

    if (hal_bldc_button_ctrl_init(GPIO_NUM_0) != ESP_OK) {
        return ESP_FAIL;
    }

    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &hal_bldc_timer_cb,
        .name = "periodic"
    };
    esp_timer_handle_t periodic_bldc_timer;
    if (esp_timer_create(&periodic_timer_args, &periodic_bldc_timer) != ESP_OK) {
        return ESP_FAIL;
    }

    if (esp_timer_start_periodic(periodic_bldc_timer, 1000 * 1000) != ESP_OK) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t hal_bldc_start_stop(uint8_t status)
{
    if (status == 0) {
        return bldc_control_stop(bldc_control_handle);
    } else {
        return bldc_control_start(bldc_control_handle, motor_parameter.target_speed);
    }
    return ESP_OK;
}

esp_err_t hal_bldc_set_speed(uint16_t speed)
{
    speed = LIMIT(speed, motor_parameter.min_speed, motor_parameter.max_speed);
    return bldc_control_set_speed_rpm(bldc_control_handle, speed);
}
