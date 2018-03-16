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
#include "esp_alink.h"
#include "plug_device.h"
#include "light_device.h"
#include "device.h"
#include "parse.h"

#ifdef SMART_LIGHT_DEVICE
static light_dev_handle_t g_light;

static smart_light_dev_t g_light_info = {
    .errorcode         = 0x00,
    .on_off            = 0x01,
    .work_mode         = 0x02,
    .luminance         = 0x50,
    .color_temp        = 0xbb8,
    .red               = 0x20,
    .green             = 0x20,
    .blue              = 0x20,
};
#endif

#ifdef SMART_PLUG_DEVICE
static plug_handle_t g_plug;

static smart_plug_dev_t g_plug_info = {
    .errorcode   = 0x00,
    .on_off      = 0x01,
    .power       = 0x40,
    .rms_current = 0x02,
    .rms_voltage = 0x20,
    .switch_multiple_1 = 0x00,
    .switch_multiple_2 = 0x00,
    .switch_multiple_3 = 0x00,
};
#endif

esp_err_t cloud_device_init(SemaphoreHandle_t xSemWriteInfo)
{
#ifdef SMART_PLUG_DEVICE
    g_plug = plug_init(xSemWriteInfo);
#endif

#ifdef SMART_LIGHT_DEVICE
    g_light = light_init();
#endif
    return ESP_OK;
}

esp_err_t cloud_device_write(char *down_cmd)
{
    if(down_cmd == NULL) {
        return ESP_FAIL;
    }
    
#ifdef SMART_PLUG_DEVICE
    if(g_plug == NULL) {
        return ESP_FAIL;
    }
    
    cloud_device_parse(down_cmd, &g_plug_info);

    if (g_plug_info.on_off == 0) {
        for (int i=0; i<3; i++) {
            plug_state_write(g_plug, PLUG_OFF, i);
        }
    } else {
        plug_status_t plug_sta = 0;
        plug_state_read(g_plug, &plug_sta, 0);
        if (plug_sta != g_plug_info.switch_multiple_1) {
            plug_state_write(g_plug, g_plug_info.switch_multiple_1, 0);
        }

        plug_state_read(g_plug, &plug_sta, 1);
        if (plug_sta != g_plug_info.switch_multiple_2) {
            plug_state_write(g_plug, g_plug_info.switch_multiple_2, 1);
        }

        plug_state_read(g_plug, &plug_sta, 2);
        if (plug_sta != g_plug_info.switch_multiple_3) {
            plug_state_write(g_plug, g_plug_info.switch_multiple_3, 2);
        }
    }
#endif

#ifdef SMART_LIGHT_DEVICE
    cloud_device_parse(down_cmd, &g_light_info);

    if (g_light_info.on_off == 0) {
        light_set(g_light, 0, 0, 0, 2700, 0);
    } else {
        light_set(g_light, g_light_info.red, g_light_info.green, g_light_info.blue, 
            g_light_info.color_temp, g_light_info.luminance);
    }
#endif
    return ESP_OK;
}

esp_err_t cloud_device_read(char *up_cmd)
{
#ifdef SMART_PLUG_DEVICE

    if(g_plug == NULL) {
        return ESP_FAIL;
    }

    plug_powermeter_read(g_plug, &g_plug_info.power, &g_plug_info.rms_current, &g_plug_info.rms_voltage);
    plug_state_read(g_plug, &g_plug_info.switch_multiple_1, 0);
    plug_state_read(g_plug, &g_plug_info.switch_multiple_2, 1);
    plug_state_read(g_plug, &g_plug_info.switch_multiple_3, 2);

    cloud_device_pack(up_cmd, &g_plug_info);
#endif

#ifdef SMART_LIGHT_DEVICE
    /* get light status */
    cloud_device_pack(up_cmd, &g_light_info);
#endif
    return ESP_OK;
}

esp_err_t cloud_device_net_status_write(cloud_dev_net_status_t net_status)
{
#ifdef SMART_PLUG_DEVICE
    switch(net_status) {
    case STA_DISCONNECTED:
        return plug_net_status_write(g_plug, PLUG_STA_DISCONNECTED);

    case CLOUD_DISCONNECTED:
        return plug_net_status_write(g_plug, PLUG_CLOUD_DISCONNECTED);

    case CLOUD_CONNECTED:
        return plug_net_status_write(g_plug, PLUG_CLOUD_CONNECTED);
        
    default:
        break;
    }
#endif

#ifdef SMART_LIGHT_DEVICE
    switch(net_status) {
    case STA_DISCONNECTED:
        return light_net_status_write(g_light, LIGHT_STA_DISCONNECTED);

    case CLOUD_DISCONNECTED:
        return light_net_status_write(g_light, LIGHT_CLOUD_DISCONNECTED);

    case CLOUD_CONNECTED:
        return light_net_status_write(g_light, LIGHT_CLOUD_CONNECTED);

    default:
        break;
    }
#endif
    return ESP_OK;
}

