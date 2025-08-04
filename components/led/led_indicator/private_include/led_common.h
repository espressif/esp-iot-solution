/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "led_convert.h"
#include "led_types.h"

#define LEDC_MODE       CONFIG_LEDC_SPEED_MODE_VALUE
#define LEDC_DUTY_RES   CONFIG_LEDC_TIMER_BIT_NUM
#define LEDC_FREQ_HZ    CONFIG_LEDC_TIMER_FREQ_HZ

#define LEDC_TIMER_CONFIG(ledc_timer)   \
{                                     \
    .speed_mode = LEDC_MODE,          \
    .duty_resolution = LEDC_DUTY_RES, \
    .timer_num = ledc_timer,          \
    .freq_hz = LEDC_FREQ_HZ,          \
    .clk_cfg = LEDC_AUTO_CLK,         \
}

#define LEDC_CHANNEL_CONFIG(ledc_timer, ledc_channel, gpio) \
{                                                       \
    .speed_mode = LEDC_MODE,                            \
    .timer_sel = ledc_timer,                            \
    .hpoint = 0,                                        \
    .duty = 0,                                          \
    .intr_type = LEDC_INTR_DISABLE,                     \
    .channel = ledc_channel,                            \
    .gpio_num = gpio,                                   \
}

typedef struct {
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);              /*!< Pointer function for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data);                               /*!< Pointer function for Deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);  /*!< Pointer function for setting brightness, must be supported by hardware */
    esp_err_t (*hal_indicator_set_rgb)(void *hardware, uint32_t rgb_value);               /*!< Pointer function for setting rgb, must be supported by hardware */
    esp_err_t (*hal_indicator_set_hsv)(void *hardware, uint32_t hsv_value);               /*!< Pointer function for setting hsv, must be supported by hardware */
    void *hardware_data;                           /*!< Hardware data of the LED indicator */
    int active_blink;                              /*!< Active blink list*/
    int preempt_blink;                             /*!< Highest priority blink list*/
    int *p_blink_steps;                            /*!< Stage of each blink list */
    led_indicator_ihsv_t current_fade_value;       /*!< Current fade value */
    led_indicator_ihsv_t last_fade_value;          /*!< Save the last value. */
    uint16_t fade_value_count;                     /*!< Count the number of fade */
    uint16_t fade_step;                            /*!< Step of fade */
    uint16_t fade_total_step;                      /*!< Total step of fade */
    uint32_t max_duty;                             /*!< Max duty cycle from duty_resolution : 2^duty_resolution -1 */
    SemaphoreHandle_t mutex;                       /*!< Mutex to achieve thread-safe */
    TimerHandle_t h_timer;                         /*!< LED timer handle, invalid if works in pwm mode */
    blink_step_t const **blink_lists;              /*!< User defined LED blink lists */
    uint16_t blink_list_num;                       /*!< Number of blink lists */
} _led_indicator_t;

typedef struct _led_indicator_com_config {
    esp_err_t (*hal_indicator_set_on_off)(void *hardware_data, bool on_off);              /*!< Pointer function for setting on or off */
    esp_err_t (*hal_indicator_deinit)(void *hardware_data);                               /*!< Pointer function for Deinitialization */
    esp_err_t (*hal_indicator_set_brightness)(void *hardware_data, uint32_t brightness);  /*!< Pointer function for setting brightness, must be supported by hardware */
    esp_err_t (*hal_indicator_set_rgb)(void *hardware, uint32_t rgb_value);               /*!< Pointer function for setting rgb, must be supported by hardware */
    esp_err_t (*hal_indicator_set_hsv)(void *hardware, uint32_t hsv_value);               /*!< Pointer function for setting hsv, must be supported by hardware */
    void *hardware_data;                  /*!< GPIO number of the LED indicator */
    blink_step_t const **blink_lists;     /*!< User defined LED blink lists */
    uint16_t blink_list_num;              /*!< Number of blink lists */
    led_indicator_duty_t duty_resolution; /*!< Resolution of duty setting in number of bits. The range of duty values is [0, (2**duty_resolution) -1]. If the brightness cannot be set, set this as 1. */
} _led_indicator_com_config_t;

/**
 * @brief Creates a new LED indicator instance with the specified configuration.
 *
 * @param cfg Pointer to a configuration structure of type _led_indicator_com_config_t
 *            containing initialization parameters for the LED indicator.
 *
 * @return Pointer to the created _led_indicator_t instance on success, or NULL on failure.
 */
_led_indicator_t *_led_indicator_create_com(_led_indicator_com_config_t *cfg);

/**
 * @brief Add a new node to the LED indicator instance.
 *
 * @param[in,out] p_led_indicator Pointer to the LED indicator instance to which the node will be added.
 *
 * @return esp_err_t
 *     - ESP_ERR_INVALID_ARG if parameter is invalid
 *     - ESP_ERR_NOT_FOUND no predefined blink_type found
 *     - ESP_OK Success
 */
esp_err_t _led_indicator_add_node(_led_indicator_t *p_led_indicator);

#ifdef __cplusplus
}
#endif
