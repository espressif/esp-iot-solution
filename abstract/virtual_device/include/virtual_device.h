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

#ifndef _IOT_VIRTUAL_DEVICE_H_
#define _IOT_VIRTUAL_DEVICE_H_
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

#ifdef CONFIG_SMART_SOCKET_DEVICE
#define SMART_SOCKET_DEVICE
#endif
#ifdef CONFIG_SMART_LIGHT_DEVICE
#define SMART_LIGHT_DEVICE
#endif

typedef struct {
    char power;
    char temp_value;
    char light_value;
    char time_delay;
    char work_mode;
} virtual_device_t;

typedef enum {
    VIRTUAL_DEV_STA_DISCONNECTED,    
    VIRTUAL_DEV_CONNECTING_CLOUD,    
    VIRTUAL_DEV_CLOUD_CONNECTED,
} virtual_dev_net_t;

esp_err_t virtual_device_init(SemaphoreHandle_t xSemWriteInfo);

esp_err_t virtual_device_write(virtual_device_t vd);

esp_err_t virtual_device_read(virtual_device_t* vd);

esp_err_t virtual_device_net_write(virtual_dev_net_t vd_net);
#endif