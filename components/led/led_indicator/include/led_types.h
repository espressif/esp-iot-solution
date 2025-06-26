/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
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
 * @brief LED state: 0-100, only hardware that supports to set brightness can adjust brightness.
 *
 */
enum {
    LED_STATE_OFF = 0,           /*!< turn off the LED */
    LED_STATE_25_PERCENT = 64,   /*!< 25% brightness, must support to set brightness */
    LED_STATE_50_PERCENT = 128,  /*!< 50% brightness, must support to set brightness */
    LED_STATE_75_PERCENT = 191,  /*!< 75% brightness, must support to set brightness */
    LED_STATE_ON = UINT8_MAX,    /*!< turn on the LED */
};

/**
 * @brief actions in this type
 *
 */
typedef enum {
    LED_BLINK_STOP = -1,   /*!< stop the blink */
    LED_BLINK_HOLD,        /*!< hold the on-off state */
    LED_BLINK_BREATHE,     /*!< breathe state */
    LED_BLINK_BRIGHTNESS,  /*!< set the brightness, it will transition from the old brightness to the new brightness */
    LED_BLINK_RGB,         /*!< color change with R(0-255) G(0-255) B(0-255) */
    LED_BLINK_RGB_RING,    /*!< Gradual color transition from old color to new color in a color ring */
    LED_BLINK_HSV,         /*!< color change with H(0-360) S(0-255) V(0-255) */
    LED_BLINK_HSV_RING,    /*!< Gradual color transition from old color to new color in a color ring */
    LED_BLINK_LOOP,        /*!< loop from first step */
} blink_step_type_t;

/**
 * @brief one blink step, a meaningful signal consists of a group of steps
 *
 */
typedef struct {
    blink_step_type_t type;          /*!< action type in this step */
    uint32_t value;                  /*!< hold on or off, set 0 if LED_BLINK_STOP() or LED_BLINK_LOOP */
    uint32_t hold_time_ms;           /*!< hold time(ms), set 0 if not LED_BLINK_HOLD */
} blink_step_t;

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

typedef struct {
    blink_step_t const **blink_lists;           /*!< user defined LED blink lists */
    uint16_t blink_list_num;                    /*!< number of blink lists */
} led_indicator_config_t;

typedef void *led_indicator_handle_t; /*!< LED indicator operation handle */

#ifdef __cplusplus
}
#endif
