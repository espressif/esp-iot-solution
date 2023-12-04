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
    BP1658CJ_RGB_CURRENT_0MA,
    BP1658CJ_RGB_CURRENT_10MA,
    BP1658CJ_RGB_CURRENT_20MA,
    BP1658CJ_RGB_CURRENT_30MA,
    BP1658CJ_RGB_CURRENT_40MA,
    BP1658CJ_RGB_CURRENT_50MA,
    BP1658CJ_RGB_CURRENT_60MA,
    BP1658CJ_RGB_CURRENT_70MA,
    BP1658CJ_RGB_CURRENT_80MA,
    BP1658CJ_RGB_CURRENT_90MA,
    BP1658CJ_RGB_CURRENT_100MA,
    BP1658CJ_RGB_CURRENT_110MA,
    BP1658CJ_RGB_CURRENT_120MA,
    BP1658CJ_RGB_CURRENT_130MA,
    BP1658CJ_RGB_CURRENT_140MA,
    BP1658CJ_RGB_CURRENT_150MA,
    BP1658CJ_RGB_CURRENT_MAX,
} bp1658cj_rgb_current_t;

/**
 * @brief Current options for CW channels
 *
 */
typedef enum {
    BP1658CJ_CW_CURRENT_0MA,
    BP1658CJ_CW_CURRENT_5MA,
    BP1658CJ_CW_CURRENT_10MA,
    BP1658CJ_CW_CURRENT_15MA,
    BP1658CJ_CW_CURRENT_20MA,
    BP1658CJ_CW_CURRENT_25MA,
    BP1658CJ_CW_CURRENT_30MA,
    BP1658CJ_CW_CURRENT_35MA,
    BP1658CJ_CW_CURRENT_40MA,
    BP1658CJ_CW_CURRENT_45MA,
    BP1658CJ_CW_CURRENT_50MA,
    BP1658CJ_CW_CURRENT_55MA,
    BP1658CJ_CW_CURRENT_60MA,
    BP1658CJ_CW_CURRENT_65MA,
    BP1658CJ_CW_CURRENT_70MA,
    BP1658CJ_CW_CURRENT_75MA,
    BP1658CJ_CW_CURRENT_MAX,
} bp1658cj_cw_current_t;

/**
 * @brief Output configuration
 *
 */
typedef struct {
    bp1658cj_rgb_current_t rgb_current;
    bp1658cj_cw_current_t cw_current;
    gpio_num_t iic_clk;
    gpio_num_t iic_sda;
    bool enable_iic_queue;
    uint16_t freq_khz;
} driver_bp1658cj_t;

/**
 * @brief BP1658CJ channel abstract definition
 *
 */
typedef enum {
    BP1658CJ_CHANNEL_R = 0,
    BP1658CJ_CHANNEL_G,
    BP1658CJ_CHANNEL_B,
    BP1658CJ_CHANNEL_C,
    BP1658CJ_CHANNEL_W,
    BP1658CJ_CHANNEL_MAX,
} bp1658cj_channel_t;

/**
 * @brief BP1658CJ output pin definition
 *
 */
typedef enum {
    BP1658CJ_PIN_OUT1 = 0,
    BP1658CJ_PIN_OUT2,
    BP1658CJ_PIN_OUT3,
    BP1658CJ_PIN_OUT4,
    BP1658CJ_PIN_OUT5,
    BP1658CJ_PIN_OUT_MAX,
} bp1658cj_out_pin_t;

/**
 * @brief Initialize bp1658cj output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t bp1658cj_init(driver_bp1658cj_t *config, void(*hook_func)(void *));

/**
 * @brief Set any channel output
 *
 * @param channel Abstract pin
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t bp1658cj_set_channel(bp1658cj_channel_t channel, uint16_t value);

/**
 * @brief Register the bp1658cj channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param pin Chip pin
 * @return esp_err_t
 */
esp_err_t bp1658cj_regist_channel(bp1658cj_channel_t channel, bp1658cj_out_pin_t pin);

/**
 * @brief Set only rgb channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t bp1658cj_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b);

/**
 * @brief Set only cw channel output
 *
 * @param value_c Output cold white value
 * @param value_w Output warm white value
 * @return esp_err_t
 */
esp_err_t bp1658cj_set_cw_channel(uint16_t value_c, uint16_t value_w);

/**
 * @brief Set all channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @param value_c Output cold white value
 * @param value_w Output warm white value
 * @return esp_err_t
 */
esp_err_t bp1658cj_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t bp1658cj_set_shutdown(void);

/**
 * @brief Set sleep mode
 *
 * @param enable_sleep If set to true will enter sleep mode
 * @return esp_err_t
 */
esp_err_t bp1658cj_set_sleep_mode(bool enable_sleep);

/**
 * @brief Set the maximum current
 *
 * @param rgb rgb channel current
 * @param cw cw channel current
 * @return esp_err_t
 */
esp_err_t bp1658cj_set_max_current(bp1658cj_rgb_current_t rgb, bp1658cj_cw_current_t cw);

/**
 * @brief Deinitialize bp1658cj and release resources
 *
 * @return esp_err_t
 */
esp_err_t bp1658cj_deinit(void);
