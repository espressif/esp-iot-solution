/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp_log.h"
#include "app_espnow.h"

#define SERVO_NUM 7
#define FRAME_HEADER_SIZE 2
#define PACKET_SIZE (FRAME_HEADER_SIZE + SERVO_NUM * 2 + 2)  // 2 bytes frame header + 6 servos * 2 bytes + 2 bytes CRC

static const char *TAG = "ESPNOW";
QueueHandle_t s_espnow_queue = NULL;

void app_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    uint8_t * mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    espnow_recv_data_t recv_data;
    recv_data.data = (uint8_t *)malloc(len);
    memcpy(recv_data.data, data, len);
    recv_data.len = len;

    if (xQueueSend(s_espnow_queue, &recv_data, portMAX_DELAY) != pdPASS) {
        ESP_LOGW(TAG, "Failed to send data to queue");
        free(recv_data.data);
    }
}

void app_espnow_recv_task(void *pvParameter)
{
    espnow_recv_data_t recv_data;
    while (1) {
        if (xQueueReceive(s_espnow_queue, &recv_data, portMAX_DELAY) == pdPASS) {

            if (recv_data.len == PACKET_SIZE && recv_data.data[0] == 0xFF && recv_data.data[1] == 0xFF) {
                uint16_t received_crc = ((uint16_t)recv_data.data[PACKET_SIZE - 1] << 8) |
                                        recv_data.data[PACKET_SIZE - 2];
                uint16_t calculated_crc = esp_crc16_le(UINT16_MAX, recv_data.data, PACKET_SIZE - 2);
                if (received_crc == calculated_crc) {
                    fwrite(recv_data.data, 1, recv_data.len, stdout);
                    fflush(stdout);
                }
            }

            free(recv_data.data);
        }
    }
}

void app_espnow_init()
{
    s_espnow_queue = xQueueCreate(10, sizeof(espnow_recv_data_t));
    if (s_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // Initialize WiFi and ESPNOW
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(app_espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    xTaskCreate(app_espnow_recv_task, "espnow_recv_task", 4 * 1024, NULL, 5, NULL);
}
