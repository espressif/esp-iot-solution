/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bldc_common.h"
#include "bldc_adc.h"
#include "bldc_gptimer.h"
#include "bldc_gpio.h"
#include "bldc_ledc.h"
#if CONFIG_SOC_MCPWM_SUPPORTED
#include "bldc_mcpwm.h"
#endif

#ifdef __cplusplus
}
#endif
