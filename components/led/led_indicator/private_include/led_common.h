/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/ledc.h"

#define LEDC_MODE       CONFIG_LEDC_SPEED_MODE_VALUE
#define LEDC_DUTY_RES   CONFIG_LEDC_TIMER_BIT_NUM
#define LEDC_FREQ_HZ    CONFIG_LEDC_TIMER_FREQ_HZ

#define LEDC_TIMER_CONFIG(ledc_timer)   \
{                                     \
    .speed_mode = LEDC_MODE,          \
    .duty_resolution = LEDC_DUTY_RES, \
    .timer_num = ledc_timer,          \
    .freq_hz = LEDC_FREQ_HZ,          \
    .clk_cfg = LEDC_AUTO_CLK,         \
}

#define LEDC_CHANNEL_CONFIG(ledc_timer, ledc_channel, gpio) \
{                                                       \
    .speed_mode = LEDC_MODE,                            \
    .timer_sel = ledc_timer,                            \
    .hpoint = 0,                                        \
    .duty = 0,                                          \
    .intr_type = LEDC_INTR_DISABLE,                     \
    .channel = ledc_channel,                            \
    .gpio_num = gpio,                                   \
}

#ifdef __cplusplus
}
#endif
