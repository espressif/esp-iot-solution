/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_variable.h"
#include "string.h"
#include "bldc_fan_config.h"

motor_parameter_t motor_parameter;

void app_variable_init()
{
    memset(&motor_parameter, 0x00, sizeof(motor_parameter));
    motor_parameter.target_speed = BLDC_MIN_SPEED; /*!< motor target speed */
    motor_parameter.min_speed = BLDC_MIN_SPEED;    /*!< motor min speed */
    motor_parameter.max_speed = BLDC_MAX_SPEED;    /*!< motor max speed */
}
