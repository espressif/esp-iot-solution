/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_err_t (*init)(uint32_t gpio_num);
    esp_err_t (*deinit)(uint32_t gpio_num);
    uint8_t (*get_key_level)(void *gpio_num);
    esp_err_t (*init_intr)(uint32_t gpio_num, gpio_int_type_t intr_type, gpio_isr_t isr_handler, void *args);
    esp_err_t (*set_intr)(uint32_t gpio_num, gpio_int_type_t intr_type);
    esp_err_t (*intr_control)(uint32_t gpio_num, bool enable);
    esp_err_t (*wake_up_control)(uint32_t gpio_num, uint8_t wake_up_level, bool enable);
    esp_err_t (*wake_up_init)(uint32_t gpio_num, uint8_t wake_up_level);
} knob_hal_t;

extern const knob_hal_t knob_gpio_hal;

#if SOC_RTCIO_INPUT_OUTPUT_SUPPORTED
extern const knob_hal_t knob_rtc_hal;
#endif

#ifdef __cplusplus
}
#endif
