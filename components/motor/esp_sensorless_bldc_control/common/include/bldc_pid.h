/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "pid_ctrl.h"

#define BLDC_LPF(valuePrev, value, alpha) ((valuePrev) = (valuePrev) * (alpha) + (value) * (1 - (alpha)))

#define PID_CTRL_PARAMETER_DEFAULT() \
{                                    \
    .kp = SPEED_KP,   \
    .ki = SPEED_KI,   \
    .kd = SPEED_KD,   \
    .max_output = SPEED_MAX_OUTPUT, \
    .min_output = SPEED_MIN_OUTPUT, \
    .max_integral = SPEED_MAX_INTEGRAL, \
    .min_integral = SPEED_MIN_INTEGRAL, \
    .cal_type = SPEED_CAL_TYPE,       \
}

#ifdef __cplusplus
}
#endif
