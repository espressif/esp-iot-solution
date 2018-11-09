// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "iot_espnow.h"

static const char* TAG = "iot_espnow";

#define SEND_CB_OK               BIT0
#define SEND_CB_FAIL             BIT1

#define ESPNOW_ERCV_QUEUE_NUM    10

#define IOT_ESPNOW_PMK_DEFAULT CONFIG_IOT_ESPNOW_PMK
#define IOT_ESPNOW_LMK_DEFAULT "lmk1234567890123"

static bool g_iot_espnow_inited = false;
static EventGroupHandle_t g_event_group = NULL;
static xQueueHandle s_espnow_queue = NULL;
static const uint8_t g_bcast_mac[ESP_NOW_ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

typedef struct {
    uint8_t addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} espnow_data_t;

static void iot_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS) {
        xEventGroupSetBits(g_event_group, SEND_CB_OK);
    } else {
        xEventGroupSetBits(g_event_group, SEND_CB_FAIL);
    }
}

static void iot_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    /**< if s_espnow_queue is full, delete the front item */
    if (!uxQueueSpacesAvailable(s_espnow_queue)) {
        espnow_data_t prev_data;
        if(xQueueReceive(s_espnow_queue, (void *)&prev_data, 0) == pdTRUE) {
            free(prev_data.data);
        }
        ESP_LOGW(TAG, "espnow recv queue is full...");
    }

    espnow_data_t recv_data;
    recv_data.data = (uint8_t *)calloc(1, len);
    memcpy(recv_data.data, data, len);
    memcpy(recv_data.addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_data.data_len = len;
    if(xQueueSend(s_espnow_queue, (void *)&recv_data, 0) != pdTRUE) {
        free(recv_data.data);
    }
}

esp_err_t iot_espnow_init(void)
{
    if (g_iot_espnow_inited) {
        return ESP_OK;
    }

    /**< event group for espnow sent cb */
    g_event_group = xEventGroupCreate();
    s_espnow_queue = xQueueCreate(ESPNOW_ERCV_QUEUE_NUM, sizeof(espnow_data_t));

    /**< init espnow, register cb and set pmk*/
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(iot_espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(iot_espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)IOT_ESPNOW_PMK_DEFAULT));

    g_iot_espnow_inited = true;
    return ESP_OK;
}

esp_err_t iot_espnow_deinit(void)
{
    if (g_iot_espnow_inited == false) {
        ESP_LOGE(TAG, "espnow not inited");
        return ESP_FAIL;
    }
    
    ESP_ERROR_CHECK(esp_now_unregister_recv_cb());
    ESP_ERROR_CHECK(esp_now_unregister_send_cb());
    ESP_ERROR_CHECK(esp_now_deinit());
    vEventGroupDelete(g_event_group);
    g_event_group = NULL;

    g_iot_espnow_inited = false;
    return ESP_OK;
}

esp_err_t iot_espnow_add_peer_base(const uint8_t dest_addr[ESP_NOW_ETH_ALEN],
                                   bool default_encrypt, const uint8_t lmk[ESP_NOW_KEY_LEN],
                                   wifi_interface_t interface, int8_t channel)
{
    static SemaphoreHandle_t s_addpeer_mutex = NULL;

    if (!s_addpeer_mutex) {
        s_addpeer_mutex = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(s_addpeer_mutex, portMAX_DELAY);

    if (esp_now_is_peer_exist(dest_addr)) {
        ESP_ERROR_CHECK(esp_now_del_peer(dest_addr));
    }

    esp_err_t ret = ESP_OK;
    uint8_t pri_channel = 0;
    if (channel < 0) {
        wifi_second_chan_t sec_channel = 0;
        ESP_ERROR_CHECK(esp_wifi_get_channel(&pri_channel, &sec_channel));
        ESP_LOGI(TAG, "espnow add peer, channel: %d", pri_channel);
    } else {
        pri_channel = channel;
    }

    esp_now_peer_info_t *peer = calloc(1, sizeof(esp_now_peer_info_t));
    peer->ifidx               = interface;
    peer->channel             = pri_channel;

    if (!memcmp(dest_addr, g_bcast_mac, ESP_NOW_ETH_ALEN)) {
        if (default_encrypt) {
            ESP_LOGW(TAG, "broadcast addr cannot enable encryption");
        }
        /**< broadcast addr cannot encrypt */
        peer->encrypt = false;
    } else {
        if (default_encrypt) {
            ESP_LOGI(TAG, "Set default key...%s", IOT_ESPNOW_LMK_DEFAULT);
            peer->encrypt = true;
            memcpy(peer->lmk, IOT_ESPNOW_LMK_DEFAULT, ESP_NOW_KEY_LEN);
        } else if (lmk) {
            peer->encrypt = true;
            memcpy(peer->lmk, lmk, ESP_NOW_KEY_LEN);
        } else {
            peer->encrypt = false;
        }
    }

    memcpy(peer->peer_addr, dest_addr, ESP_NOW_ETH_ALEN);
    ret = esp_now_add_peer(peer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_now_add_peer fail, ret: %d", ret);
    }
    
    free(peer);
    xSemaphoreGive(s_addpeer_mutex);
    return ret;
}

esp_err_t iot_espnow_del_peer(const uint8_t dest_addr[ESP_NOW_ETH_ALEN])
{
    esp_err_t ret = ESP_OK;

    if (esp_now_is_peer_exist(dest_addr)) {
        ret = esp_now_del_peer(dest_addr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_del_peer fail, ret: %d", ret);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

size_t iot_espnow_recv(uint8_t src_addr[ESP_NOW_ETH_ALEN],
                       void *data, TickType_t block_ticks)
{
    if (!s_espnow_queue) {
        return ESP_FAIL;
    }

    espnow_data_t espnow_recv_data;
    if (xQueueReceive(s_espnow_queue, (void *)&espnow_recv_data, block_ticks) != pdTRUE) {
        ESP_LOGE(TAG, "espnow read fail");
        return ESP_FAIL;
    }
    memcpy(src_addr, espnow_recv_data.addr, ESP_NOW_ETH_ALEN);
    memcpy(data, espnow_recv_data.data, espnow_recv_data.data_len);    
    free(espnow_recv_data.data);
    return espnow_recv_data.data_len;
}

size_t iot_espnow_send(const uint8_t dest_addr[ESP_NOW_ETH_ALEN],
                        const void *data, size_t data_len, TickType_t block_ticks)
{
    size_t ret;
    SemaphoreHandle_t s_send_mutex = NULL;

    if (!s_send_mutex) {
        s_send_mutex = xSemaphoreCreateMutex();
    }

    if (xSemaphoreTake(s_send_mutex, block_ticks) != pdTRUE) {
        return ESP_FAIL;
    }

    xEventGroupClearBits(g_event_group, SEND_CB_OK | SEND_CB_FAIL);
    /**< espnow send and wait cb */
    ret = esp_now_send(dest_addr, (uint8_t *)data, data_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "espnow send fail: %d", ret);
        goto EXIT;
    }

    EventBits_t uxBits = xEventGroupWaitBits(g_event_group, SEND_CB_OK | SEND_CB_FAIL,
                                             pdTRUE, pdFALSE, portMAX_DELAY);
    if ((uxBits & SEND_CB_OK) != SEND_CB_OK) {
        ret = ESP_FAIL;
        ESP_LOGE(TAG, "g_event_group wait SEND_CB_OK fail");
        goto EXIT;
    }

    ret = data_len;

EXIT:
    xSemaphoreGive(s_send_mutex);
    return ret;
}

