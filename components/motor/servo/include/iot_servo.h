/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_SERVO_H_
#define _IOT_SERVO_H_

#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

/**
 * @brief Configuration of servo motor channel
 *
 */
typedef struct {
    gpio_num_t servo_pin[LEDC_CHANNEL_MAX];     /**< Pin number of pwm output */
    ledc_channel_t ch[LEDC_CHANNEL_MAX];    /**< The ledc channel which used */
} servo_channel_t;

/**
 * @brief Configuration of servo motor
 *
 */
typedef struct {
    uint16_t max_angle;        /**< Servo max angle */
    uint16_t min_width_us;     /**< Pulse width corresponding to minimum angle, which is usually 500us */
    uint16_t max_width_us;     /**< Pulse width corresponding to maximum angle, which is usually 2500us */
    uint32_t freq;             /**< PWM frequency */
    ledc_timer_t timer_number; /**< Timer number of ledc */
    servo_channel_t channels;  /**< Channels to use */
    uint8_t channel_number;    /**< Total channel number */
} servo_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ledc to control the servo
 *
 * @param speed_mode Select the LEDC channel group with specified speed mode. Note that not all targets support high speed mode.
 * @param config Pointer of servo configure struct
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Configure ledc failed
 */
esp_err_t iot_servo_init(ledc_mode_t speed_mode, const servo_config_t *config);

/**
 * @brief Deinitialize ledc for servo
 *
 * @param speed_mode Select the LEDC channel group with specified speed mode.
 *
 * @return
 *     - ESP_OK Success
 */
esp_err_t iot_servo_deinit(ledc_mode_t speed_mode);

/**
 * @brief Set the servo motor to a certain angle
 *
 * @note This API is not thread-safe
 *
 * @param speed_mode Select the LEDC channel group with specified speed mode.
 * @param channel LEDC channel, select from ledc_channel_t
 * @param angle The angle to go
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t iot_servo_write_angle(ledc_mode_t speed_mode, uint8_t channel, float angle);

/**
 * @brief Read current angle of one channel
 *
 * @param speed_mode Select the LEDC channel group with specified speed mode.
 * @param channel LEDC channel, select from ledc_channel_t
 * @param angle Current angle of the channel
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t iot_servo_read_angle(ledc_mode_t speed_mode, uint8_t channel, float *angle);

#ifdef __cplusplus
}
#endif

#endif /* _IOT_SERVO_H_ */
