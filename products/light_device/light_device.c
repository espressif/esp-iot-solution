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
#include "light.h"
#include "light_device.h"
#include "param.h"
#include "button.h"

#define LIGHT_NAME_SPACE    "param"
#define LIGHT_PARAM_KEY     "light"
#define CHANNEL_NUM     2
#define LIGHT_FREQ_HZ   1000

#define CHANNEL_ID_R    0
#define CHANNEL_ID_B    1
#define CHANNEL_R_IO    18
#define CHANNEL_B_IO    19

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
    light_handle_t light = light_create(LEDC_TIMER_0, LEDC_HIGH_SPEED_MODE, LIGHT_FREQ_HZ, CHANNEL_NUM);
    light_dev->light = light;
    light_channel_regist(light, CHANNEL_ID_R, CHANNEL_R_IO, LEDC_CHANNEL_0, LEDC_HIGH_SPEED_MODE);
    light_channel_regist(light, CHANNEL_ID_B, CHANNEL_B_IO, LEDC_CHANNEL_1, LEDC_HIGH_SPEED_MODE);
    if (param_load(LIGHT_NAME_SPACE, LIGHT_PARAM_KEY, &(light_dev->light_param)) != ESP_OK) {
        light_set((light_dev_handle_t)light_dev, 0, 0);
    }
    else {
        light_set((light_dev_handle_t)light_dev, light_dev->light_param.bright, light_dev->light_param.temp); 
    }
    return light_dev;
}

static esp_err_t light_state_write(light_handle_t light_handle, uint32_t duty[])
{
    if (light_handle ==NULL) {
        return ESP_FAIL;
    }
    for (int i = 0; i < CHANNEL_NUM; i++) {
        light_duty_write(light_handle, i, duty[i]);
    }
    return ESP_OK;
}

esp_err_t light_set(light_dev_handle_t light_dev, uint8_t bright, uint8_t temp)
{
    if (light_dev ==NULL) {
        return ESP_FAIL;
    }
    uint32_t sum_duty = bright * LIGHT_FULL_DUTY / 100;
    uint32_t duty[CHANNEL_NUM] = { temp * sum_duty / 100, (100 - temp) * sum_duty / 100};
    light_device_t* light = (light_device_t*)light_dev;
    light->light_param.bright = bright;
    light->light_param.temp = temp;
    param_save(LIGHT_NAME_SPACE, LIGHT_PARAM_KEY, &light->light_param, sizeof(save_param_t));
    return light_state_write(light->light, duty);
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