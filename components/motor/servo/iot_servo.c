/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "iot_servo.h"

static const char *TAG = "servo";

#define SERVO_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }

#define SERVO_LEDC_INIT_BITS LEDC_TIMER_10_BIT
#define SERVO_FREQ_MIN       50
#define SERVO_FREQ_MAX       400

static uint32_t g_full_duty = 0;
static servo_config_t g_cfg[LEDC_SPEED_MODE_MAX] = {0};

static uint32_t calculate_duty(ledc_mode_t speed_mode, float angle)
{
    float angle_us = angle / g_cfg[speed_mode].max_angle * (g_cfg[speed_mode].max_width_us - g_cfg[speed_mode].min_width_us) + g_cfg[speed_mode].min_width_us;
    ESP_LOGD(TAG, "angle us: %f", angle_us);
    uint32_t duty = (uint32_t)((float)g_full_duty * (angle_us) * g_cfg[speed_mode].freq / (1000000.0f));
    return duty;
}

static float calculate_angle(ledc_mode_t speed_mode, uint32_t duty)
{
    float angle_us = (float)duty * 1000000.0f / (float)g_full_duty / (float)g_cfg[speed_mode].freq;
    angle_us -= g_cfg[speed_mode].min_width_us;
    angle_us = angle_us < 0.0f ? 0.0f : angle_us;
    float angle = angle_us * g_cfg[speed_mode].max_angle / (g_cfg[speed_mode].max_width_us - g_cfg[speed_mode].min_width_us);
    return angle;
}

esp_err_t iot_servo_init(ledc_mode_t speed_mode, const servo_config_t *config)
{
    esp_err_t ret;
    SERVO_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->channel_number > 0 && config->channel_number <= LEDC_CHANNEL_MAX, "Servo channel number out the range", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->freq <= SERVO_FREQ_MAX && config->freq >= SERVO_FREQ_MIN, "Servo pwm frequency out the range", ESP_ERR_INVALID_ARG);
    uint64_t pin_mask = 0;
    uint32_t ch_mask = 0;
    for (size_t i = 0; i < config->channel_number; i++) {
        uint64_t _pin_mask = 1ULL << config->channels.servo_pin[i];
        uint32_t _ch_mask = 1UL << config->channels.ch[i];
        SERVO_CHECK(!(pin_mask & _pin_mask), "servo gpio has a duplicate", ESP_ERR_INVALID_ARG);
        SERVO_CHECK(!(ch_mask & _ch_mask), "servo channel has a duplicate", ESP_ERR_INVALID_ARG);
        SERVO_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(config->channels.servo_pin[i]), "servo gpio invalid", ESP_ERR_INVALID_ARG);
        pin_mask |= _pin_mask;
        ch_mask |= _ch_mask;
    }

    ledc_timer_config_t ledc_timer = {
        .clk_cfg = LEDC_AUTO_CLK,
        .duty_resolution = SERVO_LEDC_INIT_BITS,     // resolution of PWM duty
        .freq_hz = config->freq,                     // frequency of PWM signal
        .speed_mode = speed_mode,            // timer mode
        .timer_num = config->timer_number            // timer index
    };
    ret = ledc_timer_config(&ledc_timer);
    SERVO_CHECK(ESP_OK == ret, "ledc timer configuration failed", ESP_FAIL);
    for (size_t i = 0; i < config->channel_number; i++) {
        ledc_channel_config_t ledc_ch = {
            .intr_type  = LEDC_INTR_DISABLE,
            .channel    = config->channels.ch[i],
            .duty       = calculate_duty(speed_mode, 0),
            .gpio_num   = config->channels.servo_pin[i],
            .speed_mode = speed_mode,
            .timer_sel  = config->timer_number,
            .hpoint     = 0
        };
        ret = ledc_channel_config(&ledc_ch);
        SERVO_CHECK(ESP_OK == ret, "ledc channel configuration failed", ESP_FAIL);
    }
    g_full_duty = (1 << SERVO_LEDC_INIT_BITS) - 1;
    g_cfg[speed_mode] = *config;

    return ESP_OK;
}

esp_err_t iot_servo_deinit(ledc_mode_t speed_mode)
{
    SERVO_CHECK(speed_mode < LEDC_SPEED_MODE_MAX, "LEDC speed mode invalid", ESP_ERR_INVALID_ARG);
    for (size_t i = 0; i < g_cfg[speed_mode].channel_number; i++) {
        ledc_stop(speed_mode, g_cfg[speed_mode].channels.ch[i], 0);
    }
    ledc_timer_rst(speed_mode, g_cfg[speed_mode].timer_number);
    g_full_duty = 0;
    return ESP_OK;
}

esp_err_t iot_servo_write_angle(ledc_mode_t speed_mode, uint8_t channel, float angle)
{
    SERVO_CHECK(speed_mode < LEDC_SPEED_MODE_MAX, "LEDC speed mode invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(channel < LEDC_CHANNEL_MAX, "LEDC channel number too large", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(angle >= 0.0f, "Angle can't to be negative", ESP_ERR_INVALID_ARG);
    esp_err_t ret;
    uint32_t duty = calculate_duty(speed_mode, angle);
    ret = ledc_set_duty(speed_mode, (ledc_channel_t)channel, duty);
    ret |= ledc_update_duty(speed_mode, (ledc_channel_t)channel);
    SERVO_CHECK(ESP_OK == ret, "write servo angle failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t iot_servo_read_angle(ledc_mode_t speed_mode, uint8_t channel, float *angle)
{
    SERVO_CHECK(speed_mode < LEDC_SPEED_MODE_MAX, "LEDC speed mode invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(channel < LEDC_CHANNEL_MAX, "LEDC channel number too large", ESP_ERR_INVALID_ARG);
    uint32_t duty = ledc_get_duty(speed_mode, channel);
    float a = calculate_angle(speed_mode, duty);
    *angle = a;
    return ESP_OK;
}
