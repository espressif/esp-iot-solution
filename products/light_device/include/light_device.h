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

#ifndef _IOT_LIGHT_DEVICE_H_
#define _IOT_LIGHT_DEVICE_H_
#include "light.h"
typedef void* light_dev_handle_t;

typedef enum {
    LIGHT_STA_DISCONNECTED,    
    LIGHT_CONNECTING_CLOUD,    
    LIGHT_CLOUD_CONNECTED,
} light_net_status_t;

light_handle_t light_init();

esp_err_t light_set(light_dev_handle_t light_dev, uint8_t bright, uint8_t temp);

uint8_t light_bright_get(light_dev_handle_t light_dev);

uint8_t light_temp_get(light_dev_handle_t light_dev);

esp_err_t light_net_status_write(light_dev_handle_t light_handle, light_net_status_t net_status);
#endif