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

#include <stdlib.h>
#include "iot_light.h"
#include "light_device.h"
#include "light_config.h"
#include "iot_param.h"
#include "iot_button.h"

typedef struct {
    int ledc_channel;
    int ledc_gpio;
} light_conf_t;

static const light_conf_t LIGHT_PWM_UNIT[IOT_LIGHT_CHANNEL_NUM] = {
    {
        .ledc_channel = IOT_LIGHT_PWM_U0_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U0_LEDC_IO,
    },
#if IOT_LIGHT_CHANNEL_NUM > 1
    {
        .ledc_channel = IOT_LIGHT_PWM_U1_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U1_LEDC_IO,
    },
#endif
#if IOT_LIGHT_CHANNEL_NUM > 2
    {
        .ledc_channel = IOT_LIGHT_PWM_U2_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U2_LEDC_IO,
    },
#endif
#if IOT_LIGHT_CHANNEL_NUM > 3
    {
        .ledc_channel = IOT_LIGHT_PWM_U3_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U3_LEDC_IO,
    },
#endif
#if IOT_LIGHT_CHANNEL_NUM > 4
    {
        .ledc_channel = IOT_LIGHT_PWM_U4_LEDC_CHANNEL,
        .ledc_gpio    = IOT_LIGHT_PWM_U4_LEDC_IO,
    },
#endif
};

typedef struct {
    uint8_t bright;
    uint8_t temp;
} save_param_t;

typedef struct {
    light_handle_t light;
    save_param_t light_param;
} light_device_t;

light_dev_handle_t light_init()
{
    light_device_t* light_dev = (light_device_t*)calloc(1, sizeof(light_device_t));
    light_handle_t light = iot_light_create(IOT_LIGHT_PWM_TIMER_IDX, IOT_LIGHT_PWM_SPEED_MODE, IOT_LIGHT_FREQ_HZ, IOT_LIGHT_CHANNEL_NUM, IOT_LIGHT_PWM_BIT_NUM);
    light_dev->light = light;
    for(int i = 0; i< IOT_LIGHT_CHANNEL_NUM; i ++) {
        iot_light_channel_regist(light, i, LIGHT_PWM_UNIT[i].ledc_gpio, LIGHT_PWM_UNIT[i].ledc_channel);
    }

    if (iot_param_load(IOT_LIGHT_NAME_SPACE, IOT_LIGHT_PARAM_KEY, &(light_dev->light_param)) != ESP_OK) {
        light_set((light_dev_handle_t)light_dev, 0, 0);
    }
    else {
        light_set((light_dev_handle_t)light_dev, light_dev->light_param.bright, light_dev->light_param.temp); 
    }
    light_net_status_write(light, LIGHT_STA_DISCONNECTED);
    return light_dev;
}

static esp_err_t light_state_write(light_handle_t light_handle, uint32_t duty[])
{
    if (light_handle ==NULL) {
        return ESP_FAIL;
    }
    for (int i = 0; i < IOT_LIGHT_CHANNEL_NUM; i++) {
        iot_light_duty_write(light_handle, i, duty[i], LIGHT_DUTY_FADE_2S);
    }
    return ESP_OK;
}

esp_err_t light_set(light_dev_handle_t light_dev, uint8_t bright, uint8_t temp)
{
    if (light_dev ==NULL) {
        return ESP_FAIL;
    }
    uint32_t sum_duty = bright * LIGHT_FULL_DUTY / 100;
    uint32_t r_duty = sum_duty * sum_duty / 1000 >= LIGHT_FULL_DUTY ? LIGHT_FULL_DUTY : sum_duty * sum_duty / 1000;
    uint32_t duty[IOT_LIGHT_CHANNEL_NUM] = {r_duty, (14700 + temp*108) * sum_duty / 25500 / 2, (4100 + temp*214) * sum_duty / 25500 / 2};
    light_device_t* light = (light_device_t*)light_dev;
    light->light_param.bright = bright;
    light->light_param.temp = temp;
    iot_param_save(IOT_LIGHT_NAME_SPACE, IOT_LIGHT_PARAM_KEY, &light->light_param, sizeof(save_param_t));
    return light_state_write(light->light, duty);
}

esp_err_t light_net_status_write(light_dev_handle_t light_handle, light_net_status_t net_status)
{
    if (light_handle ==NULL) {
        return ESP_FAIL;
    }
    light_device_t* light_dev = (light_device_t*)light_handle;
    switch (net_status) {
        case LIGHT_STA_DISCONNECTED:
            iot_light_breath_write(light_handle, IOT_CHANNEL_ID_R, 4000);
            iot_light_duty_write(light_handle, IOT_CHANNEL_ID_G, 0, LIGHT_SET_DUTY_DIRECTLY);
            iot_light_duty_write(light_handle, IOT_CHANNEL_ID_B, 0, LIGHT_SET_DUTY_DIRECTLY);
            break;
        case LIGHT_CLOUD_CONNECTED:
            if (iot_param_load(IOT_LIGHT_NAME_SPACE, IOT_LIGHT_PARAM_KEY, &(light_dev->light_param)) != ESP_OK) {
                light_set(light_handle, 0, 0);
            }
            else {
                light_set(light_handle, light_dev->light_param.bright, light_dev->light_param.temp); 
            }
            break;
        default:
            break;
    }   
    return ESP_OK;
}

uint8_t light_bright_get(light_dev_handle_t light_dev)
{
    light_device_t* light = (light_device_t*)light_dev;
    return light->light_param.bright;
}

uint8_t light_temp_get(light_dev_handle_t light_dev)
{
    light_device_t* light = (light_device_t*)light_dev;
    return light->light_param.temp;
}
