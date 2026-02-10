/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "esp_log.h"
#include "leader_config.h"
#include "app_espnow.h"

#define FRAME_HEADER_SIZE 2
#define PACKET_SIZE (FRAME_HEADER_SIZE + MAX_SERVO_NUM * 2 + 2)                        /*!< 2 bytes frame header + 7 servos * 2 bytes + 2 bytes CRC */

static const char *TAG = "ESP_NOW";
static uint8_t slave_mac_addr[ESP_NOW_ETH_ALEN] = {0};

static bool parse_mac_address(const char *mac_str, uint8_t *mac_bytes)
{
    if (mac_str == NULL || mac_bytes == NULL) {
        return false;
    }

    int len = strlen(mac_str);
    int byte_count = 0;

    // Check if format is "XX:XX:XX:XX:XX:XX" (17 chars) or "XXXXXXXXXXXX" (12 chars)
    if (len == 17) {
        // Format with colons: "XX:XX:XX:XX:XX:XX"
        for (int i = 0; i < len && byte_count < ESP_NOW_ETH_ALEN; i += 3) {
            if (i + 1 >= len) {
                break;
            }
            char hex_str[3] = {mac_str[i], mac_str[i + 1], '\0'};
            mac_bytes[byte_count++] = (uint8_t)strtoul(hex_str, NULL, 16);
            if (i + 2 < len && mac_str[i + 2] != ':') {
                return false;                                                          /*!< Invalid format */
            }
        }
    } else if (len == 12) {
        // Format without colons: "XXXXXXXXXXXX"
        for (int i = 0; i < len && byte_count < ESP_NOW_ETH_ALEN; i += 2) {
            if (i + 1 >= len) {
                break;
            }
            char hex_str[3] = {mac_str[i], mac_str[i + 1], '\0'};
            mac_bytes[byte_count++] = (uint8_t)strtoul(hex_str, NULL, 16);
        }
    } else {
        return false; // Invalid length
    }

    return (byte_count == ESP_NOW_ETH_ALEN);
}

void app_espnow_init(void)
{
    uint8_t mac_bytes[ESP_NOW_ETH_ALEN];
    if (!parse_mac_address(CONFIG_ESPNOW_SLAVE_MAC, mac_bytes)) {
        ESP_LOGE(TAG, "Failed to parse MAC address from config: %s", CONFIG_ESPNOW_SLAVE_MAC);
        return;
    }

    memcpy(slave_mac_addr, mac_bytes, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    // Add slave mac address to espnow
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for peer info");
        esp_now_deinit();
        return;
    }

    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = (wifi_interface_t)ESP_IF_WIFI_STA;
    peer->encrypt = false;
    memcpy(peer->peer_addr, mac_bytes, ESP_NOW_ETH_ALEN);
    esp_now_add_peer(peer);
    free(peer);
}

static uint16_t calculate_crc(const uint8_t *data, size_t length)
{
    return esp_crc16_le(UINT16_MAX, data, length);
}

esp_err_t app_espnow_send_servo_data(const float *servo_angles, size_t angles_count)
{
    if (angles_count != MAX_SERVO_NUM) {
        ESP_LOGE(TAG, "Invalid servo angles count: %d, expected: %d", angles_count, MAX_SERVO_NUM);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t packet[PACKET_SIZE];

    // Add frame header
    packet[0] = 0xFF;
    packet[1] = 0xFF;

    // Pack servo data: convert degrees to radians, then to int16_t (multiply by 100 for 0.01 radian precision)
    // Input range: -180.00 to 180.00 degrees -> -π to π radians -> -314 to 314 (approximately)
    for (int i = 0; i < MAX_SERVO_NUM; i++) {
        // Clamp angle to valid range (-180 to 180 degrees)
        float angle_deg = servo_angles[i];
        if (angle_deg > 180.0f) {
            angle_deg = 180.0f;
        } else if (angle_deg < -180.0f) {
            angle_deg = -180.0f;
        }

        float angle_rad = angle_deg * M_PI / 180.0f;                                   /*!< Convert degrees to radians */

        int16_t angle_int = (int16_t)(angle_rad * 100.0f);                             /*!< Convert radians to int16_t (multiply by 100 for 0.01 radian precision) */

        // Pack as little-endian int16_t
        packet[FRAME_HEADER_SIZE + i * 2] = (uint8_t)(angle_int & 0xFF);               /*!< Low byte of int16_t */
        packet[FRAME_HEADER_SIZE + i * 2 + 1] = (uint8_t)((angle_int >> 8) & 0xFF);    /*!< High byte of int16_t */
    }

    // Calculate CRC (excluding CRC field itself)
    uint16_t crc = calculate_crc(packet, PACKET_SIZE - 2);
    packet[PACKET_SIZE - 2] = (uint8_t)(crc & 0xFF);                                   /*!< CRC low byte */
    packet[PACKET_SIZE - 1] = (uint8_t)((crc >> 8) & 0xFF);                            /*!< CRC high byte */

    return esp_now_send(slave_mac_addr, packet, PACKET_SIZE);
}

esp_err_t app_espnow_send(const uint8_t *data, size_t length)
{
    return esp_now_send(slave_mac_addr, data, length);
}
