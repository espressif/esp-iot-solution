/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "bldc_control.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the bldc
 *
 * @param direction CW or CCW
 * @return
 *    - ESP_OK: Success in initializing the bldc
 *    - ESP_FAIL: BLDC parameter or resource configuration error
 */
esp_err_t hal_bldc_init(dir_enum_t direction);

/**
 * @brief Sets bldc operation status
 *
 * @param status 0 for stopping the motor, 1 for starting the motor
 * @return
 *    - ESP_OK: Success in setting the status of bldc
 *    - other: Specific error what went wrong during setting.
 */
esp_err_t hal_bldc_start_stop(uint8_t status);

/**
 * @brief Sets bldc speed
 *
 * @param speed Desired speed of the BLDC
 * @return
 *    - ESP_OK: Success in setting the speed of bldc
 *    - other: Specific error what went wrong during setting.
 */
esp_err_t hal_bldc_set_speed(uint16_t speed);

#ifdef __cplusplus
}
#endif
