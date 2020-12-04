// Copyright 2020 Espressif Systems (Shanghai) Co. Ltd.
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

#include "esp_log.h"
#include "esp_err.h"
#include "driver/ledc.h"
#include "servo.h"

static const char *TAG = "servo";

#define SERVO_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }

#define SERVO_LEDC_INIT_BITS LEDC_TIMER_10_BIT

static uint32_t g_full_duty;
static servo_config_t g_cfg;
static float g_angle[LEDC_CHANNEL_MAX];


static uint32_t calculate_duty(float angle)
{
    float angle_us = angle / g_cfg.max_angle * (g_cfg.max_width_us - g_cfg.min_width_us) + g_cfg.min_width_us;
    ESP_LOGD(TAG, "angle us: %f", angle_us);
    uint32_t duty = g_full_duty * ((int)angle_us) / (1000000 / g_cfg.freq);
    return duty;
}

esp_err_t servo_init(const servo_config_t *config)
{
    esp_err_t ret;

    SERVO_CHECK(NULL != config, "Pointer of config is invalid", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->channel_number > 0 && config->channel_number <= LEDC_CHANNEL_MAX, "Servo channel number out the range", ESP_ERR_INVALID_ARG);
    SERVO_CHECK(config->freq <= 1000 && config->freq >= 50, "Servo pwm frequency out the range", ESP_ERR_INVALID_ARG);

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

    g_full_duty = (1 << SERVO_LEDC_INIT_BITS) - 1;
    g_cfg = *config;

    ledc_timer_config_t ledc_timer;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer.duty_resolution = SERVO_LEDC_INIT_BITS;     // resolution of PWM duty
    ledc_timer.freq_hz = config->freq;                     // frequency of PWM signal
    ledc_timer.speed_mode = config->speed_mode;            // timer mode
    ledc_timer.timer_num = config->timer_number;           // timer index
    ret = ledc_timer_config(&ledc_timer);
    SERVO_CHECK(ESP_OK == ret, "ledc timer configuration failed", ESP_FAIL);

    for (size_t i = 0; i < config->channel_number; i++) {
        ledc_channel_config_t ledc_ch;
        ledc_ch.intr_type = LEDC_INTR_DISABLE;
        ledc_ch.channel    = config->channels.ch[i];
        ledc_ch.duty       = calculate_duty(0);
        ledc_ch.gpio_num   = config->channels.servo_pin[i];
        ledc_ch.speed_mode = config->speed_mode;
        ledc_ch.timer_sel  = config->timer_number;
        ledc_ch.hpoint     = 0;
        ret = ledc_channel_config(&ledc_ch);
        SERVO_CHECK(ESP_OK == ret, "ledc channel configuration failed", ESP_FAIL);
        g_angle[i] = 0.0f;
    }

    return ESP_OK;
}

esp_err_t servo_deinit(void)
{
    for (size_t i = 0; i < g_cfg.channel_number; i++) {
        ledc_stop(g_cfg.speed_mode, g_cfg.channels.ch[i], 0);
    }
    ledc_timer_rst(g_cfg.speed_mode, g_cfg.timer_number);
    g_full_duty = 0;
    return ESP_OK;
}

esp_err_t servo_write_angle(uint8_t channel, float angle)
{
    SERVO_CHECK(channel < LEDC_CHANNEL_MAX, "Servo channel number too large", ESP_ERR_INVALID_ARG);

    esp_err_t ret;
    uint32_t duty = calculate_duty(angle);
    ledc_set_duty(g_cfg.speed_mode, (ledc_channel_t)channel, duty);
    ret = ledc_update_duty(g_cfg.speed_mode, (ledc_channel_t)channel);
    SERVO_CHECK(ESP_OK == ret, "write servo angle failed", ESP_FAIL);

    g_angle[channel] = angle;
    return ESP_OK;
}

esp_err_t servo_read_angle(uint8_t channel, float *angle)
{
    SERVO_CHECK(channel < LEDC_CHANNEL_MAX, "Servo channel number too large", ESP_ERR_INVALID_ARG);

    *angle = g_angle[channel];
    return ESP_OK;
}
