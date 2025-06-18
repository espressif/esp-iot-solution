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
 * @brief Initialize the specific GPIO to work as a LED indicator
 *
 * @param param led_indicator_gpio_config_t
 * @param ret_handle gpio_handle
 * @return esp_err_t
 *     - ESP_OK success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t led_indicator_gpio_init(void *param, void **ret_handle);

/**
 * @brief Deinitialize the specific GPIO that works as a LED indicator
 *
 * @param handle GPIO handle
 * @return esp_err_t
 *     - ESP_OK success
 */
esp_err_t led_indicator_gpio_deinit(void *handle);

/**
 * @brief Set the specific GPIO's level to make the LED indicator ON or OFF
 *
 * @param handle GPIO handle
 * @param on_off Set number to control the GPIO's level. If the LED's positive side is connected to this GPIO, then setting number greater than 0 will make the LED OFF,
 *               and setting 0 will make the LED ON. If the LED's negative side is connected to this GPIO, then setting 0 will make the LED OFF, and
 *               setting number greater than 0 will make the LED ON.
 * @return esp_err_t
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG GPIO number error
 */
esp_err_t led_indicator_gpio_set_on_off(void *handle, bool on_off);

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
 *     - ESP_FAIL Delete fail
 */
led_indicator_handle_t iot_led_new_gpio_device(const led_config_t *led_config, const led_indicator_gpio_config_t *gpio_cfg);
#ifdef __cplusplus
}
#endif
