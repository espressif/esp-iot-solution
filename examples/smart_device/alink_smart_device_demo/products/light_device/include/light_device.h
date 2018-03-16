/* Smart device Example

   For other examples please check:
   https://github.com/espressif/esp-iot-solution/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#ifndef _IOT_LIGHT_DEVICE_H_
#define _IOT_LIGHT_DEVICE_H_
#include "iot_light.h"

typedef void* light_dev_handle_t;

typedef enum {
    LIGHT_STA_DISCONNECTED,
    LIGHT_CLOUD_DISCONNECTED,    
    LIGHT_CLOUD_CONNECTED,
} light_net_status_t;

light_dev_handle_t light_init();

esp_err_t light_set(light_dev_handle_t light_dev, uint32_t red, uint32_t green, uint32_t blue, uint32_t color_temp, uint32_t luminance);

esp_err_t light_get(light_dev_handle_t light_dev, uint32_t *red, uint32_t *green, uint32_t *blue, uint32_t *color_temp, uint32_t *luminance);

esp_err_t light_net_status_write(light_dev_handle_t light_dev, light_net_status_t net_status);
#endif

