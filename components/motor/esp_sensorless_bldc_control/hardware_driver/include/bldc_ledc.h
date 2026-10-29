/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "bldc_common.h"
#include "driver/ledc.h"

#define BLDC_LEDC_TIMER      LEDC_TIMER_0
#define BLDC_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define BLDC_LEDC_DUTY_RES   LEDC_TIMER_11_BIT

#if CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32P4
#define LEDC_TIMER_CONFIG_DEFAULT()    \
{                                          \
    .speed_mode = BLDC_LEDC_MODE,          \
    .duty_resolution = BLDC_LEDC_DUTY_RES, \
    .timer_num = LEDC_TIMER_0,             \
    .freq_hz = FREQ_HZ,                       \
    .clk_cfg = LEDC_USE_XTAL_CLK,           \
}
#else
#define LEDC_TIMER_CONFIG_DEFAULT()    \
{                                          \
    .speed_mode = BLDC_LEDC_MODE,          \
    .duty_resolution = BLDC_LEDC_DUTY_RES, \
    .timer_num = LEDC_TIMER_0,             \
    .freq_hz = FREQ_HZ,                       \
    .clk_cfg = LEDC_USE_APB_CLK,           \
}
#endif

#define LEDC_CHANNEL_CONFIG_DEFAULT() \
{                                     \
    .speed_mode = BLDC_LEDC_MODE,     \
    .timer_sel = BLDC_LEDC_TIMER,     \
    .hpoint = 0,                      \
    .duty = 0,                        \
    .intr_type = LEDC_INTR_DISABLE,   \
}

typedef struct {
    ledc_channel_t ledc_channel[3];
    int gpio_num[3];
} bldc_ledc_config_t;

esp_err_t bldc_ledc_init(bldc_ledc_config_t *ledc_config);

esp_err_t bldc_ledc_set_duty(void *channel, uint32_t duty);

esp_err_t bldc_ledc_deinit();

#ifdef __cplusplus
}
#endif
