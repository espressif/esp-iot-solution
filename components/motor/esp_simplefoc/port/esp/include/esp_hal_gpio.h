/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _IoRecord {
    gpio_config_t conf; /*!< gpio pin configuration */
    gpio_num_t number;  /*!< gpio pin number */
} IoRecord;

/**
 * @brief Set GPIO Mode. Arduino style function.
 *
 * @param pin gpio pin number
 * @param mode gpio pin mode
 */
void pinMode(uint8_t pin, uint8_t mode);

/**
 * @brief Write GPIO Value. Arduino style function.
 *
 * @param pin gpio pin number
 * @param val gpio pin level
 */
void digitalWrite(uint8_t pin, uint8_t val);

/**
 * @brief Read GPIO Value. Arduino style function.
 *
 * @param pin gpio pin number
 * @return
 *     - 0 Low level
 *     - 1 High level
 */
int digitalRead(uint8_t pin);

/**
 * @brief Set GPIO Interrupt. Arduino style function.
 *
 * @param pin gpio pin number
 * @return uint8_t gpio pin number
 * @note
 *      This function only returns the pin number that needs to be interrupted by binding.
 *
 */
uint8_t digitalPinToInterrupt(uint8_t pin);

/**
 * @brief Bind funtion to GPIO Interrupt. Arduino style function.
 *
 * @param pin gpio pin number
 * @param handler interrupt function
 * @param mode interrupt triggering mode
 */
void attachInterrupt(uint8_t pin, void (*handler)(void), int mode);

#ifdef __cplusplus
}
#endif
