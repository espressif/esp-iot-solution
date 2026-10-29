/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CMD_WIFI_H
#define __CMD_WIFI_H

#ifdef __cplusplus
extern "C" {
#endif

void initialise_wifi(void);

uint32_t wifi_get_local_ip(void);

esp_err_t wifi_cmd_sta_join(const char* ssid, const char* pass);

esp_err_t wifi_cmd_ap_set(const char* ssid, const char* pass);

esp_err_t wifi_cmd_sta_scan(const char* ssid);

esp_err_t wifi_cmd_query(void);

esp_err_t wifi_cmd_set_mode(char* mode);

esp_err_t wifi_cmd_start_smart_config(void);

esp_err_t wifi_cmd_stop_smart_config(void);

esp_err_t wif_cmd_disconnect_wifi(void);

void wifi_buffer_free(void *buffer, void *ctx);

esp_err_t wifi_recv_callback(void *buffer, uint16_t len, void *ctx);

extern uint8_t tud_network_mac_address[6];

#ifdef __cplusplus
}
#endif

#endif // __CMD_WIFI_H
