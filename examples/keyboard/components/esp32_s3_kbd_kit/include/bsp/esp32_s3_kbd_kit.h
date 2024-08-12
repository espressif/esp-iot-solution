/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "keyboard_button.h"
#include "bsp/keyboard.h"
#include "led_strip.h"

esp_err_t bsp_keyboard_init(keyboard_btn_handle_t *kbd_handle, keyboard_btn_config_t *ex_cfg);

esp_err_t bsp_ws2812_init(led_strip_handle_t *led_strip);

esp_err_t bsp_ws2812_enable(bool enable);

esp_err_t bsp_ws2812_clear(void);

bool bsp_ws2812_is_enable(void);

esp_err_t bsp_lamp_array_init(uint32_t bind);

esp_err_t bsp_rgb_matrix_init(void);

#ifdef __cplusplus
}
#endif
