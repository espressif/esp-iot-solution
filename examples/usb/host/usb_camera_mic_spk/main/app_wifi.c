/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_wifi.h"

#include <stdio.h>
#include <string.h>
#include "captive_dns.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#define APP_WIFI_AP_SSID             CONFIG_ESP_WIFI_SOFTAP_SSID
#define APP_WIFI_AP_PASSWORD         CONFIG_ESP_WIFI_SOFTAP_PASSWORD
#define APP_WIFI_AP_CHANNEL          CONFIG_ESP_WIFI_AP_CHANNEL
#define APP_WIFI_AP_MAX_CONN         CONFIG_ESP_WIFI_SOFTAP_MAX_STA

static const char *TAG = "app_wifi";

esp_err_t app_wifi_start(void)
{
    esp_err_t ret = esp_netif_init();
    ESP_RETURN_ON_ERROR(ret, TAG, "esp_netif_init failed");

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Create event loop failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (!ap_netif) {
        ESP_LOGE(TAG, "Create default Wi-Fi AP netif failed");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");

    wifi_config_t wifi_config = {0};
    uint8_t ap_mac[6] = {0};
    ESP_RETURN_ON_ERROR(esp_wifi_get_mac(WIFI_IF_AP, ap_mac), TAG, "esp_wifi_get_mac failed");

    // Append the last three AP MAC bytes to keep each SoftAP SSID unique.
    char ap_ssid[sizeof(wifi_config.ap.ssid) + 1] = {0};
    int ssid_len = snprintf(ap_ssid, sizeof(ap_ssid), "%s-%02X%02X%02X", APP_WIFI_AP_SSID, ap_mac[3], ap_mac[4], ap_mac[5]);
    if (ssid_len < 0) {
        ESP_LOGE(TAG, "Format SoftAP SSID failed");
        return ESP_FAIL;
    }
    if ((size_t)ssid_len >= sizeof(ap_ssid)) {
        ESP_LOGW(TAG, "SoftAP SSID truncated to fit %u bytes", (unsigned)(sizeof(ap_ssid) - 1));
    }

    memcpy(wifi_config.ap.ssid, ap_ssid, strlen(ap_ssid));
    wifi_config.ap.ssid_len = strlen(ap_ssid);
    strlcpy((char *)wifi_config.ap.password, APP_WIFI_AP_PASSWORD, sizeof(wifi_config.ap.password));
    wifi_config.ap.channel = APP_WIFI_AP_CHANNEL;
    wifi_config.ap.max_connection = APP_WIFI_AP_MAX_CONN;
    wifi_config.ap.authmode = strlen(APP_WIFI_AP_PASSWORD) == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    wifi_config.ap.pmf_cfg = (wifi_pmf_config_t) {
        .required = false,
    };

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "esp_wifi_set_mode failed");
    // Disable Wi-Fi power save to reduce WebSocket audio latency.
    ret = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Set Wi-Fi power save NONE failed: %s", esp_err_to_name(ret));
    }
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wifi_config), TAG, "esp_wifi_set_config failed");

    // Configure DHCP DNS before esp_wifi_start(), because the default AP DHCP server starts with Wi-Fi.
    ret = captive_dns_start(&(captive_dns_config_t) {
        .ap_netif = ap_netif,
        .configure_dhcp_dns = true,
    });
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Captive DNS could not start, portal pop-up disabled: %s", esp_err_to_name(ret));
    }

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    ESP_LOGI(TAG, "SoftAP started, SSID:%s URL:http://192.168.4.1 ps=NONE", ap_ssid);
    (void)ap_netif;
    return ESP_OK;
}
