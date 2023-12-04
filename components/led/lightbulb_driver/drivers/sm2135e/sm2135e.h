/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#pragma once

/**
 * @brief Current options for RGB channels
 *
 */
typedef enum {
    SM2135E_RGB_CURRENT_10MA,
    SM2135E_RGB_CURRENT_15MA,
    SM2135E_RGB_CURRENT_20MA,
    SM2135E_RGB_CURRENT_25MA,
    SM2135E_RGB_CURRENT_30MA,
    SM2135E_RGB_CURRENT_35MA,
    SM2135E_RGB_CURRENT_40MA,
    SM2135E_RGB_CURRENT_45MA,
    SM2135E_RGB_CURRENT_MAX,
} sm2135e_rgb_current_t;

/**
 * @brief Current options for WY channels
 *
 */
typedef enum {
    SM2135E_WY_CURRENT_10MA,
    SM2135E_WY_CURRENT_15MA,
    SM2135E_WY_CURRENT_20MA,
    SM2135E_WY_CURRENT_25MA,
    SM2135E_WY_CURRENT_30MA,
    SM2135E_WY_CURRENT_35MA,
    SM2135E_WY_CURRENT_40MA,
    SM2135E_WY_CURRENT_45MA,
    SM2135E_WY_CURRENT_50MA,
    SM2135E_WY_CURRENT_55MA,
    SM2135E_WY_CURRENT_60MA,
    SM2135E_WY_CURRENT_MAX,
} sm2135e_wy_current_t;

/**
 * @brief Output configuration
 *
 */
typedef struct {
    sm2135e_rgb_current_t rgb_current;
    sm2135e_wy_current_t wy_current;
    gpio_num_t iic_clk;
    gpio_num_t iic_sda;
    bool enable_iic_queue;
    uint16_t freq_khz;
} driver_sm2135e_t;

/**
 * @brief SM2135E channel abstract definition
 *
 */
typedef enum {
    SM2135E_CHANNEL_R = 0,
    SM2135E_CHANNEL_G,
    SM2135E_CHANNEL_B,
    SM2135E_CHANNEL_W,
    SM2135E_CHANNEL_Y,
    SM2135E_CHANNEL_MAX,
} sm2135e_channel_t;

/**
 * @brief SM2135E output pin definition
 *
 */
typedef enum {
    SM2135E_PIN_OUT1 = 0,
    SM2135E_PIN_OUT2,
    SM2135E_PIN_OUT3,
    SM2135E_PIN_OUT4,
    SM2135E_PIN_OUT5,
    SM2135E_PIN_OUT_MAX,
} sm2135e_out_pin_t;

/**
 * @brief Initialize sm2135e output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t sm2135e_init(driver_sm2135e_t *config, void(*hook_func)(void *));

/**
 * @brief Register the sm2135e channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param pin Chip pin
 * @return esp_err_t
 */
esp_err_t sm2135e_regist_channel(sm2135e_channel_t channel, sm2135e_out_pin_t pin);

/**
 * @brief Set any channel output
 *
 * @param channel Abstract pin
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t sm2135e_set_channel(sm2135e_channel_t channel, uint8_t value);

/**
 * @brief Set only rgb channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t sm2135e_set_rgb_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b);

/**
 * @brief Set only wy channel output
 *
 * @param value_w Output white value
 * @param value_y Output yellow value
 * @return esp_err_t
 */
esp_err_t sm2135e_set_wy_channel(uint8_t value_w, uint8_t value_y);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t sm2135e_set_shutdown(void);

/**
 * @brief Set output mode
 *
 * @param set_wy_mode If set to true, the wy channel will output
 * @return esp_err_t
 */
esp_err_t sm2135e_set_output_mode(bool set_wy_mode);

/**
 * @brief Set the maximum current
 *
 * @param rgb rgb channel current
 * @param wy wy channel current
 * @return esp_err_t
 */
esp_err_t sm2135e_set_max_current(sm2135e_rgb_current_t rgb, sm2135e_wy_current_t wy);

/**
 * @brief Deinitialize sm2135e and release resources
 *
 * @return esp_err_t
 */
esp_err_t sm2135e_deinit(void);
