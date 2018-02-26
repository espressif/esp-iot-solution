/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
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

#include "mqtt.h"
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
    mqtt_client *client = (mqtt_client *)param;
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
        mqtt_publish(client, "$dp", buf, len + 3, 0, 0);
        s_device_info_num = 0;

        for (station_info_one = g_station_list->next; station_info_one; station_info_one = g_station_list->next) {
            g_station_list->next = station_info_one->next;
            free(station_info_one);
        }

        vTaskDelay((unsigned long long)ONENET_PUB_INTERVAL * 10000 / portTICK_RATE_MS);
    }
}

void onenet_start(mqtt_client *client)
{
    if (!onenet_initialised) {
        xTaskCreate(&onenet_task, "onenet_task", 2048, client, CONFIG_MQTT_PRIORITY + 1, &xOneNetTask);
        onenet_initialised = true;
    }
}

void onenet_stop(mqtt_client *client)
{
    if (onenet_initialised) {
        if (xOneNetTask) {
            vTaskDelete(xOneNetTask);
        }

        onenet_initialised = false;
    }
}

void connected_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    onenet_start(client);
}

void disconnected_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    onenet_stop(client);
}

void reconnect_cb(void *self, void *params)
{
    mqtt_client *client = (mqtt_client *)self;
    onenet_start(client);
}

void subscribe_cb(void *self, void *params)
{
    printf("Subscribe successful\n");
}

void publish_cb(void *self, void *params)
{
    printf("Publish successful\n");
}

void data_cb(void *self, void *params)
{
    (void)self;
    mqtt_event_data_t *event_data = (mqtt_event_data_t *)params;

    if (event_data->data_offset == 0) {

        char *topic = malloc(event_data->topic_length + 1);
        memcpy(topic, event_data->topic, event_data->topic_length);
        topic[event_data->topic_length] = 0;
        printf("Publish topic: %s\n", topic);
        free(topic);
    }

    printf("Publish data[%d/%d bytes]\n",
           event_data->data_length + event_data->data_offset,
           event_data->data_total_length);
}
