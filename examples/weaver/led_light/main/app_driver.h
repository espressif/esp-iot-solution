/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_POWER       true
#define DEFAULT_HUE         180
#define DEFAULT_SATURATION  100
#define DEFAULT_BRIGHTNESS  25

/**
 * @brief Initialize hardware driver (LED + button)
 */
void app_driver_init(void);

/**
 * @brief Set LED color in HSV
 *
 * @param[in] hue Hue (0-360)
 * @param[in] saturation Saturation (0-100)
 * @param[in] brightness Brightness (0-100)
 * @return
 *      - ESP_OK: LED color set successfully
 *      - ESP_ERR_INVALID_STATE: LED driver not initialized
 */
esp_err_t app_light_set_led(uint16_t hue, uint16_t saturation, uint16_t brightness);

/**
 * @brief Set light power state
 *
 * @param[in] power true to turn on, false to turn off
 * @return
 *      - ESP_OK: Power state set successfully
 *      - ESP_ERR_INVALID_STATE: LED driver not initialized
 */
esp_err_t app_light_set_power(bool power);

/**
 * @brief Set light brightness
 *
 * @param[in] brightness Brightness (0-100)
 * @return
 *      - ESP_OK: Brightness set successfully
 *      - ESP_ERR_INVALID_STATE: LED driver not initialized
 */
esp_err_t app_light_set_brightness(uint16_t brightness);

/**
 * @brief Set light hue
 *
 * @param[in] hue Hue (0-360)
 * @return
 *      - ESP_OK: Hue set successfully
 *      - ESP_ERR_INVALID_STATE: LED driver not initialized
 */
esp_err_t app_light_set_hue(uint16_t hue);

/**
 * @brief Set light saturation
 *
 * @param[in] saturation Saturation (0-100)
 * @return
 *      - ESP_OK: Saturation set successfully
 *      - ESP_ERR_INVALID_STATE: LED driver not initialized
 */
esp_err_t app_light_set_saturation(uint16_t saturation);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
