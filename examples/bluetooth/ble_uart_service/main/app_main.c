/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ble_uart.h"
#include "ble_protocol.h"
#include "emote_demo.h"

static const char *TAG = "app";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Build default device name from BT MAC. */
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_BT);
    char name[24];
    (void)snprintf(name, sizeof(name), "emote-%02X%02X", mac[4], mac[5]);

    /* Override with NVS-persisted name if available. */
    {
        nvs_handle_t h;
        if (nvs_open("ble_uart", NVS_READONLY, &h) == ESP_OK) {
            size_t len = sizeof(name);
            if (nvs_get_str(h, "name", name, &len) == ESP_OK && name[0] != '\0') {
                ESP_LOGI(TAG, "restored ble name '%s'", name);
            }
            nvs_close(h);
        }
    }

    ble_protocol_init();

    ble_uart_config_t ble_cfg = {
        .encrypted       = false,
        .device_name     = name,
        .ble_uart_on_rx  = ble_protocol_rx_feed,
    };
    ESP_ERROR_CHECK(ble_uart_install(&ble_cfg));
    ESP_ERROR_CHECK(ble_uart_open());

    ESP_LOGI(TAG, "emote_gen + BLE NUS");
    emote_demo_run();
}
