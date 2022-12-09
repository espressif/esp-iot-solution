// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include "esp_idf_version.h"

/**
 * @brief Output configuration
 *
 */
typedef struct {
    uint16_t freq_hz;
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
    /* The LEDC driver default active high, if you need active low, please set it to true. */
    bool invert_level;
#endif
} driver_pwm_t;

/**
 * @brief PWM channel abstract definition
 * @note
 *     1. CCT matches BRIGHTNESS
 *     2. COLD matches WARM
 *
 */
typedef enum {
    PWM_CHANNEL_R = 0,
    PWM_CHANNEL_G,
    PWM_CHANNEL_B,
    PWM_CHANNEL_CCT_COLD,
    PWM_CHANNEL_BRIGHTNESS_WARM,
    PWM_CHANNEL_MAX,
} pwm_channel_t;

/**
 * @brief Initialize pwm output
 *
 * @param config Driver configuration
 * @return esp_err_t
 */
esp_err_t pwm_init(driver_pwm_t *config);

/**
 * @brief Register the pwm channel
 * @note Needs to correspond to the real hardware
 *
 * @param channel Abstract channel
 * @param gpio_num Chip GPIO pin
 * @return esp_err_t
 */
esp_err_t pwm_regist_channel(pwm_channel_t channel, gpio_num_t gpio_num);

/**
 * @brief Set any channel output
 *
 * @param channel Abstract channel
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t pwm_set_channel(pwm_channel_t channel, uint16_t value);

/**
 * @brief Set only rgb channel output
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t pwm_set_rgb_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b);

/**
 * @brief Set only cct/brightness or cold/warm channel output
 *
 * @param value_cct_c Output cct or cold value
 * @param value_b_w Output brightness or warm value
 * @return esp_err_t
 */
esp_err_t pwm_set_cctb_or_cw_channel(uint16_t value_cct_c, uint16_t value_b_w);

/**
 * @brief Set all channel outputs
 *
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @param value_cct_c Output cct or cold value
 * @param value_b_w Output brightness or warm value
 * @return esp_err_t
 */
esp_err_t pwm_set_rgbcctb_or_rgbcw_channel(uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_cct_c, uint16_t value_b_w);

/**
 * @brief Stop all channel output
 *
 * @return esp_err_t
 */
esp_err_t pwm_set_shutdown(void);

/**
 * @brief Set output with hardware fade
 *
 * @param channel Abstract pin
 * @param value
 * @param fade_ms
 * @return esp_err_t
 */
esp_err_t pwm_set_hw_fade(pwm_channel_t channel, uint16_t value, int fade_ms);

/**
 * @brief Deinitialize pwm and release resources
 *
 * @return esp_err_t
 */
esp_err_t pwm_deinit(void);

/**
 * @brief Enable/Disable sleep status
 *
 * @param is_enable
 * @return esp_err_t
 */
esp_err_t pwm_set_sleep(bool is_enable);
