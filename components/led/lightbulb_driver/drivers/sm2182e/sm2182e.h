/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "driver/gpio.h"

/**
 * @brief Current options for WY channels
 *
 */
typedef enum {
    SM2182E_CW_CURRENT_5MA,
    SM2182E_CW_CURRENT_10MA,
    SM2182E_CW_CURRENT_15MA,
    SM2182E_CW_CURRENT_20MA,
    SM2182E_CW_CURRENT_25MA,
    SM2182E_CW_CURRENT_30MA,
    SM2182E_CW_CURRENT_35MA,
    SM2182E_CW_CURRENT_40MA,
    SM2182E_CW_CURRENT_45MA,
    SM2182E_CW_CURRENT_50MA,
    SM2182E_CW_CURRENT_55MA,
    SM2182E_CW_CURRENT_60MA,
    SM2182E_CW_CURRENT_65MA,
    SM2182E_CW_CURRENT_70MA,
    SM2182E_CW_CURRENT_75MA,
    SM2182E_CW_CURRENT_80MA,
    SM2182E_CW_CURRENT_MAX,
} sm2182e_cw_current_t;

/**
 * @brief Output configuration
 *
 */
typedef struct {
    sm2182e_cw_current_t cw_current;
    gpio_num_t iic_clk;
    gpio_num_t iic_sda;
    bool enable_iic_queue;
    uint16_t freq_khz;
} driver_sm2182e_t;

/**
 * @brief SM2182E channel abstract definition
 *
 */
typedef enum {
    SM2182E_CHANNEL_C = 3,
    SM2182E_CHANNEL_W,
    SM2182E_CHANNEL_MAX,
} sm2182e_channel_t;

/**
 * @brief SM2182E output pin definition
 *
 */
typedef enum {
    SM2182E_PIN_OUT1 = 0,
    SM2182E_PIN_OUT2,
    SM2182E_PIN_OUT_MAX,
} sm2182e_out_pin_t;

/**
 * @brief Initialize sm2135e output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t sm2182e_init(driver_sm2182e_t *config, void(*hook_func)(void *));

/**
 * @brief Register the sm2182e channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param pin Chip pin
 * @return esp_err_t
 */
esp_err_t sm2182e_regist_channel(sm2182e_channel_t channel, sm2182e_out_pin_t pin);

/**
 * @brief Set only wy channel output
 *
 * @param value_w Output cold value
 * @param value_y Output warm value
 * @return esp_err_t
 */
esp_err_t sm2182e_set_cw_channel(uint16_t value_c, uint16_t value_w);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t sm2182e_set_shutdown(void);

/**
 * @brief Set standby mode
 *
 * @param enable_standby If set to true will enter standby mode
 * @return esp_err_t
 */
esp_err_t sm2182e_set_standby_mode(bool enable_standby);

/**
 * @brief Convert the cw channel current value into the enumeration value required by the driver
 *
 * @param current_mA Only the following current values can be used: 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80
 * @return sm2182e_cw_current_t
 */
sm2182e_cw_current_t sm2182e_cw_current_mapping(int current_mA);

/**
 * @brief Deinitialize sm2182e and release resources
 *
 * @return esp_err_t
 */
esp_err_t sm2182e_deinit(void);
