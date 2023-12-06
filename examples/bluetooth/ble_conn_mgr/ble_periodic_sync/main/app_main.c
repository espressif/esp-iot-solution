/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"

static const char *TAG = "app_main";

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_PERIODIC_REPORT:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_PERIODIC_REPORT");
        esp_ble_conn_periodic_report_t *periodic_report = (esp_ble_conn_periodic_report_t *)event_data;
        ESP_LOGI(TAG, "sync_handle : %d", periodic_report->sync_handle);
        ESP_LOGI(TAG, "tx_power : %d", periodic_report->tx_power);
        ESP_LOGI(TAG, "rssi : %d", periodic_report->rssi);
        ESP_LOGI(TAG, "data_status : %d", periodic_report->data_status);
        ESP_LOGI(TAG, "data_length : %d", periodic_report->data_length);
        ESP_LOGI(TAG, "data : ");
        ESP_LOG_BUFFER_HEX(TAG, periodic_report->data, periodic_report->data_length);
        break;
    case ESP_BLE_CONN_EVENT_PERIODIC_SYNC_LOST:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_PERIODIC_SYNC_LOST");
        esp_ble_conn_periodic_sync_lost_t *periodic_sync_lost = (esp_ble_conn_periodic_sync_lost_t *)event_data;
        ESP_LOGI(TAG, "sync_handle : %d", periodic_sync_lost->sync_handle);
        ESP_LOGI(TAG, "reason : %s", periodic_sync_lost->reason == 13 ? "timeout" : (periodic_sync_lost->reason == 14 ? "terminated locally" : "Unknown reason"));
        break;
    case ESP_BLE_CONN_EVENT_PERIODIC_SYNC:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_PERIODIC_SYNC");
        esp_ble_conn_periodic_sync_t *periodic_sync = (esp_ble_conn_periodic_sync_t *)event_data;
        ESP_LOGI(TAG, "status : %d, periodic_sync_handle : %d, sid : %d", periodic_sync->status, periodic_sync->sync_handle, periodic_sync->sid);
        ESP_LOGI(TAG, "adv addr : %02x:%02x:%02x:%02x:%02x:%02x", periodic_sync->adv_addr[0], periodic_sync->adv_addr[1], periodic_sync->adv_addr[2],
                 periodic_sync->adv_addr[3], periodic_sync->adv_addr[4], periodic_sync->adv_addr[5]);
        ESP_LOGI(TAG, "adv_phy : %s", periodic_sync->adv_phy == 1 ? "1m" : (periodic_sync->adv_phy == 2 ? "2m" : "coded"));
        ESP_LOGI(TAG, "per_adv_ival : %d", periodic_sync->per_adv_ival);
        ESP_LOGI(TAG, "adv_clk_accuracy : %d", periodic_sync->adv_clk_accuracy);
        break;
    default:
        break;
    }
}

void
app_main(void)
{
    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV
    };

    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_event_loop_create_default();
    esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL);

    esp_ble_conn_init(&config);
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
    }
}
