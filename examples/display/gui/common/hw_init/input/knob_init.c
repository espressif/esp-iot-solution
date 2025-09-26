/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hw_init.h"

#if HW_USE_ENCODER

#include <stdbool.h>
#include "esp_log.h"
#include "iot_knob.h"
#include "iot_button.h"
#include "button_gpio.h"

#define HW_ENCODER_A         (GPIO_NUM_10)
#define HW_ENCODER_B         (GPIO_NUM_6)
#define HW_ENCODER_ENTER     (GPIO_NUM_9)

static const char *TAG = "hw_knob_init";

static const knob_config_t hw_knob_encoder_config = {
    .default_direction = 0,
    .gpio_encoder_a = HW_ENCODER_A,
    .gpio_encoder_b = HW_ENCODER_B,
    .enable_power_save = false,
};

static button_handle_t s_encoder_button_handle = NULL;

const knob_config_t *hw_knob_get_config(void)
{
    return &hw_knob_encoder_config;
}

button_handle_t hw_knob_get_button(void)
{
    if (s_encoder_button_handle == NULL) {
        ESP_LOGI(TAG, "Initializing encoder button on GPIO %d", HW_ENCODER_ENTER);

        const button_config_t btn_cfg = {
            .long_press_time = 0,
            .short_press_time = 0,
        };

        const button_gpio_config_t btn_gpio_cfg = {
            .gpio_num = HW_ENCODER_ENTER,
            .active_level = 0,
            .enable_power_save = false,
            .disable_pull = false,
        };

        esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &s_encoder_button_handle);
        if (ret != ESP_OK || s_encoder_button_handle == NULL) {
            ESP_LOGE(TAG, "Failed to create encoder button: %d", ret);
            return NULL;
        }

        ESP_LOGI(TAG, "Encoder button initialized successfully");
    }

    return s_encoder_button_handle;
}

#endif
