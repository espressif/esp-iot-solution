/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "iot_servo.h"
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "servo";

#define SERVO_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }

#define SERVO_LEDC_INIT_BITS LEDC_TIMER_11_BIT
#define SERVO_FREQ_MIN       50
#define SERVO_FREQ_MAX       400

struct servo_t {
    servo_config_t config;
    ledc_timer_bit_t duty_resolution;
    uint32_t full_duty;
};

typedef struct {
    bool used;
    uint32_t freq;
    ledc_timer_bit_t duty_resolution;
    uint8_t channel_count;
} servo_timer_state_t;

typedef struct {
    bool used;
    ledc_timer_t timer_number;
    gpio_num_t gpio_num;
} servo_channel_state_t;

static servo_timer_state_t s_timer_state[LEDC_SPEED_MODE_MAX][LEDC_TIMER_MAX];
static servo_channel_state_t s_channel_state[LEDC_SPEED_MODE_MAX][LEDC_CHANNEL_MAX];

static uint32_t calculate_duty(servo_handle_t servo, float angle)
{
    const servo_config_t *config = &servo->config;
    float angle_us = angle / config->max_angle * (config->max_width_us - config->min_width_us) + config->min_width_us;
    ESP_LOGD(TAG, "angle us: %f", angle_us);
    float duty = (float)servo->full_duty * angle_us * config->freq / 1000000.0f;
    duty = duty > (float)servo->full_duty ? (float)servo->full_duty : duty;
    duty = duty < 0.0f ? 0.0f : duty;
    return (uint32_t)(duty + 0.5f);
}

static float calculate_angle(servo_handle_t servo, uint32_t duty)
{
    const servo_config_t *config = &servo->config;
    float angle_us = (float)duty * 1000000.0f / (float)servo->full_duty / (float)config->freq;
    angle_us -= config->min_width_us;
    angle_us = angle_us < 0.0f ? 0.0f : angle_us;
    float angle = angle_us * config->max_angle / (config->max_width_us - config->min_width_us);
    return angle;
}

static esp_err_t check_resources(const servo_config_t *config, ledc_timer_bit_t duty_resolution)
{
    servo_channel_state_t *channel_state = &s_channel_state[config->speed_mode][config->channel];
    SERVO_CHECK(!channel_state->used, "LEDC channel is already used by servo", ESP_ERR_INVALID_STATE);

    servo_timer_state_t *timer_state = &s_timer_state[config->speed_mode][config->timer_number];
    if (timer_state->used) {
        SERVO_CHECK(timer_state->freq == config->freq, "LEDC timer frequency conflict", ESP_ERR_INVALID_STATE);
        SERVO_CHECK(timer_state->duty_resolution == duty_resolution, "LEDC timer duty resolution conflict", ESP_ERR_INVALID_STATE);
    }

    return ESP_OK;
}

static void claim_resources(const servo_config_t *config, ledc_timer_bit_t duty_resolution)
{
    servo_timer_state_t *timer_state = &s_timer_state[config->speed_mode][config->timer_number];
    if (!timer_state->used) {
        timer_state->used = true;
        timer_state->freq = config->freq;
        timer_state->duty_resolution = duty_resolution;
    }
    timer_state->channel_count++;

    servo_channel_state_t *channel_state = &s_channel_state[config->speed_mode][config->channel];
    channel_state->used = true;
    channel_state->timer_number = config->timer_number;
    channel_state->gpio_num = config->gpio_num;
}

static bool release_resources(const servo_config_t *config)
{
    servo_channel_state_t *channel_state = &s_channel_state[config->speed_mode][config->channel];
    channel_state->used = false;
    channel_state->timer_number = 0;
    channel_state->gpio_num = GPIO_NUM_NC;

    servo_timer_state_t *timer_state = &s_timer_state[config->speed_mode][config->timer_number];
    if (timer_state->channel_count > 0) {
        timer_state->channel_count--;
    }

    if (timer_state->channel_count == 0) {
        timer_state->used = false;
        timer_state->freq = 0;
        timer_state->duty_resolution = 0;
        return true;
    }

    return false;
}

esp_err_t iot_servo_new(const servo_config_t *config, servo_handle_t *ret_servo)
{
    SERVO_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(NULL != ret_servo, "Pointer of servo handle is invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->speed_mode < LEDC_SPEED_MODE_MAX, "LEDC speed mode invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->timer_number < LEDC_TIMER_MAX, "LEDC timer number too large", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->channel < LEDC_CHANNEL_MAX, "LEDC channel number too large", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(GPIO_IS_VALID_OUTPUT_GPIO(config->gpio_num), "servo gpio invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->freq <= SERVO_FREQ_MAX && config->freq >= SERVO_FREQ_MIN, "Servo pwm frequency out the range", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->max_angle > 0, "Servo max angle is invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->min_width_us < config->max_width_us, "Servo pulse width range is invalid", ESP_ERR_INVALID_ARG);
    *ret_servo = NULL;
    SERVO_CHECK(!s_channel_state[config->speed_mode][config->channel].used, "LEDC channel is already used by servo", ESP_ERR_INVALID_STATE);

    bool timer_is_new = !s_timer_state[config->speed_mode][config->timer_number].used;
    ledc_timer_bit_t duty_resolution = SERVO_LEDC_INIT_BITS;

    esp_err_t ret = check_resources(config, duty_resolution);
    SERVO_CHECK(ESP_OK == ret, "LEDC resource conflict", ret);

    servo_handle_t servo = calloc(1, sizeof(struct servo_t));
    SERVO_CHECK(NULL != servo, "No memory for servo instance", ESP_ERR_NO_MEM);
    servo->config = *config;
    servo->duty_resolution = duty_resolution;
    servo->full_duty = (1 << servo->duty_resolution) - 1;

    if (timer_is_new) {
        ledc_timer_config_t ledc_timer = {
            .clk_cfg = LEDC_AUTO_CLK,
            .duty_resolution = servo->duty_resolution,
            .freq_hz = config->freq,
            .speed_mode = config->speed_mode,
            .timer_num = config->timer_number,
        };
        ret = ledc_timer_config(&ledc_timer);
        if (ret != ESP_OK) {
            free(servo);
            ESP_LOGE(TAG, "%s(%d): ledc timer configuration failed", __FUNCTION__, __LINE__);
            return ESP_FAIL;
        }
    }

    ledc_channel_config_t ledc_ch = {
        .intr_type  = LEDC_INTR_DISABLE,
        .channel    = config->channel,
        .duty       = calculate_duty(servo, 0),
        .gpio_num   = config->gpio_num,
        .speed_mode = config->speed_mode,
        .timer_sel  = config->timer_number,
        .hpoint     = 0,
    };
    ret = ledc_channel_config(&ledc_ch);
    if (ret != ESP_OK) {
        if (timer_is_new) {
            ledc_timer_rst(config->speed_mode, config->timer_number);
        }
        free(servo);
        ESP_LOGE(TAG, "%s(%d): ledc channel configuration failed", __FUNCTION__, __LINE__);
        return ESP_FAIL;
    }

    claim_resources(config, servo->duty_resolution);
    *ret_servo = servo;
    return ESP_OK;
}

esp_err_t iot_servo_del(servo_handle_t servo)
{
    SERVO_CHECK(NULL != servo, "Servo handle is invalid", ESP_ERR_INVALID_ARG);
    esp_err_t ret = ledc_stop(servo->config.speed_mode, servo->config.channel, 0);
    SERVO_CHECK(ESP_OK == ret, "stop servo channel failed", ESP_FAIL);

    bool timer_unused = release_resources(&servo->config);
    esp_err_t reset_ret = ESP_OK;
    if (timer_unused) {
        reset_ret = ledc_timer_rst(servo->config.speed_mode, servo->config.timer_number);
    }
    if (reset_ret != ESP_OK) {
        ESP_LOGW(TAG, "reset servo timer failed");
    }
    free(servo);
    return ESP_OK;
}

esp_err_t iot_servo_write_angle(servo_handle_t servo, float angle)
{
    SERVO_CHECK(NULL != servo, "Servo handle is invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(angle >= 0.0f, "Angle can't to be negative", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(angle <= servo->config.max_angle, "Angle out of servo range", ESP_ERR_INVALID_ARG);

    uint32_t duty = calculate_duty(servo, angle);
    esp_err_t ret = ledc_set_duty(servo->config.speed_mode, servo->config.channel, duty);
    SERVO_CHECK(ESP_OK == ret, "set servo duty failed", ESP_FAIL);
    ret = ledc_update_duty(servo->config.speed_mode, servo->config.channel);
    SERVO_CHECK(ESP_OK == ret, "update servo duty failed", ESP_FAIL);
    return ESP_OK;
}

esp_err_t iot_servo_read_angle(servo_handle_t servo, float *angle)
{
    SERVO_CHECK(NULL != servo, "Servo handle is invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(NULL != angle, "Pointer of angle is invalid", ESP_ERR_INVALID_ARG);

    uint32_t duty = ledc_get_duty(servo->config.speed_mode, servo->config.channel);
    *angle = calculate_angle(servo, duty);
    return ESP_OK;
}
