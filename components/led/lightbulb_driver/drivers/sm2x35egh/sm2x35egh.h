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
    SM2235EGH_RGB_CURRENT_4MA = 0,
    SM2235EGH_RGB_CURRENT_8MA,
    SM2235EGH_RGB_CURRENT_12MA,
    SM2235EGH_RGB_CURRENT_16MA,
    SM2235EGH_RGB_CURRENT_20MA,
    SM2235EGH_RGB_CURRENT_24MA,
    SM2235EGH_RGB_CURRENT_28MA,
    SM2235EGH_RGB_CURRENT_32MA,
    SM2235EGH_RGB_CURRENT_36MA,
    SM2235EGH_RGB_CURRENT_40MA,
    SM2235EGH_RGB_CURRENT_44MA,
    SM2235EGH_RGB_CURRENT_48MA,
    SM2235EGH_RGB_CURRENT_52MA,
    SM2235EGH_RGB_CURRENT_56MA,
    SM2235EGH_RGB_CURRENT_60MA,
    SM2235EGH_RGB_CURRENT_64MA,
    SM2235EGH_RGB_CURRENT_MAX,
    SM2335EGH_RGB_CURRENT_10MA = 0,
    SM2335EGH_RGB_CURRENT_20MA,
    SM2335EGH_RGB_CURRENT_30MA,
    SM2335EGH_RGB_CURRENT_40MA,
    SM2335EGH_RGB_CURRENT_50MA,
    SM2335EGH_RGB_CURRENT_60MA,
    SM2335EGH_RGB_CURRENT_70MA,
    SM2335EGH_RGB_CURRENT_80MA,
    SM2335EGH_RGB_CURRENT_90MA,
    SM2335EGH_RGB_CURRENT_100MA,
    SM2335EGH_RGB_CURRENT_110MA,
    SM2335EGH_RGB_CURRENT_120MA,
    SM2335EGH_RGB_CURRENT_130MA,
    SM2335EGH_RGB_CURRENT_140MA,
    SM2335EGH_RGB_CURRENT_150MA,
    SM2335EGH_RGB_CURRENT_160MA,
    SM2335EGH_RGB_CURRENT_MAX,
} sm2x35egh_rgb_current_t;

/**
 * @brief Current options for CW channels
 *
 */
typedef enum {
    SM2235EGH_CW_CURRENT_5MA = 0,
    SM2235EGH_CW_CURRENT_10MA,
    SM2235EGH_CW_CURRENT_15MA,
    SM2235EGH_CW_CURRENT_20MA,
    SM2235EGH_CW_CURRENT_25MA,
    SM2235EGH_CW_CURRENT_30MA,
    SM2235EGH_CW_CURRENT_35MA,
    SM2235EGH_CW_CURRENT_40MA,
    SM2235EGH_CW_CURRENT_45MA,
    SM2235EGH_CW_CURRENT_50MA,
    SM2235EGH_CW_CURRENT_55MA,
    SM2235EGH_CW_CURRENT_60MA,
    SM2235EGH_CW_CURRENT_65MA,
    SM2235EGH_CW_CURRENT_70MA,
    SM2235EGH_CW_CURRENT_75MA,
    SM2235EGH_CW_CURRENT_80MA,
    SM2235EGH_CW_CURRENT_MAX,
    SM2335EGH_CW_CURRENT_5MA = 0,
    SM2335EGH_CW_CURRENT_10MA,
    SM2335EGH_CW_CURRENT_15MA,
    SM2335EGH_CW_CURRENT_20MA,
    SM2335EGH_CW_CURRENT_25MA,
    SM2335EGH_CW_CURRENT_30MA,
    SM2335EGH_CW_CURRENT_35MA,
    SM2335EGH_CW_CURRENT_40MA,
    SM2335EGH_CW_CURRENT_45MA,
    SM2335EGH_CW_CURRENT_50MA,
    SM2335EGH_CW_CURRENT_55MA,
    SM2335EGH_CW_CURRENT_60MA,
    SM2335EGH_CW_CURRENT_65MA,
    SM2335EGH_CW_CURRENT_70MA,
    SM2335EGH_CW_CURRENT_75MA,
    SM2335EGH_CW_CURRENT_80MA,
    SM2335EGH_CW_CURRENT_MAX,
} sm2x35egh_cw_current_t;

/**
 * @brief Output configuration
 *
 */
typedef struct {
    sm2x35egh_rgb_current_t rgb_current;
    sm2x35egh_cw_current_t cw_current;
    gpio_num_t iic_clk;
    gpio_num_t iic_sda;
    bool enable_iic_queue;
    uint16_t freq_khz;
} driver_sm2x35egh_t;

/**
 * @brief SM2x35EGH channel abstract definition
 *
 */
typedef enum {
    SM2x35EGH_CHANNEL_R = 0,
    SM2x35EGH_CHANNEL_G,
    SM2x35EGH_CHANNEL_B,
    SM2x35EGH_CHANNEL_C,
    SM2x35EGH_CHANNEL_W,
    SM2x35EGH_CHANNEL_MAX,
} sm2x35egh_channel_t;

/**
 * @brief SM2x35EGH output pin definition
 *
 */
typedef enum {
    SM2x35EGH_PIN_OUT1 = 0,
    SM2x35EGH_PIN_OUT2,
    SM2x35EGH_PIN_OUT3,
    SM2x35EGH_PIN_OUT4,
    SM2x35EGH_PIN_OUT5,
    SM2x35EGH_PIN_OUT_MAX,
} sm2x35egh_out_pin_t;

/**
 * @brief Initialize sm2235egh/sm2335egh output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t sm2x35egh_init(driver_sm2x35egh_t *config, void(*hook_func)(void *));

/**
 * @brief Set any channel output
 *
 * @param channel Abstract pin
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t sm2x35egh_set_channel(sm2x35egh_channel_t channel, uint16_t value);

/**
 * @brief Register the sm2135eh channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param pin Chip pin
 * @return esp_err_t
 */
esp_err_t sm2x35egh_regist_channel(sm2x35egh_channel_t channel, sm2x35egh_out_pin_t pin);

/**
 * @brief Set only rgb channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t sm2x35egh_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b);

/**
 * @brief Set only cw channel output
 *
 * @param value_w Output white value
 * @param value_y Output yellow value
 * @return esp_err_t
 */
esp_err_t sm2x35egh_set_cw_channel(uint16_t value_w, uint16_t value_y);

/**
 * @brief Set all channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @param value_c Output cold value
 * @param value_w Output warm value
 * @return esp_err_t
 */
esp_err_t sm2x35egh_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t sm2x35egh_set_shutdown(void);

/**
 * @brief Deinitialize sm2235egh/sm2335egh and release resources
 *
 * @return esp_err_t
 */
esp_err_t sm2x35egh_deinit(void);

/**
 * @brief Set standby mode
 *
 * @param enable_standby If set to true will enter standby mode
 * @return esp_err_t
 */
esp_err_t sm2x35egh_set_standby_mode(bool enable_sleep);
