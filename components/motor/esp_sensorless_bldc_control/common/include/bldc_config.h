/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if __has_include("bldc_user_cfg.h")
#include "bldc_user_cfg.h"
#else
#error "bldc_user_cfg.h not found, must provide by user"
#endif // BLDC_USER_CFG_H

#define COMPARER_RPM_CALCULATION_COEFFICIENT (((1000000 / ALARM_COUNT_US) / (2.0 * POLE_PAIR)) * 60)
#define ADC_RPM_CALCULATION_COEFFICIENT (((1000000 / ALARM_COUNT_US) / (1.0 * POLE_PAIR)) * 60)

#define SKIP_INVALID_SPEED_CALCULATION ((uint32_t)(COMPARER_RPM_CALCULATION_COEFFICIENT / (SPEED_MAX_RPM * MAX_SPEED_MEASUREMENT_FACTOR)))

#ifdef __cplusplus
}
#endif
