/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "led_indicator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED GPIO configuration
 *
 */
typedef struct {
    bool is_active_level_high;     /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    int32_t gpio_num;              /*!< num of GPIO */
} led_indicator_gpio_config_t;

/**
 * @brief Create a new LED indicator device using a GPIO pin.
 *
 *
 * @param led_config   Pointer to the LED configuration structure.
 * @param gpio_cfg     Pointer to the GPIO configuration structure for the LED.
 * @param handle pointer to LED indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_OK Success
 *     - ESP_FAIL Failed to initialize GPIO mode or create LED indicator
 */
esp_err_t led_indicator_new_gpio_device(const led_indicator_config_t *led_config, const led_indicator_gpio_config_t *gpio_cfg, led_indicator_handle_t *handle);

#ifdef __cplusplus
}
#endif
