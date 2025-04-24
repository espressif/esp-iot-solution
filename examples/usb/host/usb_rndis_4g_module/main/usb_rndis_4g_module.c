/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_event.h"
#include "dhcpserver/dhcpserver_options.h"
#include "ping/ping_sock.h"
#include "usb_host_rndis.h"
#include "app_wifi.h"
#include "iot_eth.h"

static const char *TAG = "4g_module";

static void on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "%"PRIu32" bytes from %s icmp_seq=%u ttl=%u time=%"PRIu32" ms\n", recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGW(TAG, "From %s icmp_seq=%u timeout\n", ipaddr_ntoa(&target_addr), seqno);
    // Users can add logic to handle ping timeout
    // Add Wait or Reset logic
}

static esp_err_t _on_lowlevel_init_done(iot_eth_handle_t handle)
{
    ESP_LOGI(TAG, "USB RNDIS driver initialized");
    return ESP_OK;
}

static esp_err_t _on_lowlevel_deinit(iot_eth_handle_t handle)
{
    ESP_LOGI(TAG, "USB RNDIS driver deinitialized");
    return ESP_OK;
}

void app_main(void)
{
    usb_host_rndis_config_t rndis_cfg = {
        .auto_detect = true,
        .auto_detect_timeout = pdMS_TO_TICKS(1000),
        .rx_buffer_size = 1024 * 4,
        .tx_buffer_size = 1024 * 4,
    };

    iot_eth_driver_t *rndis_handle = NULL;
    esp_err_t ret = iot_eth_new_usb_rndis(&rndis_cfg, &rndis_handle);
    if (ret != ESP_OK || rndis_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create USB RNDIS driver");
        return;
    }

    iot_eth_config_t eth_cfg = {
        .driver = rndis_handle,
        .on_lowlevel_init_done = _on_lowlevel_init_done,
        .on_lowlevel_deinit = _on_lowlevel_deinit,
        .stack_input = NULL,
        .user_data = NULL,
    };

    iot_eth_handle_t eth_handle = NULL;
    ret = iot_eth_install(&eth_cfg, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install USB RNDIS driver");
        return;
    }

    while (1) {
        ret = iot_eth_start(eth_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to start USB RNDIS driver, try again...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        break;
    }

    app_wifi_main();

    ip_addr_t target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    char *ping_addr_s = NULL;
    ping_addr_s = "8.8.8.8";
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ipaddr_aton(ping_addr_s, &target_addr);
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = 2000;
    ping_config.task_stack_size = 4096;
    ping_config.count = 1;

    /* set callback functions */
    esp_ping_callbacks_t cbs = {
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL,
        .cb_args = NULL,
    };
    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &cbs, &ping);

    while (1) {
        esp_ping_start(ping);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
