/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a GPIO pin for knob input.
 *
 * This function configures a specified GPIO pin as an input for knob control.
 * It sets the pin mode, disables interrupts, and enables the pull-up resistor.
 *
 * @param gpio_num The GPIO number to be configured.
 * @return
 *      - ESP_OK: Configuration successful.
 *      - ESP_ERR_INVALID_ARG: Parameter error.
 *      - ESP_FAIL: Configuration failed.
 */
esp_err_t knob_gpio_init(uint32_t gpio_num);

/**
 * @brief Deinitialize a GPIO pin for knob input.
 *
 * This function resets the specified GPIO pin.
 *
 * @param gpio_num The GPIO number to be deinitialized.
 * @return
 *      - ESP_OK: Reset successful.
 *      - ESP_FAIL: Reset failed.
 */
esp_err_t knob_gpio_deinit(uint32_t gpio_num);

/**
 * @brief Get the level of a GPIO pin.
 *
 * This function returns the current level (high or low) of the specified GPIO pin.
 *
 * @param gpio_num The GPIO number to read the level from.
 * @return The level of the GPIO pin (0 or 1).
 */
uint8_t knob_gpio_get_key_level(void *gpio_num);

/**
 * @brief Initialize a GPIO pin with interrupt capability for knob input.
 *
 * This function configures a specified GPIO pin to trigger interrupts and installs
 * an ISR service if not already installed. It adds an ISR handler for the GPIO pin.
 *
 * @param gpio_num The GPIO number to be configured.
 * @param intr_type The type of interrupt to be configured.
 * @param isr_handler The ISR handler function to be called on interrupt.
 * @param args Arguments to be passed to the ISR handler.
 * @return
 *      - ESP_OK: Configuration successful.
 *      - ESP_ERR_INVALID_ARG: Parameter error.
 *      - ESP_FAIL: Configuration failed.
 */
esp_err_t knob_gpio_init_intr(uint32_t gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler, void *args);

/**
 * @brief Set the interrupt type for a GPIO pin.
 *
 * This function sets the interrupt type for the specified GPIO pin.
 *
 * @param gpio_num The GPIO number to configure.
 * @param intr_type The type of interrupt to be configured.
 * @return
 *      - ESP_OK: Configuration successful.
 *      - ESP_ERR_INVALID_ARG: Parameter error.
 *      - ESP_FAIL: Configuration failed.
 */
esp_err_t knob_gpio_set_intr(uint32_t gpio_num, gpio_int_type_t intr_type);

/**
 * @brief Control the interrupt for a GPIO pin.
 *
 * This function enables or disables the interrupt for the specified GPIO pin.
 *
 * @param gpio_num The GPIO number to configure.
 * @param enable Whether to enable or disable the interrupt.
 * @return
 *      - ESP_OK: Configuration successful.
 *      - ESP_ERR_INVALID_ARG: Parameter error.
 *      - ESP_FAIL: Configuration failed.
 */
esp_err_t knob_gpio_intr_control(uint32_t gpio_num, bool enable);

/**
 * @brief Control the wake up functionality of GPIO pins.
 *
 * This function enables or disables the wake up functionality from GPIO pins.
 *
 * @param enable Whether to enable or disable the wake up functionality.
 * @param wake_up_level Level of the GPIO when triggered.
 * @param enable Enable or disable the GPIO wakeup.
 * @return
 *      - ESP_OK: Operation successful.
 *      - ESP_FAIL: Operation failed.
 */
esp_err_t knob_gpio_wake_up_control(uint32_t gpio_num, uint8_t wake_up_level, bool enable);

/**
 * @brief Initialize wake up functionality for a GPIO pin.
 *
 * This function configures a specified GPIO pin to wake up the system from sleep
 * based on the specified wake up level.
 *
 * @param gpio_num The GPIO number to configure.
 * @param wake_up_level The level (0 or 1) to trigger wake up.
 * @return
 *      - ESP_OK: Configuration successful.
 *      - ESP_ERR_INVALID_ARG: Parameter error.
 *      - ESP_FAIL: Configuration failed.
 */
esp_err_t knob_gpio_wake_up_init(uint32_t gpio_num, uint8_t wake_up_level);

#ifdef __cplusplus
}
#endif
