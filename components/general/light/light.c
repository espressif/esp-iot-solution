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
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "soc/ledc_reg.h"
#include "driver/ledc.h"
#include "iot_light.h"

static const char* TAG = "light";

#define IOT_CHECK(tag, a, ret)  if(!(a)) {       \
        return (ret);                            \
        }
#define ERR_ASSERT(tag, param, ret)  IOT_CHECK(tag, (param) == ESP_OK, ret)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define LIGHT_NUM_MAX   4

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

static light_t* g_light_group[LIGHT_NUM_MAX];
static bool g_fade_installed = false;

static void breath_timer_callback(TimerHandle_t xTimer)
{
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] != NULL) {
            light_t* light = g_light_group[i];
            for (int j = 0; j < light->channel_num; j++) {
                if (light->channel_group[j] != NULL && light->channel_group[j]->timer == xTimer) {
                    light_channel_t* l_chn = light->channel_group[j];
                    ledc_set_fade_with_time(l_chn->mode, l_chn->channel, l_chn->next_duty, l_chn->breath_period / 2);
                    l_chn->next_duty = light->full_duty - l_chn->next_duty;
                    ledc_fade_start(l_chn->mode, l_chn->channel, LEDC_FADE_NO_WAIT);
                }
            }
        }
    }
}

static light_channel_t* light_channel_create(gpio_num_t io_num, ledc_channel_t channel, ledc_mode_t mode, ledc_timer_t timer)
{
    ledc_channel_config_t ledc_channel = {
        .channel = channel,
        .duty = 0,
        .gpio_num = io_num,
        .intr_type = LEDC_INTR_FADE_END,
        .speed_mode = mode,
        .timer_sel = timer
    };
    ERR_ASSERT(TAG, ledc_channel_config(&ledc_channel), NULL);
    light_channel_t* pwm = (light_channel_t*)calloc(1, sizeof(light_channel_t));
    pwm->io_num = io_num;
    pwm->channel = channel;
    pwm->mode = mode;
    pwm->breath_period = 0;
    pwm->timer = NULL;
    pwm->next_duty = 0;
    return pwm;
}

static esp_err_t light_channel_delete(light_channel_t* light_channel)
{
    POINT_ASSERT(TAG, light_channel);
    if (light_channel->timer != NULL) {
        xTimerDelete(light_channel->timer, portMAX_DELAY);
        light_channel->timer = NULL;
    }
    free(light_channel);
    return ESP_OK;
}

light_handle_t iot_light_create(ledc_timer_t timer, ledc_mode_t speed_mode, uint32_t freq_hz, uint8_t channel_num, ledc_timer_bit_t timer_bit) 
{
    IOT_CHECK(TAG, channel_num != 0, NULL);
    ledc_timer_config_t timer_conf = {
        .timer_num = timer,
        .speed_mode = speed_mode,
        .freq_hz = freq_hz,
        .duty_resolution = timer_bit
    };
    ERR_ASSERT(TAG, ledc_timer_config( &timer_conf), NULL);
    light_t* light_ptr = (light_t*)calloc(1, sizeof(light_t) + sizeof(light_channel_t*) * channel_num);
    light_ptr->channel_num = channel_num;
    light_ptr->ledc_timer = timer;
    light_ptr->full_duty = (1 << timer_bit) - 1;
    light_ptr->freq_hz = freq_hz;
    light_ptr->mode = speed_mode;
    light_ptr->timer_bit = timer_bit;
    for (int i = 0; i < channel_num; i++) {
        light_ptr->channel_group[i] = NULL;
    }
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] == NULL) {
            g_light_group[i] = light_ptr;
            break;
        }
    }
    return (light_handle_t)light_ptr;
}

esp_err_t iot_light_delete(light_handle_t light_handle)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    for (int i = 0; i < light->channel_num; i++) {
        if (light->channel_group[i] != NULL) {
            light_channel_delete(light->channel_group[i]);
        }
    }
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] == light) {
            g_light_group[i] = NULL;
            break;
        }
    }
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] != NULL) {
            goto FREE_MEM;
        }
    }
    ledc_fade_func_uninstall(0);
    g_fade_installed = false;
FREE_MEM:
    free(light_handle);
    return ESP_OK;
}

esp_err_t iot_light_channel_regist(light_handle_t light_handle, uint8_t channel_idx, gpio_num_t io_num, ledc_channel_t channel)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    IOT_CHECK(TAG, channel_idx < light->channel_num, FAIL);
    if (light->channel_group[channel_idx] != NULL) {
        ESP_LOGE(TAG, "this channel index has been registered");
        return ESP_FAIL;
    }
    light->channel_group[channel_idx] = light_channel_create(io_num, channel, light->mode, light->ledc_timer);
    if (g_fade_installed == false) {
        ledc_fade_func_install(0);
        g_fade_installed = true;
    }
    return ESP_OK;
}

esp_err_t iot_light_duty_write(light_handle_t light_handle, uint8_t channel_id, uint32_t duty, light_duty_mode_t duty_mode)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    IOT_CHECK(TAG, channel_id < light->channel_num, ESP_FAIL);
    POINT_ASSERT(TAG, light->channel_group[channel_id]);
    light_channel_t* l_chn = light->channel_group[channel_id];
    if(l_chn->timer != NULL) {
        xTimerStop(l_chn->timer, portMAX_DELAY);
    }
    IOT_CHECK(TAG, duty_mode < LIGHT_DUTY_FADE_MAX, ESP_FAIL);
    switch (duty_mode) {
        case LIGHT_SET_DUTY_DIRECTLY:
            ledc_set_duty(l_chn->mode, l_chn->channel, duty);
            ledc_update_duty(l_chn->mode, l_chn->channel);
            break;
        default:
            ledc_set_fade_with_time(l_chn->mode, l_chn->channel, duty, duty_mode * 1000);
            ledc_fade_start(l_chn->mode, l_chn->channel, LEDC_FADE_NO_WAIT);
            break;
    }
    return ESP_OK;
}

esp_err_t iot_light_breath_write(light_handle_t light_handle, uint8_t channel_id, int breath_period_ms)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    IOT_CHECK(TAG, channel_id < light->channel_num, FAIL);
    POINT_ASSERT(TAG, light->channel_group[channel_id]);
    light_channel_t* l_chn = light->channel_group[channel_id];
    if (l_chn->breath_period != breath_period_ms) {
        if(l_chn->timer != NULL) {
            xTimerDelete(l_chn->timer, portMAX_DELAY);
        }
        l_chn->timer = xTimerCreate("light_breath", (breath_period_ms / 2) / portTICK_PERIOD_MS, pdTRUE, (void*) 0, breath_timer_callback);
    }
    l_chn->breath_period = breath_period_ms;
    ledc_set_duty(l_chn->mode, l_chn->channel, 0);
    ledc_update_duty(l_chn->mode, l_chn->channel);
    POINT_ASSERT(TAG, light->channel_group[channel_id]->timer);
    xTimerStart(l_chn->timer, portMAX_DELAY);
    ledc_set_fade_with_time(l_chn->mode, l_chn->channel, light->full_duty, breath_period_ms / 2);
    l_chn->next_duty = 0;
    ledc_fade_start(l_chn->mode, l_chn->channel, LEDC_FADE_NO_WAIT);
    return ESP_OK;
}

esp_err_t iot_light_blink_starte(light_handle_t light_handle, uint32_t channel_mask, uint32_t period_ms)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    IOT_CHECK(TAG, period_ms > 0 && period_ms <= 1000, ESP_FAIL);
    ledc_timer_config_t timer_conf = {
        .timer_num = light->ledc_timer,
        .speed_mode = light->mode,
        .freq_hz = 1000 / period_ms,
        .duty_resolution = LEDC_TIMER_10_BIT,
    };
    ERR_ASSERT(TAG, ledc_timer_config( &timer_conf), ESP_FAIL);
    for (int i = 0; i < light->channel_num; i++) {
        if (light->channel_group[i] != NULL) {
            if (light->channel_group[i]->timer != NULL) {
                xTimerStop(light->channel_group[i]->timer, portMAX_DELAY);
            }
            if (channel_mask & 1<<i) {
                iot_light_duty_write(light_handle, i, (1 << LEDC_TIMER_10_BIT) / 2, LIGHT_SET_DUTY_DIRECTLY);
            } else {
                iot_light_duty_write(light_handle, i, 0, LIGHT_SET_DUTY_DIRECTLY);
            }
        }
    }
    return ESP_OK;
}

esp_err_t iot_light_blink_stop(light_handle_t light_handle)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    ledc_timer_config_t timer_conf = {
        .timer_num = light->ledc_timer,
        .speed_mode = light->mode,
        .freq_hz = light->freq_hz,
        .duty_resolution = light->timer_bit,
    };
    ERR_ASSERT(TAG, ledc_timer_config( &timer_conf), ESP_FAIL);
    for (int i = 0; i < light->channel_num; i++) {
        iot_light_duty_write(light_handle, i, 0, LIGHT_SET_DUTY_DIRECTLY);
    }
    return ESP_OK;
}
