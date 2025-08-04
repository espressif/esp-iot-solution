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
 * @brief Create a new LED indicator device using the LEDC (LED Controller) peripheral.
 *
 *
 * @param led_config   Pointer to the general LED configuration structure.
 * @param ledc_cfg     Pointer to the LEDC-specific configuration structure.
 * @param handle pointer to LED indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_OK Success
 *     - ESP_FAIL Failed to initialize LEDC mode or create LED indicator
 */
esp_err_t led_indicator_new_ledc_device(const led_indicator_config_t *led_config, const led_indicator_ledc_config_t *ledc_cfg, led_indicator_handle_t *handle);

#ifdef __cplusplus
}
#endif
