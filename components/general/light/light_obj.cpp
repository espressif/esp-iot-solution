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
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "iot_light.h"
#include "driver/ledc.h"

static const char* TAG = "LIGHT_CPP";

CLight::CLight(light_channel_num_t channel_num, uint32_t freq_hz, ledc_timer_t timer, ledc_timer_bit_t timer_bit,
        ledc_mode_t speed_mode) :
        m_full_duty((1 << timer_bit) - 1), m_channel_num(channel_num), red(&m_light_handle, 0, m_full_duty),
        green(&m_light_handle, 1, m_full_duty), blue(&m_light_handle, 2, m_full_duty), cw(&m_light_handle, 3, m_full_duty),
        ww(&m_light_handle, 4, m_full_duty)
{
    m_light_handle = iot_light_create(timer, speed_mode, freq_hz, channel_num, timer_bit);
}

CLight::~CLight()
{
    iot_light_delete(m_light_handle);
    m_light_handle = NULL;
}

esp_err_t CLight::blink_start(uint32_t channel_mask, uint32_t period_ms)
{
    return iot_light_blink_starte(m_light_handle, channel_mask, period_ms);
}

esp_err_t CLight::blink_stop()
{
    return iot_light_blink_stop(m_light_handle);
}

uint32_t CLight::get_full_duty()
{
    return m_full_duty;
}

esp_err_t CLight::on()
{
    for (int i = 0; i < m_channel_num; i++) {
        m_channels[i]->duty(m_full_duty);
    }
    return ESP_OK;
}

esp_err_t CLight::off()
{
    for (int i = 0; i < m_channel_num; i++) {
        m_channels[i]->duty(0);
    }
    return ESP_OK;
}

CLight::CLightChannel::CLightChannel(light_handle_t *p_light_handle, int idx, uint32_t full_duty)
{
    mp_channel_handle = p_light_handle;
    m_ch_idx = idx;
    m_ch_full_duty = full_duty;
}

esp_err_t CLight::CLightChannel::check_init()
{
    if ((m_io_num == GPIO_NUM_MAX) || (m_ledc_idx == LEDC_CHANNEL_MAX)) {
        ESP_LOGE(TAG, "Light channel not initialized!");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t CLight::CLightChannel::init(gpio_num_t io_num, ledc_channel_t ledc_channel)
{
    m_io_num = io_num;
    m_ledc_idx = ledc_channel;
    light_handle_t light_handle = *mp_channel_handle;
    return iot_light_channel_regist(light_handle, m_ch_idx, io_num, ledc_channel);
}

esp_err_t CLight::CLightChannel::on()
{
    if (check_init() != ESP_OK) {
        return ESP_FAIL;
    }
    light_handle_t light_handle = *mp_channel_handle;
    m_duty = m_ch_full_duty;
    return iot_light_duty_write(light_handle, m_ch_idx, m_ch_full_duty, LIGHT_SET_DUTY_DIRECTLY);
}

esp_err_t CLight::CLightChannel::off()
{
    if (check_init() != ESP_OK) {
        return ESP_FAIL;
    }
    light_handle_t light_handle = *mp_channel_handle;
    m_duty = 0;
    return iot_light_duty_write(light_handle, m_ch_idx, 0, LIGHT_SET_DUTY_DIRECTLY);
}

uint32_t CLight::CLightChannel::duty()
{
    return m_duty;
}

esp_err_t CLight::CLightChannel::duty(uint32_t duty_val, light_duty_mode_t duty_mode)
{
    if (check_init() != ESP_OK) {
        return ESP_FAIL;
    }
    light_handle_t light_handle = *mp_channel_handle;
    if (duty() == duty_val) {
        return ESP_OK;
    }
    esp_err_t ret = iot_light_duty_write(light_handle, m_ch_idx, duty_val, duty_mode);
    if (ret == ESP_OK) {
        m_duty = duty_val;
    }
    return ret;
}

esp_err_t CLight::CLightChannel::breath(int breath_period_ms)
{
    if (check_init() != ESP_OK) {
        return ESP_FAIL;
    }
    light_handle_t light_handle = *mp_channel_handle;
    return iot_light_breath_write(light_handle, m_ch_idx, breath_period_ms);
}


