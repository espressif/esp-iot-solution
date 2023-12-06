
/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"

/**
 * @brief Get the gamma value of the LED indicator
 *
 * @param input The input value to get the gamma value from, should be between 0 and 255
 * @return uint8_t The gamma value of the LEDC indicator
 */
uint8_t led_indicator_get_gamma_value(uint8_t input);

/**
 * @brief Create a new gamma table for the LED indicator
 *
 * @param gamma The gamma value to use for the new table, should be larger than 0
 *              Gamma values less than 1 (0-1) create steeper adjustment curves, enhancing details in
 *              darker areas while compressing brighter areas. On the other hand, gamma values greater
 *              than 1 create gentler adjustment curves, enhancing details in brighter areas but potentially
 *              losing details in darker areas. However, a gamma value greater than 1 is generally considered
 *              more visually pleasing as it better mimics the non-linear perception of brightness by the human eye.
 * @return esp_err_t
 *         ESP_ERR_INVALID_ARG if gamma is less than or equal to 0
 *         ESP_OK on success
 */
esp_err_t led_indicator_new_gamma_table(float gamma);

#ifdef __cplusplus
}
#endif
