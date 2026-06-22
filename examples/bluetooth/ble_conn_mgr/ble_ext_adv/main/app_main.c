/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"

#include "ext_adv.h"

static const char *TAG = "app_main";

static uint8_t s_ext_adv_buf[EXT_ADV_1K_TOTAL];

void app_main(void)
{
    ext_adv_1k_demo_fill(s_ext_adv_buf, sizeof(s_ext_adv_buf));

    uint32_t fnv = ext_adv_1k_demo_fnv1a(s_ext_adv_buf, EXT_ADV_1K_TOTAL);
    ESP_LOGI(TAG, "Extended ADV payload: %u bytes, primary 1M / secondary 2M, non-conn non-scan AUX, FNV-1a=0x%08" PRIx32,
             (unsigned)EXT_ADV_1K_TOTAL, fnv);

    esp_ble_conn_config_t config = {
        .device_name = EXT_ADV_DEMO_NAME,
        .broadcast_data = "NA",
        .extended_adv_data = (const char *)s_ext_adv_buf,
        .extended_adv_len = EXT_ADV_1K_TOTAL,
    };

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));

    esp_ble_conn_adv_params_t adv = {0};
    adv.adv_handle = 0;
    adv.own_addr_type = ESP_BLE_CONN_ADDR_RANDOM;
    adv.primary_phy = ESP_BLE_CONN_PHY_1M;
    adv.secondary_phy = ESP_BLE_CONN_PHY_2M;
    adv.sid = 2;
    adv.itvl_min = 0x140;
    adv.itvl_max = 0x140;
    /* NimBLE rejects connectable+scannable when legacy_pdu=0 (see ble_gap_ext_adv_params_validate). */
    adv.adv_event_properties = 0;
    adv.tx_power = 127;
    ESP_ERROR_CHECK(esp_ble_conn_adv_params_set(&adv));

    esp_err_t start_ret = esp_ble_conn_start();
    if (start_ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_conn_start failed: %s", esp_err_to_name(start_ret));
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
    }
}
