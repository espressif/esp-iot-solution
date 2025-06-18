/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "led_indicator.h"

/**
 * @brief LED custom configuration
 *
 */
typedef struct {
    bool is_active_level_high;                                                             /*!< Set true if GPIO level is high when light is ON, otherwise false, values should be modified based on the actual scenario.*/
    led_indicator_duty_t duty_resolution;                                                  /*!< Resolution of duty setting in number of bits. The range of duty values is [0, (2**duty_resolution) -1]. If the brightness cannot be set, set this as 1. */
    esp_err_t (*hal_indicator_init)(void *hardware_data);                                  /*!< pointer functions for initialization*/
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);               /*!< pointer functions for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data);                                /*!< pointer functions for deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);   /*!< pointer functions for setting brightness, must be supported by hardware */
    esp_err_t (*hal_indicator_set_rgb)(void *hardware, uint32_t rgb_value);                /*!< Pointer function for setting rgb, must be supported by hardware */
    esp_err_t (*hal_indicator_set_hsv)(void *hardware, uint32_t hsv_value);                /*!< Pointer function for setting rgb, must be supported by hardware */
    void *hardware_data;                                                                   /*!< user hardware data*/
} led_indicator_custom_config_t;

/**
 * @brief Create a new custom LED indicator device.
 *
 *
 * @param led_config    Pointer to the LED configuration structure.
 * @param custom_cfg    Pointer to the custom configuration structure for the LED indicator.
 * @param handle pointer to LED indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_OK Success
 *     - ESP_FAIL Delete fail
 */
led_indicator_handle_t iot_led_new_custom_device(const led_config_t *led_config, const led_indicator_custom_config_t *custom_cfg);
#ifdef __cplusplus
}
#endif
