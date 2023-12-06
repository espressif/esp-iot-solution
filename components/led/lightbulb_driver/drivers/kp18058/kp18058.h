/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#pragma once

/**
 * @brief voltage Compensation options
 *
 */
typedef enum {
    KP18058_COMPENSATION_VOLTAGE_140V = 0,
    KP18058_COMPENSATION_VOLTAGE_145V,
    KP18058_COMPENSATION_VOLTAGE_150V,
    KP18058_COMPENSATION_VOLTAGE_155V,
    KP18058_COMPENSATION_VOLTAGE_160V,
    KP18058_COMPENSATION_VOLTAGE_165V,
    KP18058_COMPENSATION_VOLTAGE_170V,
    KP18058_COMPENSATION_VOLTAGE_260V,
    KP18058_COMPENSATION_VOLTAGE_270V,
    KP18058_COMPENSATION_VOLTAGE_280V,
    KP18058_COMPENSATION_VOLTAGE_290V,
    KP18058_COMPENSATION_VOLTAGE_300V,
    KP18058_COMPENSATION_VOLTAGE_310V,
    KP18058_COMPENSATION_VOLTAGE_320V,
    KP18058_COMPENSATION_VOLTAGE_330V,
} kp18058_compensation_t;

/**
 * @brief Voltage compensation slope
 *
 */
typedef enum {
    KP18058_SLOPE_7_5 = 0,
    KP18058_SLOPE_10_0,
    KP18058_SLOPE_12_5,
    KP18058_SLOPE_15_0,
} kp18058_slope_t;

/**
 * @brief Chopping frequency
 *
 */
typedef enum {
    KP18058_CHOPPING_2KHZ = 0,
    KP18058_CHOPPING_1KHZ,
    KP18058_CHOPPING_500HZ,
    KP18058_CHOPPING_250HZ,
} kp18058_chopping_freq_t;

/**
 * @brief Output configuration
 *
 */
typedef struct {
    uint8_t rgb_current_multiple;   // Range 1-31, the base value is 1.5mA. If the multiple is 10, will output 15mA current.
    uint8_t cw_current_multiple;    // Range 1-31, the base value is 2.5mA. If the multiple is 10, will output 25mA current.
    gpio_num_t iic_clk;
    gpio_num_t iic_sda;
    uint16_t iic_freq_khz;
    bool enable_iic_queue : 1;
    bool enable_custom_param : 1;   // If enabled will customize byte1-3, Otherwise use default parameters.
    struct {
        kp18058_compensation_t compensation;
        kp18058_slope_t slope;
        kp18058_chopping_freq_t chopping_freq;
        bool disable_voltage_compensation : 1;
        bool disable_chopping_dimming : 1;
        bool enable_rc_filter : 1;
    } custom_param;
} driver_kp18058_t;

/**
 * @brief kp18058 channel abstract definition
 *
 */
typedef enum {
    KP18058_CHANNEL_R = 0,
    KP18058_CHANNEL_G,
    KP18058_CHANNEL_B,
    KP18058_CHANNEL_C,
    KP18058_CHANNEL_W,
    KP18058_CHANNEL_MAX,
} kp18058_channel_t;

/**
 * @brief kp18058 output pin definition
 *
 */
typedef enum {
    KP18058_PIN_OUT1 = 0,
    KP18058_PIN_OUT2,
    KP18058_PIN_OUT3,
    KP18058_PIN_OUT4,
    KP18058_PIN_OUT5,
    KP18058_PIN_OUT_MAX,
} kp18058_out_pin_t;

/**
 * @brief Initialize kp18058 output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t kp18058_init(driver_kp18058_t *config, void(*hook_func)(void *));

/**
 * @brief Set any channel output
 *
 * @param channel Abstract pin
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t kp18058_set_channel(kp18058_channel_t channel, uint16_t value);

/**
 * @brief Register the kp18058 channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param pin Chip pin
 * @return esp_err_t
 */
esp_err_t kp18058_regist_channel(kp18058_channel_t channel, kp18058_out_pin_t pin);

/**
 * @brief Set only rgb channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t kp18058_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b);

/**
 * @brief Set only cw channel output
 *
 * @param value_w Output white value
 * @param value_y Output yellow value
 * @return esp_err_t
 */
esp_err_t kp18058_set_cw_channel(uint16_t value_w, uint16_t value_y);

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
esp_err_t kp18058_set_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_c, uint16_t value_w);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t kp18058_set_shutdown(void);

/**
 * @brief Deinitialize kp18058 and release resources
 *
 * @return esp_err_t
 */
esp_err_t kp18058_deinit(void);

/**
 * @brief Set standby mode
 *
 * @param enable_standby If set to true will enter standby mode
 * @return esp_err_t
 */
esp_err_t kp18058_set_standby_mode(bool enable_sleep);
