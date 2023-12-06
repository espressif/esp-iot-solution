/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_timer.h"

typedef enum
{
    STEP_CW = 0,
    STEP_CCW,
} stepper_dir_t;

typedef struct
{
    uint8_t is_start;
    gpio_num_t pin[4];
    stepper_dir_t dir;
    esp_timer_handle_t stepper_timer;
} stepper_motor_t;

extern stepper_motor_t stepper_motor;

/**
 * @brief stepper init
 *
 * @param pin_1
 * @param pin_2
 * @param pin_3
 * @param pin_4
 * @return esp_err_t
 */
esp_err_t hal_stepper_motor_init(gpio_num_t pin_1, gpio_num_t pin_2, gpio_num_t pin_3, gpio_num_t pin_4);
