/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#pragma once

/**
 * @brief Output configuration
 *
 */
typedef struct {
    uint8_t current[5];
    gpio_num_t iic_clk;
    gpio_num_t iic_sda;
    bool enable_iic_queue;
    uint16_t freq_khz;
} driver_bp5758d_t;

/**
 * @brief BP5758D channel abstract definition
 *
 */
typedef enum {
    BP5758D_CHANNEL_R = 0,
    BP5758D_CHANNEL_G,
    BP5758D_CHANNEL_B,
    BP5758D_CHANNEL_C,
    BP5758D_CHANNEL_W,
    BP5758D_CHANNEL_MAX,
} bp5758d_channel_t;

/**
 * @brief BP5758D output pin definition
 *
 */
typedef enum {
    BP5758D_PIN_OUT1 = 0,
    BP5758D_PIN_OUT2,
    BP5758D_PIN_OUT3,
    BP5758D_PIN_OUT4,
    BP5758D_PIN_OUT5,
    BP5758D_PIN_OUT_MAX,
} bp5758d_out_pin_t;

/**
 * @brief Initialize sm2135e output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t bp5758d_init(driver_bp5758d_t *config, void(*hook_func)(void *));

/**
 * @brief Set any channel output
 *
 * @param channel Abstract pin
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t bp5758d_set_channel(bp5758d_channel_t channel, uint16_t value);

/**
 * @brief Register the bp5758d channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param pin Chip pin
 * @return esp_err_t
 */
esp_err_t bp5758d_regist_channel(bp5758d_channel_t channel, bp5758d_out_pin_t pin);

/**
 * @brief Set only rgb channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t bp5758d_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b);

/**
 * @brief Set only cw channel output
 *
 * @param value_c Output cold value
 * @param value_w Output white value
 * @return esp_err_t
 */
esp_err_t bp5758d_set_cw_channel(uint16_t value_c, uint16_t value_w);

/**
 * @brief Set all channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @param value_w Output cold value
 * @param value_y Output white value
 * @return esp_err_t
 */
esp_err_t bp5758d_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t bp5758d_set_shutdown(void);

/**
 * @brief Set standby mode
 *
 * @param enable_standby If set to true will enter standby mode
 * @return esp_err_t
 */
esp_err_t bp5758d_set_standby_mode(bool enable_sleep);

/**
 * @brief Deinitialize bp5758d and release resources
 *
 * @return esp_err_t
 */
esp_err_t bp5758d_deinit(void);
