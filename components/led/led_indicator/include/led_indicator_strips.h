/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "led_strip.h"
#include "led_strip_types.h"
#include "esp_idf_version.h"
#include "led_indicator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief led strip driver type
 *
 */
typedef enum {
    LED_STRIP_RMT,
    LED_STRIP_SPI,
    LED_STRIP_MAX,
} led_strip_driver_t;

typedef struct {
    led_strip_config_t led_strip_cfg;                   /*!< LED Strip Configuration. */
    led_strip_driver_t led_strip_driver;                /*!< led strip control type */
    union {
        led_strip_rmt_config_t led_strip_rmt_cfg;       /*!< RMT (Remote Control) Configuration for the LED strip. */
        led_strip_spi_config_t led_strip_spi_cfg;       /*!< SPI Configuration for the LED strip. */
    };
} led_indicator_strips_config_t;

/**
 * @brief Create a new LED indicator device using LED strips.
 *
 *
 * @param led_config    Pointer to the LED configuration structure.
 * @param strips_cfg    Pointer to the LED strips specific configuration structure.
 * @param handle pointer to LED indicator handle
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_OK Success
 *     - ESP_FAIL Delete fail
 */
esp_err_t led_indicator_new_strips_device(const led_indicator_config_t *led_config, const led_indicator_strips_config_t *strips_cfg, led_indicator_handle_t *handle);

#ifdef __cplusplus
}
#endif
