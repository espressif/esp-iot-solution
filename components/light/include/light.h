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

#include "driver/ledc.h"

//#define LIGHT_CHANNEL_MAX   8
#define LIGHT_TIMER_BIT     LEDC_TIMER_15_BIT
#define LIGHT_FULL_DUTY     ((1 << LIGHT_TIMER_BIT) - 1)

typedef void* light_handle_t;

/**
  * @brief  light initialize
  *
  * @param  timer the ledc timer used by light 
  * @param  speed_mode
  * @param  freq_hz frequency of timer
  * @param  channel_num decide how many channels the light contains
  *
  * @return  the handle of light
  */
light_handle_t light_create(ledc_timer_t timer, ledc_mode_t speed_mode, uint32_t freq_hz, uint8_t channel_num);

/**
  * @brief  add an output channel to light
  *
  * @param  light_handle 
  * @param  channel_id the id of channel (0 ~ channel_num-1)
  * @param  io_num
  * @param  channel the ledc channel you want to use
  * @param  mode 
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t light_channel_regist(light_handle_t light_handle, uint8_t channel_id, gpio_num_t io_num, ledc_channel_t channel, ledc_mode_t mode);

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
esp_err_t light_duty_write(light_handle_t light_handle, uint8_t channel_id, uint32_t duty);

#endif