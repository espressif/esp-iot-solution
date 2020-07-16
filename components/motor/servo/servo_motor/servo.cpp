// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifdef __cplusplus
extern "C" {
#endif

#include <esp_types.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp32/rom/gpio.h"
#include "driver/ledc.h"
#include "driver/pcnt.h"
#include "soc/ledc_struct.h"
#include "iot_servo.h"

static const char* SERVO_TAG = "servo";
#define SERVO_CHECK(a, str, ret_val) \
    if (!(a)) { \
        ESP_LOGE(SERVO_TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str); \
        return (ret_val); \
    }

#define SERVO_LEDC_INIT_BITS LEDC_TIMER_15_BIT
#define SERVO_LEDC_INIT_FREQ (50)
#define SERVO_DUTY_MIN_US    (500)
#define SERVO_DUTY_MAX_US    (2500)
#define SERVO_SEC_TO_US      (1000000)

CServo::CServo(int servo_io, int max_angle, int min_width_us, int max_width_us, ledc_channel_t chn, ledc_mode_t speed_mode, ledc_timer_t tim_idx)
{
    m_chn = chn;
    m_mode = speed_mode;
    m_timer = tim_idx;
    m_full_duty = (1 << SERVO_LEDC_INIT_BITS) - 1;
    m_freq = SERVO_LEDC_INIT_FREQ;
    m_angle = 0;
    m_max_angle = max_angle;
    m_min_us = min_width_us;
    m_max_us = max_width_us;

    ledc_timer_config_t ledc_timer;
    ledc_timer.duty_resolution = SERVO_LEDC_INIT_BITS; // resolution of PWM duty
    ledc_timer.freq_hz = SERVO_LEDC_INIT_FREQ;                     // frequency of PWM signal
    ledc_timer.speed_mode = m_mode;           // timer mode
    ledc_timer.timer_num = m_timer;            // timer index
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_ch;
    ledc_ch.channel    = m_chn;
    ledc_ch.duty       = 0;          //todo: set init angle
    ledc_ch.gpio_num   = servo_io;
    ledc_ch.speed_mode = m_mode;
    ledc_ch.timer_sel  = m_timer;
    ledc_channel_config(&ledc_ch);
}

esp_err_t CServo::write(float angle)
{
    m_angle = angle;
    float angle_us = angle / m_max_angle * (m_max_us - m_min_us) + m_min_us;
    ESP_LOGD(SERVO_TAG, "angle us: %f", angle_us);
    uint32_t duty = m_full_duty * ((int)angle_us) / (SERVO_SEC_TO_US / m_freq);
    ESP_LOGD(SERVO_TAG, "duty: %d", duty);
    ledc_set_duty(m_mode, m_chn, duty);
    ledc_update_duty(m_mode, m_chn);
    return ESP_OK;
}

CServo::~CServo()
{
}

#ifdef __cplusplus
}
#endif

