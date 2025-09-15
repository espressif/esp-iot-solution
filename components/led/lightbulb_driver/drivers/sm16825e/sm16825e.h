/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "driver/gpio.h"
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SM16825E channel abstract definition
 *
 */
typedef enum {
    SM16825E_CHANNEL_R = 0,    /**< Red channel */
    SM16825E_CHANNEL_G,        /**< Green channel */
    SM16825E_CHANNEL_B,        /**< Blue channel */
    SM16825E_CHANNEL_W,        /**< Cold white channel */
    SM16825E_CHANNEL_Y,        /**< Warm yellow channel */
    SM16825E_CHANNEL_MAX,
} sm16825e_channel_t;

/**
 * @brief SM16825E output pin definition (physical pins on chip)
 *
 */
typedef enum {
    SM16825E_PIN_OUTR = 0,     /**< OUTR pin (Standard pins for red output) */
    SM16825E_PIN_OUTG,         /**< OUTG pin (Standard pins for green output) */
    SM16825E_PIN_OUTB,         /**< OUTB pin (Standard pins for blue output) */
    SM16825E_PIN_OUTW,         /**< OUTW pin (Standard pins for cold white output) */
    SM16825E_PIN_OUTY,         /**< OUTY pin (Standard pins for warm yellow output) */
    SM16825E_PIN_OUT1 = 0,     /**< OUTR pin (Standard pins for red output) */
    SM16825E_PIN_OUT2,         /**< OUTG pin (Standard pins for green output) */
    SM16825E_PIN_OUT3,         /**< OUTB pin (Standard pins for blue output) */
    SM16825E_PIN_OUT4,         /**< OUTW pin (Standard pins for cold white output) */
    SM16825E_PIN_OUT5,         /**< OUTY pin (Standard pins for warm yellow output) */
    SM16825E_PIN_OUT_MAX,
} sm16825e_out_pin_t;

/**
 * @brief Output configuration for SM16825E
 *
 */
typedef struct {
    uint16_t led_num;           /**< Number of LED chips */
    gpio_num_t ctrl_io;        /**< Control GPIO pin */
    uint16_t current[SM16825E_CHANNEL_MAX]; /**< Current value in mA for each channel (10-300mA) */
} driver_sm16825e_t;

/**
 * @brief Initialize SM16825E output
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @return esp_err_t
 */
esp_err_t sm16825e_init(driver_sm16825e_t *config, void(*hook_func)(void *));

/**
 * @brief Deinitialize SM16825E and release resources
 *
 * @return esp_err_t
 */
esp_err_t sm16825e_deinit(void);

/**
 * @brief Mapping of logical addresses and physical pin addresses
 *
 * @param channel Channel index (0=R, 1=G, 2=B, 3=W, 4=Y)
 * @param pin Output pin number (0-4)
 * @return esp_err_t
 */
esp_err_t sm16825e_regist_channel(sm16825e_channel_t channel, sm16825e_out_pin_t pin);

/**
 * @brief Set RGBWY channels output with individual control
 *
 * @param value_r Output red value (0-255)
 * @param value_g Output green value (0-255)
 * @param value_b Output blue value (0-255)
 * @param value_w Output cold white value (0-255)
 * @param value_y Output warm yellow value (0-255)
 * @return esp_err_t
 */
esp_err_t sm16825e_set_rgbwy_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b,
                                     uint16_t value_w, uint16_t value_y);

/**
 * @brief Set current gain for specific channel
 *
 * @param channel Channel index (0=R, 1=G, 2=B, 3=W, 4=Y)
 * @param current_ma Current value in mA (10-300mA)
 * @return esp_err_t
 */
esp_err_t sm16825e_set_channel_current(uint8_t channel, uint16_t current_ma);

/**
 * @brief Enable or disable standby mode
 *
 * @param enable true to enable standby, false to disable
 * @return esp_err_t
 * @note Standby mode reduces power consumption to <2mW
 */
esp_err_t sm16825e_set_standby(bool enable);

/**
 * @brief Shutdown all channels (set to 0)
 *
 * @return esp_err_t
 */
esp_err_t sm16825e_set_shutdown(void);

#ifdef __cplusplus
}
#endif
