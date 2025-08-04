/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "driver/ledc.h"
#include "led_indicator.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEDC_MODE       CONFIG_LEDC_SPEED_MODE_VALUE
#define LEDC_DUTY_RES   CONFIG_LEDC_TIMER_BIT_NUM
#define LEDC_FREQ_HZ    CONFIG_LEDC_TIMER_FREQ_HZ

typedef struct {
    bool is_active_level_high;        /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    bool timer_inited;                /*!< Set true if LEDC timer is inited, otherwise false. */
    ledc_timer_t timer_num;           /*!< The timer source of channel */
    int32_t red_gpio_num;             /*!< Red LED pwm gpio number */
    int32_t green_gpio_num;           /*!< Green LED pwm gpio number */
    int32_t blue_gpio_num;            /*!< Blue LED pwm gpio number */
    ledc_channel_t red_channel;       /*!< Red LED LEDC channel */
    ledc_channel_t green_channel;     /*!< Green LED LEDC channel */
    ledc_channel_t blue_channel;      /*!< Blue LED LEDC channel */
} led_indicator_rgb_config_t;

/**
 * @brief Create a new RGB LED device instance.
 *
 *
 * @param led_config Pointer to the general LED configuration structure.
 * @param rgb_cfg Pointer to the RGB-specific configuration structure.
 * @param handle pointer to LED indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_OK Success
 *     - ESP_FAIL Failed to initialize RGB mode or create LED indicator
 */
esp_err_t led_indicator_new_rgb_device(const led_indicator_config_t *led_config, const led_indicator_rgb_config_t *rgb_cfg, led_indicator_handle_t *handle);

#ifdef __cplusplus
}
#endif
