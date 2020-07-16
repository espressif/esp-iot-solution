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
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "iot_espnow.h"
#include "esp_log.h"
#include "unity.h"

static const char* TAG_TX = "espnow_sender";
static const char* TAG_RX = "espnow_sender";
#define PEER_ADDR_RX  "aaaabbbbcccc"
#define PEER_ADDR_TX  "aaaabbccddee"
#define ESPNOW_CHANNEL  1
#define ENCRYPT  1

#define SEND_MAX_DATA_LEN    (256)
#define RECV_MAX_DATA_LEN    (256)
#define WIFI_CONNECTED_BIT   BIT0
static EventGroupHandle_t g_wifi_event_group = NULL;

static void str2mac(uint8_t *mac, const char *mac_str)
{
    uint32_t mac_data[6] = { 0 };
    sscanf(mac_str, "%02x%02x%02x%02x%02x%02x", mac_data, mac_data + 1, mac_data + 2, mac_data + 3, mac_data + 4,
            mac_data + 5);

    for (int i = 0; i < 6; ++i) {
        mac[i] = mac_data[i];
    }
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    system_event_sta_disconnected_t *disconnected = NULL;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            printf("SYSTEM_EVENT_STA_START\n");
            break;

        case SYSTEM_EVENT_STA_CONNECTED:
            printf("SYSTEM_EVENT_STA_CONNECTED\n");
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            printf("SYSTEM_EVENT_STA_GOT_IP\n");
            xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            disconnected = &event->event_info.disconnected;
            printf("SYSTEM_EVENT_STA_DISCONNECTED, reason: %d, free_heap: %d\n", disconnected->reason,
                    esp_get_free_heap_size());

            /**< This is a workaround as ESP32 WiFi libs don't currently
             auto-reassociate. */
            esp_wifi_connect();
            xEventGroupClearBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
            break;

        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t espnow_recv_wifi_init(void)
{
    if (!g_wifi_event_group) {
        g_wifi_event_group = xEventGroupCreate();
    }

    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));/**< in case device in powe save mode */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    uint8_t mac_addr[ESP_NOW_ETH_ALEN] = { 0 };
    str2mac(mac_addr, PEER_ADDR_RX);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_mac(ESP_IF_WIFI_AP, mac_addr));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_channel(ESPNOW_CHANNEL, 0);
    return ESP_OK;
}

static esp_err_t espnow_send_wifi_init(void)
{
    if (!g_wifi_event_group) {
        g_wifi_event_group = xEventGroupCreate();
    }

    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));/**< in case device in powe save mode */
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_stop());
    uint8_t mac_addr[ESP_NOW_ETH_ALEN] = { 0 };
    str2mac(mac_addr, PEER_ADDR_TX);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_mac(ESP_IF_WIFI_STA, mac_addr));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_channel(ESPNOW_CHANNEL, 0);
    return ESP_OK;
}

static void espnow_send_handle_task(void *arg)
{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    str2mac(mac_addr, PEER_ADDR_RX);
    uint8_t *send_data = calloc(1, SEND_MAX_DATA_LEN);
    for (int i = 0; i < 32; i++) {
        send_data[i] = i;
    }
    for (;;) {
        ESP_LOGI(TAG_TX, "espnow send to %02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(mac_addr));
        iot_espnow_send(mac_addr, send_data, 32, 1000 / portTICK_RATE_MS);
        vTaskDelay(3000 / portTICK_RATE_MS);
    }
    free(send_data);
    vTaskDelete(NULL);
}

static void espnow_recv_handle_task(void *arg)
{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN] = { 0 };
    uint8_t *recv_data = calloc(1, RECV_MAX_DATA_LEN);

    for (;;) {
        memset(recv_data, 0, RECV_MAX_DATA_LEN);
        int len = iot_espnow_recv(mac_addr, recv_data, portMAX_DELAY);
        if (len <= 0) {
            continue;
        } else {
            ESP_LOGI(TAG_RX, "Recv from: %02X:%02X:%02X:%02X:%02X:%02X:", MAC2STR(mac_addr));
            printf("espnow received %d bytes: ", len);
            for (int i = 0; i < len; i++) {
                printf("%02x ", recv_data[i]);
            }
            printf("\n");
            if (recv_data[0] == 0) {
                const char* resp = "got ya!";
                iot_espnow_send(mac_addr, resp, strlen(resp), 200 / portTICK_RATE_MS);
                ESP_LOGI(TAG_RX, "Send response...");
            }
        }

    }
    free(recv_data);
    vTaskDelete(NULL);
}

static esp_err_t espnow_recv_task_init(void)
{
    iot_espnow_init();
    uint8_t mac_addr[ESP_NOW_ETH_ALEN] = { 0 };
    str2mac(mac_addr, PEER_ADDR_TX);
#if ENCRYPT
    iot_espnow_add_peer(mac_addr, (const uint8_t*)CONFIG_ESPNOW_LMK, ESP_IF_WIFI_AP, ESPNOW_CHANNEL);
#else
    iot_espnow_add_peer_no_encrypt(mac_addr, ESP_IF_WIFI_AP, -1);
#endif
    xTaskCreate(espnow_recv_handle_task, "espnow_recv_handle_task", 1024 * 3, (void*) 1, 6, NULL);
    return ESP_OK;
}

static esp_err_t espnow_send_task_init(void)
{
    iot_espnow_init();
    uint8_t dest_addr[ESP_NOW_ETH_ALEN];
    str2mac(dest_addr, PEER_ADDR_RX);
#if ENCRYPT
    iot_espnow_add_peer(dest_addr, (const uint8_t*)CONFIG_ESPNOW_LMK, ESP_IF_WIFI_STA, ESPNOW_CHANNEL);
#else
    iot_espnow_add_peer_no_encrypt(dest_addr, ESP_IF_WIFI_STA, -1);
#endif
    xTaskCreate(espnow_send_handle_task, "espnow_send_handle_task", 1024 * 3, NULL, 6, NULL);
    xTaskCreate(espnow_recv_handle_task, "espnow_recv_handle_task", 1024 * 3, (void*) 0, 6, NULL);

    return ESP_OK;
}

void espnow_recv_test()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(espnow_recv_wifi_init());
    ESP_ERROR_CHECK(espnow_recv_task_init());
}

void espnow_send_test()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(espnow_send_wifi_init());
    ESP_ERROR_CHECK(espnow_send_task_init());
}

TEST_CASE("ESPNOW recv test", "[espnow][iot]")
{
    espnow_recv_test();
}

TEST_CASE("ESPNOW send test", "[espnow][iot]")
{
    espnow_send_test();
}
