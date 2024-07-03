/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t is_start;      /*!< motor status 0:off 1:start */
    uint8_t is_natural;    /*!< motor natural wind 0: off 1: start */
    uint8_t start_count;   /*!< number of motor starts */
    uint8_t restart_count;  /*!< number of motor restarts */
    uint16_t min_speed;    /*!< motor min speed */
    uint16_t max_speed;    /*!< motor max speed */
    uint16_t target_speed; /*!< motor target speed */
} motor_parameter_t;

extern motor_parameter_t motor_parameter;

/**
 * @brief Initializes the system variables
 *
 */
void app_variable_init();

#ifdef __cplusplus
}
#endif
