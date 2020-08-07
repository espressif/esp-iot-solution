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

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "mqtt_client.h"
#include "os.h"
#include "onenet.h"
#include "sniffer.h"

static const char* TAG = "SNIFFER";
static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;
/* Filter out the common ESP32 MAC */
static const uint8_t esp_module_mac[32][3] = {
    {0x54, 0x5A, 0xA6}, {0x24, 0x0A, 0xC4}, {0xD8, 0xA0, 0x1D}, {0xEC, 0xFA, 0xBC},
    {0xA0, 0x20, 0xA6}, {0x90, 0x97, 0xD5}, {0x18, 0xFE, 0x34}, {0x60, 0x01, 0x94},
    {0x2C, 0x3A, 0xE8}, {0xA4, 0x7B, 0x9D}, {0xDC, 0x4F, 0x22}, {0x5C, 0xCF, 0x7F},
    {0xAC, 0xD0, 0x74}, {0x30, 0xAE, 0xA4}, {0x24, 0xB2, 0xDE}, {0x68, 0xC6, 0x3A},
};
/* The device num in ten minutes */
int s_device_info_num           = 0;
station_info_t *station_info    = NULL;
station_info_t *g_station_list  = NULL;
/* MQTT settings about onenet */

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    // your_context_t *context = event->context;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        onenet_start(client);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:break;
    }
    return ESP_OK;
}

static inline uint32_t sniffer_timestamp()
{
    return xTaskGetTickCount() * (1000 / configTICK_RATE_HZ);
}
/* The callback function of sniffer */
void wifi_sniffer_cb(void *recv_buf, wifi_promiscuous_pkt_type_t type)
{
    wifi_promiscuous_pkt_t *sniffer = (wifi_promiscuous_pkt_t *)recv_buf;
    sniffer_payload_t *sniffer_payload = (sniffer_payload_t *)sniffer->payload;

    /* Check if the packet is Probo Request  */
    if (sniffer_payload->header[0] != 0x40) {
        return;
    }

    if (!g_station_list) {
        g_station_list = malloc(sizeof(station_info_t));
        g_station_list->next = NULL;
    }

    /* Check if there is enough memoory to use */
    if (esp_get_free_heap_size() < 60 * 1024) {
        s_device_info_num = 0;

        for (station_info = g_station_list->next; station_info; station_info = g_station_list->next) {
            g_station_list->next = station_info->next;
            free(station_info);
        }
    }
    /* Filter out some useless packet  */
    for (int i = 0; i < 32; ++i) {
        if (!memcmp(sniffer_payload->source_mac, esp_module_mac[i], 3)) {
            return;
        }
    }
    /* Traversing the chain table to check the presence of the device */
    for (station_info = g_station_list->next; station_info; station_info = station_info->next) {
        if (!memcmp(station_info->bssid, sniffer_payload->source_mac, sizeof(station_info->bssid))) {
            return;
        }
    }
    /* Add the device information to chain table */
    if (!station_info) {
        station_info = malloc(sizeof(station_info_t));
        station_info->next = g_station_list->next;
        g_station_list->next = station_info;
    }

    station_info->rssi = sniffer->rx_ctrl.rssi;
    station_info->channel = sniffer->rx_ctrl.channel;
    station_info->timestamp = sniffer_timestamp();
    memcpy(station_info->bssid, sniffer_payload->source_mac, sizeof(station_info->bssid));
    s_device_info_num++;
    printf("\nCurrent device num = %d\n", s_device_info_num);
    printf("MAC: 0x%02X.0x%02X.0x%02X.0x%02X.0x%02X.0x%02X, The time is: %d, The rssi = %d\n", station_info->bssid[0], station_info->bssid[1], station_info->bssid[2], station_info->bssid[3], station_info->bssid[4], station_info->bssid[5], station_info->timestamp, station_info->rssi);
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
                if (!g_station_list) {
            g_station_list = malloc(sizeof(station_info_t));
            g_station_list->next = NULL;
            ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb));
            ESP_ERROR_CHECK(esp_wifi_set_promiscuous(1));
        }
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s] password:[%s]", CONFIG_WIFI_SSID, "******");
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t  mqtt_cfg = {
        .event_handle = mqtt_event_handler,
        .host            = ONENET_HOST,
        .port            = ONENET_PORT,
        .client_id       = ONENET_DEVICE_ID,
        .username        = ONENET_PROJECT_ID,
        .password        = ONENET_AUTH_INFO,
        .keepalive       = 120,
        .lwt_topic       = "/lwt",
        .lwt_msg         = "offline",
        .lwt_qos         = 0,
        .lwt_retain      = 0
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}

void app_main()
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    wifi_init();
    mqtt_app_start();
}