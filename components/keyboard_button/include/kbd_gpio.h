/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumeration defining keyboard GPIO modes
 */
typedef enum {
    KBD_GPIO_MODE_OUTPUT = 0,
    KBD_GPIO_MODE_INPUT,
} kbd_gpio_mode_t;

typedef struct {
    const int *gpios;              /*!< Array, contains GPIO numbers */
    uint32_t gpio_num;             /*!< gpios array size */
    kbd_gpio_mode_t gpio_mode;     /*!< GPIO mode */
    uint32_t active_level;         /*!< Active level, only for input mode */
    bool enable_power_save;        /*!< Enable power save, only for input mode */
} kbd_gpio_config_t;

/**
 * @brief Init GPIOs for keyboard
 *
 * @param config Pointer to kbd_gpio_config_t
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t kbd_gpio_init(const kbd_gpio_config_t *config);

/**
 * @brief Deinitialize GPIOs used for keyboard
 *
 * This function deinitializes the GPIO pins used by the keyboard.
 * It is a counterpart of kbd_gpio_init().
 *
 * @param gpios     Pointer to an array of GPIO numbers
 * @param gpio_num  Number of elements in the gpios array
 */
void kbd_gpio_deinit(const int *gpios, uint32_t gpio_num);

/**
 * @brief Read levels of multiple GPIOs
 *
 * This function reads levels of multiple GPIOs in one call.
 *
 * @param gpios Pointer to an array of GPIO numbers
 * @param gpio_num Number of elements in the gpios array
 *
 * @return Bitmask of GPIO levels, bit N is set if GPIO N level is high
 */
uint32_t kbd_gpios_read_level(const int *gpios, uint32_t gpio_num);

/**
 * Read the level of a GPIO pin.
 *
 * @param gpio The GPIO pin number to read the level from
 *
 * @return The level of the GPIO pin (0 or 1)
 */
uint32_t kbd_gpio_read_level(int gpio);

/**
 * Set the level of one or more GPIO pins.
 *
 * @param gpio_num The GPIO pin number to set the level for
 * @param level The level to set (0 or 1)
 *
 * @return esp_err_t The error code indicating success or failure
 */
void kbd_gpios_set_level(const int *gpios, uint32_t gpio_num, uint32_t level);

/**
 * Set the level of a GPIO pin.
 *
 * @param gpio The GPIO pin number to set the level for
 * @param level The level to set (0 or 1)
 *
 * @return esp_err_t The error code indicating success or failure
 */
void kbd_gpio_set_level(int gpio, uint32_t level);

/**
 * @brief Enable holding of GPIOs to prevent them from floating
 *
 * This function enables holding of GPIOs to prevent them from floating.
 * It iterates through the array of GPIO pins provided and enables hold
 * for each of them.
 *
 * @param gpios Pointer to an array of GPIO numbers
 * @param gpio_num Number of GPIOs in the array
 */
void kbd_gpios_set_hold_en(const int *gpios, uint32_t gpio_num);

/**
 * @brief Disable holding of GPIOs to prevent them from being held in reset
 *
 * This function disables holding of GPIOs to prevent them from being held in reset.
 * It iterates through the array of GPIO pins provided and disables hold for each of them.
 *
 * @param gpios Pointer to an array of GPIO numbers
 * @param gpio_num Number of GPIOs in the array
 */
void kbd_gpios_set_hold_dis(const int *gpios, uint32_t gpio_num);

/**
 * @brief Set interrupt configuration for multiple GPIOs
 *
 * This function configures interrupt settings for multiple GPIOs.
 * It installs the ISR service if not already installed, then iterates
 * through the array of GPIO pins provided and sets the interrupt type
 * and ISR handler for each GPIO.
 *
 * @param gpios Pointer to an array of GPIO numbers
 * @param gpio_num Number of GPIOs in the array
 * @param intr_type Type of GPIO interrupt (GPIO_INTR_POSEDGE, GPIO_INTR_NEGEDGE, GPIO_INTR_ANYEDGE, GPIO_INTR_HIGH_LEVEL, GPIO_INTR_LOW_LEVEL)
 * @param isr_handler ISR handler function pointer
 * @param args Pointer to additional arguments for the ISR handler
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t kbd_gpios_set_intr(const int *gpios, uint32_t gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler, void *args);

/**
 * @brief Enable or disable GPIO interrupts
 *
 * This function enables or disables interrupts on the specified GPIOs.
 *
 * @param gpios     Pointer to an array of GPIO numbers
 * @param gpio_num  Number of elements in the gpios array
 * @param enable    true to enable interrupts, false to disable
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG   Arguments is invalid.
 */
esp_err_t kbd_gpios_intr_control(const int *gpios, uint32_t gpio_num, bool enable);

#ifdef __cplusplus
}
#endif
