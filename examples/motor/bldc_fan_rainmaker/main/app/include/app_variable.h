/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
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
    uint16_t min_speed;    /*!< motor min speed */
    uint16_t max_speed;    /*!< motor max speed */
    uint16_t target_speed; /*!< motor target speed */
} motor_parameter_t;

extern motor_parameter_t motor_parameter;

/**
 * @brief variable init
 *
 */
void app_variable_init();

#ifdef __cplusplus
}
#endif
