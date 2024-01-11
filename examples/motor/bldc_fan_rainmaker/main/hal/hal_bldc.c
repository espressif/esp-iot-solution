/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
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

#define PI 3.14159265f

#define LIMIT(VALUE, MIN, MAX) \
    ((VALUE) < (MIN) ? (MIN) : ((VALUE) > (MAX) ? (MAX) : (VALUE)))

static bldc_control_handle_t bldc_control_handle = NULL;

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

esp_err_t hal_bldc_init(dir_enum_t direction)
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
