/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"

#include "patterns.h"

#ifdef CONFIG_EXAMPLE_EXTENDED_ADV
static uint8_t ext_adv_pattern[] = {
    0x02, 0x01, 0x06,
    0x03, 0x03, 0x11, 0x18,
    0x03, 0x03, 0x12, 0x18,
    0x03, 0x03, 0x13, 0x18,
    0x03, 0x03, 0x14, 0x18,
    0x03, 0x03, 0x15, 0x18,
    0x15, 0X09, 'E', 'S', 'P', '_', 'P', 'E', 'R', 'I', 'O', 'D', 'I', 'C', '_', 'A', 'D', 'V', '_', 'E', 'X', 'T',
};
#endif

void
app_main(void)
{
    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV,
#ifdef CONFIG_EXAMPLE_EXTENDED_ADV
        .extended_adv_data = (const char *)ext_adv_pattern,
        .extended_adv_len = sizeof(ext_adv_pattern),
#endif
        .periodic_adv_data = (const char *)ext_adv_pattern_1,
        .periodic_adv_len = sizeof(ext_adv_pattern_1),
    };

    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_ble_conn_init(&config);
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
    }
}
