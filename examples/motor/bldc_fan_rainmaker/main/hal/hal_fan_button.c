/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "string.h"
#include "hal_fan_button.h"
#include "hal_stepper_motor.h"
#include "esp_rmaker_core.h"
#include "hal_bldc.h"
#include "app_variable.h"
#include "app_rainmaker.h"
#include "iot_button.h"
#include "esp_log.h"
#include "bldc_fan_config.h"

#define SIGNLE_TIMING_DURATION SIGNLE_TIMING_HOUR_DURACTION*60*60*1000*1000UL   /*!<  Converting hours to us*/
hal_fan_button_t hal_fan_button;
static button_handle_t hal_fan_button_handle[4] = {0};
static const char *TAG = "fan button";

static int hal_fan_get_button_index(button_handle_t btn)
{
    for (size_t i = 0; i < 4; i++) {
        if (btn == hal_fan_button_handle[i]) {
            return i;
        }
    }
    return -1;
}

static void hal_fan_button_signle_click_cb(void *arg, void *data)
{
    static int bldc_speed[3] = {BLDC_MIN_SPEED, BLDC_MID_SPEED, BLDC_HIGH_SPEED};
    esp_rmaker_param_t *rmaker_param = NULL;
    if (iot_button_get_event(arg) == BUTTON_SINGLE_CLICK) {
        switch (hal_fan_get_button_index((button_handle_t)arg)) {
        case SETTING_START:
            motor_parameter.start_count++;
            ESP_LOGI(TAG, "Setting start, count:%d", motor_parameter.start_count);

            if (!motor_parameter.is_start) {
                motor_parameter.target_speed = BLDC_MIN_SPEED;
                hal_bldc_start_stop(1);
                motor_parameter.restart_count = 0;
                motor_parameter.start_count = 1;
            } else if (motor_parameter.is_start && motor_parameter.start_count == 4) {
                hal_bldc_start_stop(0);
                motor_parameter.start_count = 0;
            } else {
                hal_bldc_set_speed(bldc_speed[motor_parameter.start_count - 1]);
                motor_parameter.target_speed = bldc_speed[motor_parameter.start_count - 1];
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
            break;
        case SETTING_MODE:
            ESP_LOGI(TAG, "Setting mode");
            motor_parameter.is_natural = !motor_parameter.is_natural;
            rmaker_param = app_rainmaker_get_param("Natural");
            if (rmaker_param != NULL) {
                esp_rmaker_param_val_t val = {0};
                val.val.b = motor_parameter.is_natural;
                val.type = RMAKER_VAL_TYPE_BOOLEAN;
                esp_rmaker_param_update_and_report(rmaker_param, val);        /*!< Update current motor mode */
            }
            break;
        case SETTING_SHAKING_HEAD:
            ESP_LOGI(TAG, "Setting shaking head");
            stepper_motor.is_start = !stepper_motor.is_start;
            rmaker_param = app_rainmaker_get_param("Shake");
            if (rmaker_param != NULL) {
                esp_rmaker_param_val_t val = {0};
                val.val.b = stepper_motor.is_start;
                val.type = RMAKER_VAL_TYPE_BOOLEAN;
                esp_rmaker_param_update_and_report(rmaker_param, val);        /*!< Update current stepper motor start status */
            }
            break;
        case SETTING_TIME:
            if (++hal_fan_button.fan_timing_count >= MAXINUM_TIMING_COUNT) {
                hal_fan_button.fan_timing_count = 1;
            }
            ESP_LOGI(TAG, "Setting time:%lu us", hal_fan_button.fan_timing_count * SIGNLE_TIMING_DURATION);
            if (esp_timer_is_active(hal_fan_button.fan_oneshot_timer)) {
                esp_timer_stop(hal_fan_button.fan_oneshot_timer);             /*!< Shut down the original timer before starting the timer */
            }
            ESP_ERROR_CHECK(esp_timer_start_once(hal_fan_button.fan_oneshot_timer, hal_fan_button.fan_timing_count * SIGNLE_TIMING_DURATION));
            break;
        default:
            break;
        }
    }
}

static void hal_fan_button_long_press_hold_cb(void *arg, void *data)
{
    ESP_ERROR_CHECK(esp_rmaker_factory_reset(CONFIG_FAN_RAINMAKER_RESET_SECONDS, CONFIG_FAN_RAINMAKER_REBOOT_SECONDS));
}

static void hal_fan_oneshot_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Count is over");
    if (motor_parameter.is_start) {
        motor_parameter.is_start = ! motor_parameter.is_start;
        hal_bldc_start_stop(motor_parameter.is_start);                        /*!< Shut down the motor only when it is running */
    }
}

esp_err_t hal_fan_button_init(gpio_num_t start_pin, gpio_num_t mode_pin, gpio_num_t shacking_head_pin, gpio_num_t timer_pin)
{
    gpio_num_t pin[] = {start_pin, mode_pin, shacking_head_pin, timer_pin};

    gpio_config_t io_config = {
        .pin_bit_mask = (1ULL << start_pin) | (1ULL << mode_pin) | (1ULL << shacking_head_pin) | (1ULL << timer_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    memcpy(hal_fan_button.pin, pin, sizeof(pin));

    if (gpio_config(&io_config) != ESP_OK) {
        return ESP_FAIL;
    }

    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_FAN_RAINMAKER_LONG_PRESS_RESET_SECONDS * 1000,
        .short_press_time = 200,
        .gpio_button_config = {
            .gpio_num = pin[0],
            .active_level = 0,
        },
    };

    for (int i = 0; i < 4; i++) {
        cfg.gpio_button_config.gpio_num = pin[i];
        hal_fan_button_handle[i] = iot_button_create(&cfg);
        iot_button_register_cb(hal_fan_button_handle[i], BUTTON_SINGLE_CLICK, hal_fan_button_signle_click_cb, NULL);
    }

    iot_button_register_cb(hal_fan_button_handle[SETTING_START], BUTTON_LONG_PRESS_HOLD, hal_fan_button_long_press_hold_cb, NULL);  /*!< Support reset rainmaker */

    hal_fan_button.fan_timing_count = 0;
    const esp_timer_create_args_t oneshot_timer_args = {
        .callback = &hal_fan_oneshot_timer_cb,
    };
    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &hal_fan_button.fan_oneshot_timer));

    return ESP_OK;
}
