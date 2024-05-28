/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Install and initialize the buzzer driver.
 *
 * This function initializes buzzer by using LEDC peripheral
 *
 * @param buzzer_pin The GPIO pin number connected to the buzzer.
 * @return
 *     - ESP_OK: The operation was successful.
 *     - ESP_ERR_INVALID_ARG: The buzzer_pin parameter is invalid.
 *     - ESP_FAIL: The installation or initialization failed due to other reasons.
 */
esp_err_t buzzer_driver_install(gpio_num_t buzzer_pin);

/**
 * @brief Buzzer outputs voice configuration
 *
 * @param en Enable / Disable buzzer output
 *
 * TODO: add voice frequency parameter
 *
 */
void buzzer_set_voice(bool en);

#ifdef __cplusplus
}
#endif
