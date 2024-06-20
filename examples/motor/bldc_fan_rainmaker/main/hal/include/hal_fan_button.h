/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETTING_START = 0,
    SETTING_MODE,
    SETTING_SHAKING_HEAD,
    SETTING_TIME
} hal_fan_setting_t;

typedef struct {
    gpio_num_t pin[4];
    esp_timer_handle_t fan_oneshot_timer;
    uint8_t fan_timing_count;
} hal_fan_button_t;

/**
 * @brief Initializes the fan button
 *
 * @param start_pin GPIO pin number
 * @param mode_pin GPIO pin number
 * @param shacking_head_pin GPIO pin number
 * @param timer_pin GPIO pin number
 * @return
 *    - ESP_OK: Success in initializing the fan button
 *    - ESP_FAIL: GPIO resource is not available
 */
esp_err_t hal_fan_button_init(gpio_num_t start_pin, gpio_num_t mode_pin, gpio_num_t shacking_head_pin, gpio_num_t timer_pin);

#ifdef __cplusplus
}
#endif
