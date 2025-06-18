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

typedef struct {
    bool is_active_level_high;              /*!< Set true if GPIO level is high when light is ON, otherwise false. */
    bool timer_inited;                      /*!< Set true if LEDC timer is inited, otherwise false. */
    ledc_timer_t timer_num;                 /*!< The timer source of channel */
    int32_t gpio_num;                       /*!< num of gpio */
    ledc_channel_t channel;                 /*!< LEDC channel */
} led_indicator_ledc_config_t;

/**
 * @brief Initialize LEDC-related configurations for this LED indicator.
 *
 * @param param LEDC config: ledc_timer_config & ledc_channel_config
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Deinit fail
 *     - ESP_ERR_NO_MEM failed to request memory
 */
esp_err_t led_indicator_ledc_init(void *param);

/**
 * @brief Deinitialize LEDC which is used by the LED indicator.
 *
 * @param ledc_handle LED indicator LEDC operation handle
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_FAIL Deinit fail
 */
esp_err_t led_indicator_ledc_deinit(void *ledc_handle);

/**
 * @brief Set the specific LEDC's level to make the LED indicator ON or OFF
 *
 * @param ledc_handle LED indicator LEDC operation handle
 * @param on_off Set 0 to control the LEDC's output level low, while values greater than 0 set the LEDC's output level high..
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL LEDC channel doesn't init
 */
esp_err_t led_indicator_ledc_set_on_off(void *ledc_handle, bool on_off);

/**
 * @brief Set LEDC duty cycle
 *
 * @param ledc_handle LED indicator LEDC operation handle
 * @param brightness duty cycle of LEDC [0-255]
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Set brightness fail
 */
esp_err_t led_indicator_ledc_set_brightness(void *ledc_handle, uint32_t brightness);

/**
 * @brief Create a new LED indicator device using the LEDC (LED Controller) peripheral.
 *
 * This function initializes a new LED indicator device with the specified configuration,
 * utilizing the ESP32's LEDC hardware for PWM control.
 *
 * @param led_config   Pointer to the general LED configuration structure.
 * @param ledc_cfg     Pointer to the LEDC-specific configuration structure.
 * @param handle pointer to LED indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_OK Success
 *     - ESP_FAIL Delete fail
 */
led_indicator_handle_t iot_led_new_ledc_device(const led_config_t *led_config, const led_indicator_ledc_config_t *ledc_cfg);

#ifdef __cplusplus
}
#endif
