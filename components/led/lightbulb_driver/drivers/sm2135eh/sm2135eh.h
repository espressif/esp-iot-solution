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
    SM2135EH_RGB_CURRENT_9MA,
    SM2135EH_RGB_CURRENT_14MA,
    SM2135EH_RGB_CURRENT_19MA,
    SM2135EH_RGB_CURRENT_24MA,
    SM2135EH_RGB_CURRENT_29MA,
    SM2135EH_RGB_CURRENT_34MA,
    SM2135EH_RGB_CURRENT_39MA,
    SM2135EH_RGB_CURRENT_44MA,
    SM2135EH_RGB_CURRENT_MAX,
} sm2135eh_rgb_current_t;

/**
 * @brief Current options for WY channels
 *
 */
typedef enum {
    SM2135EH_WY_CURRENT_0MA,
    SM2135EH_WY_CURRENT_5MA,
    SM2135EH_WY_CURRENT_10MA,
    SM2135EH_WY_CURRENT_15MA,
    SM2135EH_WY_CURRENT_20MA,
    SM2135EH_WY_CURRENT_25MA,
    SM2135EH_WY_CURRENT_30MA,
    SM2135EH_WY_CURRENT_35MA,
    SM2135EH_WY_CURRENT_40MA,
    SM2135EH_WY_CURRENT_45MA,
    SM2135EH_WY_CURRENT_50MA,
    SM2135EH_WY_CURRENT_55MA,
    SM2135EH_WY_CURRENT_59MA,
    SM2135EH_WY_CURRENT_63MA,
    SM2135EH_WY_CURRENT_67MA,
    SM2135EH_WY_CURRENT_72MA,
    SM2135EH_WY_CURRENT_MAX,
} sm2135eh_wy_current_t;

/**
 * @brief Output configuration
 *
 */
typedef struct {
    sm2135eh_rgb_current_t rgb_current;
    sm2135eh_wy_current_t wy_current;
    gpio_num_t iic_clk;
    gpio_num_t iic_sda;
    bool enable_iic_queue;
    uint16_t freq_khz;
} driver_sm2135eh_t;

/**
 * @brief SM2135EH channel abstract definition
 *
 */
typedef enum {
    SM2135EH_CHANNEL_R = 0,
    SM2135EH_CHANNEL_G,
    SM2135EH_CHANNEL_B,
    SM2135EH_CHANNEL_W,
    SM2135EH_CHANNEL_Y,
    SM2135EH_CHANNEL_MAX,
} sm2135eh_channel_t;

/**
 * @brief SM2135EH output pin definition
 *
 */
typedef enum {
    SM2135EH_PIN_OUT1 = 0,
    SM2135EH_PIN_OUT2,
    SM2135EH_PIN_OUT3,
    SM2135EH_PIN_OUT4,
    SM2135EH_PIN_OUT5,
    SM2135EH_PIN_OUT_MAX,
} sm2135eh_out_pin_t;

/**
 * @brief Initialize sm2135e output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t sm2135eh_init(driver_sm2135eh_t *config, void(*hook_func)(void *));

/**
 * @brief Set any channel output
 *
 * @param channel Abstract pin
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t sm2135eh_set_channel(sm2135eh_channel_t channel, uint8_t value);

/**
 * @brief Register the sm2135eh channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param pin Chip pin
 * @return esp_err_t
 */
esp_err_t sm2135eh_regist_channel(sm2135eh_channel_t channel, sm2135eh_out_pin_t pin);

/**
 * @brief Set only rgb channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t sm2135eh_set_rgb_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b);

/**
 * @brief Set only wy channel output
 *
 * @param value_w Output white value
 * @param value_y Output yellow value
 * @return esp_err_t
 */
esp_err_t sm2135eh_set_wy_channel(uint8_t value_w, uint8_t value_y);

/**
 * @brief Set all channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @param value_w Output white value
 * @param value_y Output yellow value
 * @return esp_err_t
 */
esp_err_t sm2135eh_set_rgbwy_channel(uint8_t value_r, uint8_t value_g, uint8_t value_b, uint8_t value_w, uint8_t value_y);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t sm2135eh_set_shutdown(void);

/**
 * @brief Set standby mode
 *
 * @param enable_standby If set to true will enter standby mode
 * @return esp_err_t
 */
esp_err_t sm2135eh_set_standby_mode(bool enable_standby);

/**
 * @brief Set the maximum current
 *
 * @param rgb rgb channel current
 * @param wy wy channel current
 * @return esp_err_t
 */
esp_err_t sm2135eh_set_max_current(sm2135eh_rgb_current_t rgb, sm2135eh_wy_current_t wy);

/**
 * @brief Deinitialize sm2135eh and release resources
 *
 * @return esp_err_t
 */
esp_err_t sm2135eh_deinit(void);
