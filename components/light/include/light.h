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

#ifndef _IOT_LIGHT_H_
#define _IOT_LIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/ledc.h"

typedef void* light_handle_t;

typedef enum {
    LIGHT_SET_DUTY_DIRECTLY = 0,    /*!< set duty directly */
    LIGHT_DUTY_FADE_1S,             /*!< set duty with fade in 1 second */
    LIGHT_DUTY_FADE_2S,             /*!< set duty with fade in 2 second */
    LIGHT_DUTY_FADE_3S,             /*!< set duty with fade in 3 second */
    LIGHT_DUTY_FADE_MAX,            /*!< user shouldn't use this */
} light_duty_mode_t;

/**
  * @brief  light initialize
  *
  * @param  timer the ledc timer used by light 
  * @param  speed_mode
  * @param  freq_hz frequency of timer
  * @param  channel_num decide how many channels the light contains
  * @param  timer_bit 
  *
  * @return  the handle of light
  */
light_handle_t light_create(ledc_timer_t timer, ledc_mode_t speed_mode, uint32_t freq_hz, uint8_t channel_num, ledc_timer_bit_t timer_bit);

/**
  * @brief  add an output channel to light
  *
  * @param  light_handle 
  * @param  channel_idx the id of channel (0 ~ channel_num-1)
  * @param  io_num
  * @param  channel the ledc channel you want to use
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t light_channel_regist(light_handle_t light_handle, uint8_t channel_idx, gpio_num_t io_num, ledc_channel_t channel);

/**
  * @brief  free the momery of light
  *
  * @param  light_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t light_delete(light_handle_t light_handle);

/**
  * @brief  set the duty of a channel
  *
  * @param  light_handle
  * @param  channel_id
  * @param  duty
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t light_duty_write(light_handle_t light_handle, uint8_t channel_id, uint32_t duty, light_duty_mode_t duty_mode);

/**
  * @brief  set a light channel to breath mode
  *
  * @param  light_handle
  * @param  channel_id
  * @param  breath_period
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t light_breath_write(light_handle_t light_handle, uint8_t channel_id, int breath_period);

/**
  * @brief  set some channels to blink,the others would be turned off
  *
  * @param  light_handle
  * @param  channel_mask the mask to choose blink channels
  * @param  period_ms
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t light_blink_start(light_handle_t light_handle, uint32_t channel_mask, uint32_t period_ms);

/**
  * @brief  set some channels to blink,the others would be turned off
  *
  * @param  light_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t light_blink_stop(light_handle_t light_handle);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "controllable_obj.h"
class light : public controllable_obj
{
private:
    light_handle_t m_light_handle;
    uint32_t m_full_duty;
    uint32_t m_channel_num;
    light(const light&);
    light& operator=(const light&);

public:
    light(uint8_t channel_num, uint32_t freq_hz = 1000, ledc_timer_t timer = LEDC_TIMER_0, ledc_timer_bit_t timer_bit = LEDC_TIMER_13_BIT, ledc_mode_t speed_mode = LEDC_HIGH_SPEED_MODE);
    esp_err_t channel_regist(uint8_t channel_idx, gpio_num_t io_num, ledc_channel_t channel);
    esp_err_t duty_write(uint8_t channel_id, uint32_t duty);
    esp_err_t fade_write(uint8_t channel_id, uint32_t duty, light_duty_mode_t duty_mode);
    esp_err_t breath_write(uint8_t channel_id, int breath_period);
    esp_err_t blink_start(uint32_t channel_mask, uint32_t period_ms);
    esp_err_t blink_stop();
    uint32_t get_full_duty();

    virtual esp_err_t on();
    virtual esp_err_t off();
    virtual ~light();
};
#endif

#endif
