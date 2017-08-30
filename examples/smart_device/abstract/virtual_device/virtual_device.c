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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_system.h"
#include "virtual_device.h"
#include "socket_device.h"
#include "light_device.h"

socket_handle_t g_socket;
light_dev_handle_t g_light;

esp_err_t virtual_device_init(SemaphoreHandle_t xSemWriteInfo)
{
#ifdef SMART_SOCKET_DEVICE
    g_socket = socket_init(xSemWriteInfo);
#endif
#ifdef SMART_LIGHT_DEVICE
    g_light = light_init();
#endif
    return ESP_OK;
}

esp_err_t virtual_device_write(virtual_device_t vd)
{
#ifdef SMART_SOCKET_DEVICE
    if(g_socket == NULL) {
        return ESP_FAIL;
    }
    socket_status_t socket_sta = 0;
    socket_state_read(g_socket, &socket_sta, 0);
    if (socket_sta != vd.power) {
        socket_state_write(g_socket, vd.power, 0);
    }
    socket_state_read(g_socket, &socket_sta, 1);
    if (vd.temp_value < 50 && socket_sta == SOCKET_ON) {
        socket_state_write(g_socket, SOCKET_OFF, 1);
    } else if(vd.temp_value >= 50 && socket_sta == SOCKET_OFF) {
        socket_state_write(g_socket, SOCKET_ON, 1);
    }
#endif
#ifdef SMART_LIGHT_DEVICE
    if(g_light == NULL) {
        return ESP_FAIL;
    }
    if (vd.power == 0) {
        light_set(g_light, 0, 0);
    }
    else {
        light_set(g_light, vd.light_value, vd.temp_value);
    }
#endif
    return ESP_OK;
}

esp_err_t virtual_device_read(virtual_device_t* vd)
{
#ifdef SMART_SOCKET_DEVICE
    if(g_socket == NULL) {
        return ESP_FAIL;
    }
    socket_status_t socket_sta = 0;
    socket_state_read(g_socket, &socket_sta, 0);
    vd->power = socket_sta;
    socket_state_read(g_socket, &socket_sta, 1);
    vd->temp_value = socket_sta * 50;
#endif
#ifdef SMART_LIGHT_DEVICE
    if (vd->power != 0) {
        vd->temp_value = light_temp_get(g_light);
        vd->light_value = light_bright_get(g_light);
    }
#endif
    return ESP_OK;
}

esp_err_t virtual_device_net_write(virtual_dev_net_t vd_net)
{
#ifdef SMART_SOCKET_DEVICE
    return socket_net_status_write(g_socket, vd_net);
#endif
#ifdef SMART_LIGHT_DEVICE
    return light_net_status_write(g_light, vd_net);
#endif
    return ESP_OK;
}