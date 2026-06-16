/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * ESP-BLE-UART Vibe Indicator — multi-channel R/Y/G lamps controlled
 * over JSONL (Signal Light Control Protocol v1).
 */

#include <stdio.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "ble_jsonl.h"
#include "ble_uart.h"
#include "indicator.h"

static const char *TAG = "app";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(indicator_init());
    ESP_ERROR_CHECK(ble_jsonl_init());

    uint8_t mac[6] = {0};
    esp_err_t mac_err = esp_read_mac(mac, ESP_MAC_BT);
    if (mac_err != ESP_OK) {
        ESP_LOGW(TAG, "esp_read_mac(BT) failed (%s); device name suffix will be 0000",
                 esp_err_to_name(mac_err));
    }
    char name[32];
    snprintf(name, sizeof(name), "%s-%02X%02X",
             CONFIG_VIBE_INDICATOR_DEVICE_NAME_PREFIX, mac[4], mac[5]);

    ESP_ERROR_CHECK(ble_uart_install(&(ble_uart_config_t) {
        .encrypted      = false,
        .device_name    = name,
        .ble_uart_on_rx = ble_jsonl_rx_feed,
    }));

    ESP_ERROR_CHECK(ble_uart_open());
    ESP_LOGI(TAG, "Vibe Indicator ready as '%s'", name);
}
