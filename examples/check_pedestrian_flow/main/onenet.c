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
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "soc/rtc_cntl_reg.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_client.h"
#include "os.h"
#include "onenet.h"
#include "sniffer.h"

static bool onenet_initialised = false;
static TaskHandle_t xOneNetTask = NULL;

extern station_info_t *g_station_list;
extern int s_device_info_num;
station_info_t *station_info_one = NULL;

void onenet_task(void *param)
{
    esp_mqtt_client_handle_t client = *(esp_mqtt_client_handle_t *)param;
    uint32_t val;

    while (1) {
        val =  s_device_info_num ;
        char buf[128];
        memset(buf, 0, sizeof(buf));
        sprintf(&buf[3], "{\"%s\":%d}", ONENET_DATA_STREAM, val);
        uint16_t len = strlen(&buf[3]);
        buf[0] = data_type_simple_json_without_time;
        buf[1] = len >> 8;
        buf[2] = len & 0xFF;

        esp_mqtt_client_publish(client, "$dp", buf, len + 3, 0, 0);
        s_device_info_num = 0;

        for (station_info_one = g_station_list->next; station_info_one; station_info_one = g_station_list->next) {
            g_station_list->next = station_info_one->next;
            free(station_info_one);
        }

        vTaskDelay((unsigned long long)ONENET_PUB_INTERVAL * 10000 / portTICK_RATE_MS);
    }
}

void onenet_start(esp_mqtt_client_handle_t client)
{
    if (!onenet_initialised) {
        xTaskCreate(&onenet_task, "onenet_task", 2048, &client, 6, &xOneNetTask);
        onenet_initialised = true;
    }
}

void onenet_stop(esp_mqtt_client_handle_t client)
{
    if (onenet_initialised) {
        if (xOneNetTask) {
            vTaskDelete(xOneNetTask);
        }

        onenet_initialised = false;
    }
}