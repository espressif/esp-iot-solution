/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_SERVO_H_
#define _IOT_SERVO_H_

#include "esp_err.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

typedef struct servo_t *servo_handle_t;

/**
 * @brief Configuration of servo motor
 *
 */
typedef struct {
    uint16_t max_angle;          /**< Servo max angle */
    uint16_t min_width_us;       /**< Pulse width corresponding to minimum angle, which is usually 500us */
    uint16_t max_width_us;       /**< Pulse width corresponding to maximum angle, which is usually 2500us */
    uint32_t freq;               /**< PWM frequency */
    ledc_mode_t speed_mode;      /**< LEDC channel group with specified speed mode */
    ledc_timer_t timer_number;   /**< Timer number of ledc */
    ledc_channel_t channel;      /**< LEDC channel to use */
    gpio_num_t gpio_num;         /**< GPIO number of PWM output */
} servo_config_t;

/**
 * @brief Default configuration for a common 180-degree servo.
 *
 * The default PWM settings are 50 Hz with 500 us ~ 2500 us pulse width.
 */
#define SERVO_CONFIG_DEFAULT(_speed_mode, _timer_number, _channel, _gpio_num) \
    {                                                                     \
        .max_angle = 180,                                                 \
        .min_width_us = 500,                                              \
        .max_width_us = 2500,                                             \
        .freq = 50,                                                       \
        .speed_mode = (_speed_mode),                                      \
        .timer_number = (_timer_number),                                  \
        .channel = (_channel),                                            \
        .gpio_num = (_gpio_num),                                          \
    }

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a servo motor instance
 *
 * @param config Pointer of servo configure struct
 * @param ret_servo Pointer to the created servo handle
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_ERR_NO_MEM No memory for servo instance
 *     - ESP_ERR_INVALID_STATE LEDC timer/channel resource conflict
 *     - ESP_FAIL Configure ledc failed
 */
esp_err_t iot_servo_new(const servo_config_t *config, servo_handle_t *ret_servo);

/**
 * @brief Delete a servo motor instance
 *
 * @param servo Servo handle created by iot_servo_new
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Stop ledc channel failed
 */
esp_err_t iot_servo_del(servo_handle_t servo);

/**
 * @brief Set the servo motor to a certain angle
 *
 * @note This API is not thread-safe
 *
 * @param servo Servo handle created by iot_servo_new
 * @param angle The angle to go
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t iot_servo_write_angle(servo_handle_t servo, float angle);

/**
 * @brief Read current angle of the servo
 *
 * @param servo Servo handle created by iot_servo_new
 * @param angle Current angle of the channel
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
esp_err_t iot_servo_read_angle(servo_handle_t servo, float *angle);

#ifdef __cplusplus
}
#endif

#endif /* _IOT_SERVO_H_ */
