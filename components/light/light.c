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
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "light.h"

#define TAG "light"

#define IOT_CHECK(tag, a, ret)  if(!(a)) {       \
        return (ret);                            \
        }
#define ERR_ASSERT(tag, param, ret)  IOT_CHECK(tag, (param) == ESP_OK, ret)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)

typedef struct {
    gpio_num_t io_num;
    ledc_mode_t mode;
    ledc_channel_t channel; 
} light_channel_t;

typedef struct {
    uint8_t channel_num;
    ledc_timer_t ledc_timer;
    light_channel_t* channel_group[0];
} light_t;

static light_channel_t* light_channel_create(gpio_num_t io_num, ledc_channel_t channel, ledc_mode_t mode, ledc_timer_t timer)
{
    ledc_channel_config_t ledc_channel = {
        .channel = channel,
        .duty = 0,
        .gpio_num = io_num,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = mode,
        .timer_sel = timer
    };
    ERR_ASSERT(TAG, ledc_channel_config(&ledc_channel), NULL);
    light_channel_t* pwm = (light_channel_t*)calloc(1, sizeof(light_channel_t));
    pwm->io_num = io_num;
    pwm->channel = channel;
    pwm->mode = mode;
    return pwm;
}

static esp_err_t light_channel_delete(light_channel_t* light_channel)
{
    POINT_ASSERT(TAG, light_channel);
    free(light_channel);
    return ESP_OK;
}

light_handle_t light_create(ledc_timer_t timer, ledc_mode_t speed_mode, uint32_t freq_hz, uint8_t channel_num) 
{
    IOT_CHECK(TAG, channel_num != 0, NULL);
    ledc_timer_config_t timer_conf = {
        .timer_num = timer,
        .speed_mode = speed_mode,
        .freq_hz = freq_hz,
        .bit_num = LIGHT_TIMER_BIT
    };
    ERR_ASSERT(TAG, ledc_timer_config( &timer_conf), NULL);
    light_t* light_ptr = (light_t*)calloc(1, sizeof(light_t) + sizeof(light_channel_t*) * (channel_num - 1));
    light_ptr->channel_num = channel_num;
    light_ptr->ledc_timer = timer;
    for (int i = 0; i < channel_num; i++) {
        light_ptr->channel_group[i] = NULL;
    }
    return (light_handle_t)light_ptr;
}

esp_err_t light_delete(light_handle_t light_handle)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    for (int i = 0; i < light->channel_num; i++) {
        if (light->channel_group[i] != NULL) {
            light_channel_delete(light->channel_group[i]);
        }
    }
    free(light_handle);
    return ESP_OK;
}

esp_err_t light_channel_regist(light_handle_t light_handle, uint8_t channel_id, gpio_num_t io_num, ledc_channel_t channel, ledc_mode_t mode)
{
    light_t* light = (light_t*)light_handle;
    IOT_CHECK(TAG, channel_id < light->channel_num, FAIL);
    light->channel_group[channel_id] = light_channel_create(io_num, channel, mode, light->ledc_timer);
    return ESP_OK;
}

esp_err_t light_duty_write(light_handle_t light_handle, uint8_t channel_id, uint32_t duty)
{
    light_t* light = (light_t*)light_handle;
    IOT_CHECK(TAG, channel_id < light->channel_num, FAIL);
    POINT_ASSERT(TAG, light->channel_group[channel_id]);
    ledc_set_duty(light->channel_group[channel_id]->mode, light->channel_group[channel_id]->channel, duty);
    ledc_update_duty(light->channel_group[channel_id]->mode, light->channel_group[channel_id]->channel);
    return ESP_OK;
}