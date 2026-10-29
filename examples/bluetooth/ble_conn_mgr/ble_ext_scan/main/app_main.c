/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "host/ble_gap.h"

#include "esp_ble_conn_mgr.h"

#include "ext_adv.h"

static const char *TAG = "app_main";

static uint8_t s_expect[EXT_ADV_1K_TOTAL];
static uint8_t s_asm[EXT_ADV_1K_TOTAL];
static uint8_t s_addr[6];
static uint8_t s_addr_type;
static uint8_t s_sid;
static uint16_t s_off;
static bool s_have_key;

static void reasm_reset(void)
{
    s_have_key = false;
    s_off = 0;
}

static void reasm_begin(const esp_ble_conn_scan_result_t *r)
{
    memcpy(s_addr, r->addr, 6);
    s_addr_type = r->addr_type;
    s_sid = r->sid;
    s_off = 0;
    s_have_key = true;
}

static bool reasm_same(const esp_ble_conn_scan_result_t *r)
{
    return s_have_key && r->sid == s_sid && r->addr_type == s_addr_type && !memcmp(r->addr, s_addr, 6);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    if (id != ESP_BLE_CONN_EVENT_SCAN_RESULT) {
        return;
    }

    esp_ble_conn_scan_result_t *sr = (esp_ble_conn_scan_result_t *)event_data;

    if (sr->ext_data_status == ESP_BLE_CONN_SCAN_EXT_STATUS_LEGACY) {
        return;
    }

    if (sr->sid != 2) {
        return;
    }

    if (!reasm_same(sr)) {
        reasm_begin(sr);
    }

    if (s_off + sr->adv_data_len > sizeof(s_asm)) {
        ESP_LOGW(TAG, "Reassembly overflow, reset");
        reasm_reset();
        return;
    }

    memcpy(s_asm + s_off, sr->adv_data, sr->adv_data_len);
    s_off = (uint16_t)(s_off + sr->adv_data_len);

    ESP_LOGI(TAG, "EXT scan fragment: prim_phy=%u sec_phy=%u sid=%u status=%u len=%u off=%u rssi=%d",
             (unsigned)sr->prim_phy, (unsigned)sr->sec_phy, (unsigned)sr->sid,
             (unsigned)sr->ext_data_status, (unsigned)sr->adv_data_len, (unsigned)s_off, (int)sr->rssi);

    if (sr->ext_data_status == BLE_GAP_EXT_ADV_DATA_STATUS_TRUNCATED) {
        ESP_LOGW(TAG, "Truncated report from controller; payload may be incomplete");
        reasm_reset();
        return;
    }

    if (sr->ext_data_status != BLE_GAP_EXT_ADV_DATA_STATUS_COMPLETE) {
        return;
    }

    if (s_off != EXT_ADV_1K_TOTAL) {
        ESP_LOGW(TAG, "Complete status but assembled %u bytes (expected %u)", (unsigned)s_off, (unsigned)EXT_ADV_1K_TOTAL);
        reasm_reset();
        return;
    }

    if (memcmp(s_asm, s_expect, EXT_ADV_1K_TOTAL) != 0) {
        ESP_LOGE(TAG, "Payload mismatch vs expected pattern (first 32 bytes):");
        ESP_LOG_BUFFER_HEX(TAG, s_asm, 32 > s_off ? s_off : 32);
        reasm_reset();
        return;
    }

    uint32_t fnv = ext_adv_1k_demo_fnv1a(s_asm, EXT_ADV_1K_TOTAL);
    ESP_LOGI(TAG, "Target ESP_EXT_ADV_1K found: %u bytes on all PHYs, FNV-1a=0x%08" PRIx32 " (compare with advertiser log)",
             (unsigned)EXT_ADV_1K_TOTAL, fnv);
    ESP_LOGI(TAG, "Full payload (first 64 / last 64):");
    ESP_LOG_BUFFER_HEX(TAG, s_asm, 64);
    ESP_LOG_BUFFER_HEX(TAG, s_asm + EXT_ADV_1K_TOTAL - 64, 64);
    reasm_reset();
}

void app_main(void)
{
    ext_adv_1k_demo_fill(s_expect, sizeof(s_expect));
    uint32_t exp_fnv = ext_adv_1k_demo_fnv1a(s_expect, EXT_ADV_1K_TOTAL);
    ESP_LOGI(TAG, "Expect extended ADV %u bytes, FNV-1a=0x%08" PRIx32 " (must match advertiser)", (unsigned)EXT_ADV_1K_TOTAL, exp_fnv);

    esp_ble_conn_config_t config = {
        .device_name = "SCANNER",
        .broadcast_data = "NA",
    };

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_SCAN_RESULT,
                                               app_ble_conn_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));

    esp_ble_conn_scan_params_t sp = {0};
    sp.own_addr_type = ESP_BLE_CONN_ADDR_RANDOM;
    sp.passive = true;
    sp.filter_duplicates = false;
    sp.limited = false;
    sp.scan_phys = ESP_BLE_CONN_PHY_MASK_1M | ESP_BLE_CONN_PHY_MASK_CODED;
    sp.itvl = 0x40;
    sp.window = 0x40;
    ESP_ERROR_CHECK(esp_ble_conn_scan_params_set(&sp));

    ESP_ERROR_CHECK(esp_ble_conn_start());
}
