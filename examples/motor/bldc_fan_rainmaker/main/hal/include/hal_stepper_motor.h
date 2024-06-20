/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STEP_CW = 0,
    STEP_CCW,
} stepper_dir_t;

typedef struct {
    uint8_t is_start;
    gpio_num_t pin[4];
    stepper_dir_t dir;
    esp_timer_handle_t stepper_timer;
} stepper_motor_t;

extern stepper_motor_t stepper_motor;

/**
 * @brief Initializes the stepper
 *
 * @param pin_1 GPIO number
 * @param pin_2 GPIO number
 * @param pin_3 GPIO number
 * @param pin_4 GPIO number
 * @return
 *    - ESP_OK：Success in initializing the stepper
 *    - ESP_FAIL: Esp timer or gpio resource is not available.
 */
esp_err_t hal_stepper_motor_init(gpio_num_t pin_1, gpio_num_t pin_2, gpio_num_t pin_3, gpio_num_t pin_4);

#ifdef __cplusplus
}
#endif
