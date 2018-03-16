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

#ifndef _IOT_DEVICE_H_
#define _IOT_DEVICE_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#ifdef CONFIG_SMART_PLUG_DEVICE
#define SMART_PLUG_DEVICE
#endif
#ifdef CONFIG_SMART_LIGHT_DEVICE
#define SMART_LIGHT_DEVICE
#endif

#ifdef SMART_LIGHT_DEVICE
typedef struct  smart_light_dev {
    uint32_t errorcode;
    uint32_t on_off;
    uint32_t work_mode;
    uint32_t luminance;
    uint32_t color_temp;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
} smart_light_dev_t;
#endif

#ifdef SMART_PLUG_DEVICE
typedef struct smart_plug_dev {
    uint32_t errorcode;
    uint32_t on_off;
    uint32_t power;
    uint32_t rms_current;
    uint32_t rms_voltage;
    uint32_t switch_multiple_1;
    uint32_t switch_multiple_2;
    uint32_t switch_multiple_3;
} smart_plug_dev_t;
#endif

typedef enum {
    STA_DISCONNECTED,
    CLOUD_CONNECTED,
    CLOUD_DISCONNECTED, 
} cloud_dev_net_status_t;

/**
  * @brief  cloud device initialize
  *
  * @param  xSemWriteInfo semaphore for control post data
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_init(SemaphoreHandle_t xSemWriteInfo);

/**
  * @brief  set device status
  *
  * @param  down_cmd json data received from cloud
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_write(char *down_cmd);

/**
  * @brief  get device status
  *
  * @param  up_cmd json data post to cloud
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_read(char *up_cmd);

/**
  * @brief  set network status
  *
  * @param  net_status network status
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: failed
  */
esp_err_t cloud_device_net_status_write(cloud_dev_net_status_t net_status);
#endif

