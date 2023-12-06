/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_variable.h"
#include "string.h"

motor_parameter_t motor_parameter;

void app_variable_init()
{
    memset(&motor_parameter, 0x00, sizeof(motor_parameter));
    motor_parameter.target_speed = 300; /*!< motor target speed */
    motor_parameter.min_speed = 300;    /*!< motor min speed */
    motor_parameter.max_speed = 1000;   /*!< motor max speed */
}
