/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "esp_idf_version.h"
#include "driver/gpio.h"

#define PWM_RGB_CHANNEL_PHASE_DELAY_FLAG        0X01
#define PWM_CW_CHANNEL_PHASE_DELAY_FLAG         0X02
#define PWM_RGBCW_CHANNEL_PHASE_DELAY_FLAG      0X04
/* Dynamic complementary output does not support hardware fade; the Lightbulb HAL automatically uses software fade. */
#define PWM_CHANNEL_COMPLEMENTARY_OUTPUT_FLAG   0X08

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
    struct {
        uint8_t flag;
    } phase_delay;
    bool hw_cct;
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

/** Opaque PWM instance. Serialize operations on the same instance, including deletion. */
typedef struct pwm_context_s *pwm_handle_t;

/**
 * @brief Initialize an independent PWM instance
 *
 * @param config Driver configuration
 * @param hook_func Hook function, which will be called inside the driver. e.g. to notify that config have been changed internally
 * @param user_data User data pointer passed to hook function
 * @param out_handle Created instance; NULL on failure
 * @note Instances share matching-frequency timers and allocate hardware channels automatically.
 *       Do not configure this driver's LEDC resources through other drivers.
 * @return esp_err_t
 */
esp_err_t pwm_init(driver_pwm_t *config, void(*hook_func)(void *, void *), void *user_data, pwm_handle_t *out_handle);

/**
 * @brief Register the pwm channel
 * @note Needs to correspond to the real hardware
 *
 * @param pwm_handle PWM instance handle
 * @param channel Abstract channel
 * @param gpio_num Chip GPIO pin
 * @return esp_err_t
 */
esp_err_t pwm_regist_channel(pwm_handle_t pwm_handle, pwm_channel_t channel, gpio_num_t gpio_num);

/**
 * @brief Set any channel output
 *
 * @param pwm_handle PWM instance handle
 * @param channel Abstract channel
 * @param value Output value
 * @return esp_err_t
 */
esp_err_t pwm_set_channel(pwm_handle_t pwm_handle, pwm_channel_t channel, uint16_t value);

/**
 * @brief Set only rgb channel output
 *
 * @param pwm_handle PWM instance handle
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @return esp_err_t
 */
esp_err_t pwm_set_rgb_channel(pwm_handle_t pwm_handle, uint16_t value_r, uint16_t value_g, uint16_t value_b);

/**
 * @brief Set only cct/brightness or cold/warm channel output
 *
 * @param pwm_handle PWM instance handle
 * @param value_cct_c Output cct or cold value
 * @param value_b_w Output brightness or warm value
 * @return esp_err_t
 */
esp_err_t pwm_set_cctb_or_cw_channel(pwm_handle_t pwm_handle, uint16_t value_cct_c, uint16_t value_b_w);

/**
 * @brief Set all channel outputs
 *
 * @param pwm_handle PWM instance handle
 * @param value_r Output red value
 * @param value_g Output green value
 * @param value_b Output blue value
 * @param value_cct_c Output cct or cold value
 * @param value_b_w Output brightness or warm value
 * @return esp_err_t
 */
esp_err_t pwm_set_rgbcctb_or_rgbcw_channel(pwm_handle_t pwm_handle, uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_cct_c, uint16_t value_b_w);

/**
 * @brief Update every registered channel from a 5-tuple and commit once
 *
 * @param pwm_handle PWM instance handle
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Driver not initialized, or (legacy mode) a
 *        required channel is not registered
 *      - ESP_ERR_INVALID_ARG: Legacy mode only: a requested duty is out of range
 */
esp_err_t pwm_apply_registered(pwm_handle_t pwm_handle, uint16_t value_r, uint16_t value_g, uint16_t value_b, uint16_t value_cct_c, uint16_t value_b_w);

/**
 * @brief Stop all channel output
 *
 * @param pwm_handle PWM instance handle
 * @return esp_err_t
 */
esp_err_t pwm_set_shutdown(pwm_handle_t pwm_handle);

/**
 * @brief Set output with hardware fade
 *
 * @param pwm_handle PWM instance handle
 * @param channel Abstract pin
 * @param value
 * @param fade_ms
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_STATE: Driver not initialized or channel not registered
 *      - ESP_ERR_INVALID_ARG: Duty out of range
 *      - ESP_ERR_NOT_SUPPORTED: Dynamic complementary output is enabled (hardware fade cannot update hpoint)
 */
esp_err_t pwm_set_hw_fade(pwm_handle_t pwm_handle, pwm_channel_t channel, uint16_t value, int fade_ms);

/**
 * @brief Deinitialize pwm and release resources
 * @note Detached GPIOs remain outputs at the inactive level to keep the light off.
 *
 * @param pwm_handle PWM instance handle
 * @note The instance is released even if cleanup reports an error; do not reuse the handle.
 * @return ESP_OK on success, otherwise the combined cleanup error status
 */
esp_err_t pwm_deinit(pwm_handle_t pwm_handle);

/**
 * @brief Enable/Disable sleep status
 *
 * @param pwm_handle PWM instance handle
 * @param is_enable
 * @return esp_err_t
 */
esp_err_t pwm_set_sleep(pwm_handle_t pwm_handle, bool is_enable);
