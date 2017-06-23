/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "light.h"

light::light(ledc_timer_t timer, uint8_t channel_num, uint32_t freq_hz = 1000, ledc_timer_bit_t timer_bit = LEDC_TIMER_13_BIT, ledc_mode_t speed_mode = LEDC_HIGH_SPEED_MODE): m_full_duty((1 << timer_bit) - 1), m_channel_num(channel_num) 
{
    m_light_handle = light_create(timer, speed_mode, freq_hz, channel_num, timer_bit);
}

light::~light()
{
    light_delete(m_light_handle);
    m_light_handle = NULL;
}

esp_err_t light::channel_regist(uint8_t channel_idx, gpio_num_t io_num, ledc_channel_t channel, ledc_mode_t mode = LEDC_HIGH_SPEED_MODE)
{
    return light_channel_regist(m_light_handle, channel_idx, io_num, channel, mode);
}

esp_err_t light::duty_write(uint8_t channel_id, uint32_t duty, light_duty_mode_t duty_mode = LIGHT_SET_DUTY_DIRECTLY)
{
    return light_duty_write(m_light_handle, channel_id, duty, duty_mode);
}

esp_err_t light::breath_write(uint8_t channel_id, int breath_period)
{
    return light_breath_write(m_light_handle, channel_id, breath_period);
}

esp_err_t light::blink_start(uint32_t channel_mask, uint32_t period_ms)
{
    return light_blink_start(m_light_handle, channel_mask, period_ms);
}

esp_err_t light::blink_stop()
{
    return light_blink_stop(m_light_handle);
}

uint32_t light::get_full_duty()
{
    return m_full_duty;
}

esp_err_t light::on()
{
    for (int i = 0; i < m_channel_num; i++) {
        light_duty_write(m_light_handle, i, m_full_duty, LIGHT_SET_DUTY_DIRECTLY);
    }
    return ESP_OK;
}

esp_err_t light::off()
{
    for (int i = 0; i < m_channel_num; i++) {
        light_duty_write(m_light_handle, i, 0, LIGHT_SET_DUTY_DIRECTLY);
    }
    return ESP_OK;
}
