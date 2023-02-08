/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @brief LED duty should be consistent with the physical resolution of the indicator.
 * eg. LED_GPIO_MODE should with LED_DUTY_1_BIT
 *
 */
typedef enum {
    LED_DUTY_1_BIT = 1,    /*!< LED duty resolution of 1 bits */
    LED_DUTY_2_BIT,        /*!< LED duty resolution of 2 bits */
    LED_DUTY_3_BIT,        /*!< LED duty resolution of 3 bits */
    LED_DUTY_4_BIT,        /*!< LED duty resolution of 4 bits */
    LED_DUTY_5_BIT,        /*!< LED duty resolution of 5 bits */
    LED_DUTY_6_BIT,        /*!< LED duty resolution of 6 bits */
    LED_DUTY_7_BIT,        /*!< LED duty resolution of 7 bits */
    LED_DUTY_8_BIT,        /*!< LED duty resolution of 8 bits */
    LED_DUTY_9_BIT,        /*!< LED duty resolution of 9 bits */
    LED_DUTY_10_BIT,       /*!< LED duty resolution of 10 bits */
    LED_DUTY_11_BIT,       /*!< LED duty resolution of 11 bits */
    LED_DUTY_12_BIT,       /*!< LED duty resolution of 12 bits */
    LED_DUTY_13_BIT,       /*!< LED duty resolution of 13 bits */
    LED_DUTY_14_BIT,       /*!< LED duty resolution of 14 bits */
    LED_DUTY_15_BIT,       /*!< LED duty resolution of 15 bits */
} led_indicator_duty_t;

/**
 * @brief LED custom configuration
 *
 */
typedef struct {
    bool is_active_level_high;                                                             /**< Set true if GPIO level is high when light is ON, otherwise false. */
    led_indicator_duty_t duty_resolution;                                                  /*!< Resolution of duty setting in number of bits. The range of duty values is [0, (2**duty_resolution) -1]. If the brightness cannot be set, set this as 1. */
    esp_err_t (*hal_indicator_init)(void *hardware_data );                                 /*!< pointer functions for initialization*/
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);                /*!< pointer functions for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data );                               /*!< pointer functions for deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);   /*!< pointer functions for setting brightness, must be supported by hardware */
    void *hardware_data;                                                                   /*!< user hardware data*/
} led_indicator_custom_config_t;

#ifdef __cplusplus
}
#endif
