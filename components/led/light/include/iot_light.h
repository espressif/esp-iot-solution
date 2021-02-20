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
#ifndef _IOT_LIGHT_H_
#define _IOT_LIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/ledc.h"
#include "freertos/timers.h"

typedef struct {
    gpio_num_t io_num;
    ledc_mode_t mode;
    ledc_channel_t channel; 
    TimerHandle_t timer;
    int breath_period;
    uint32_t next_duty;
} light_channel_t;

typedef struct {
    uint8_t channel_num;
    ledc_mode_t mode;
    ledc_timer_t ledc_timer;
    uint32_t full_duty;
    uint32_t freq_hz;
    ledc_timer_bit_t timer_bit;
    light_channel_t* channel_group[0];
} light_t;
typedef light_t* light_handle_t;

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
light_handle_t iot_light_create(ledc_timer_t timer, ledc_mode_t speed_mode, uint32_t freq_hz, uint8_t channel_num, ledc_timer_bit_t timer_bit);

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
esp_err_t iot_light_channel_regist(light_handle_t light_handle, uint8_t channel_idx, gpio_num_t io_num, ledc_channel_t channel);

/**
  * @brief  free the momery of light
  *
  * @param  light_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_delete(light_handle_t light_handle);

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
esp_err_t iot_light_duty_write(light_handle_t light_handle, uint8_t channel_id, uint32_t duty, light_duty_mode_t duty_mode);

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
esp_err_t iot_light_breath_write(light_handle_t light_handle, uint8_t channel_id, int breath_period);

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
esp_err_t iot_light_blink_starte(light_handle_t light_handle, uint32_t channel_mask, uint32_t period_ms);

/**
  * @brief  set some channels to blink,the others would be turned off
  *
  * @param  light_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_light_blink_stop(light_handle_t light_handle);

#ifdef __cplusplus
}
#endif

#endif
