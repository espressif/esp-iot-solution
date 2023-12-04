/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Output configuration
 *
 */
typedef struct {
    uint8_t led_num;
    gpio_num_t ctrl_io;
} driver_ws2812_t;

/**
 * @brief Initialize ws2812 output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t ws2812_init(driver_ws2812_t *config, void(*hook_func)(void *));

/**
 * @brief Deinitialize ws2812 and release resources
 *
 * @return esp_err_t
 */
esp_err_t ws2812_deinit(void);

/**
 * @brief Set all channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t ws2812_set_rgb_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b);

#ifdef __cplusplus
}
#endif
