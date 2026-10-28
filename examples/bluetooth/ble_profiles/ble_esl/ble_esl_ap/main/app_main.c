/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esl_nvs.h"
#include "app_esl_ap.h"
#include "app_inventory.h"
#include "app_recovery.h"

#if CONFIG_ESL_AP_MODE_WITH_APP
#include "app_gateway.h"
#endif

static const char *TAG = "esl_ap_main";

static void on_conn_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)event_data;
    if (base != BLE_CONN_MGR_EVENTS || id != ESP_BLE_CONN_EVENT_STARTED) {
        return;
    }

    ESP_LOGI(TAG, "BLE host synced, starting ESL AP");
    app_inventory_dump("host_sync");

#if CONFIG_ESL_AP_MODE_WITH_APP
    esp_err_t gw_init = app_gateway_init();
    if (gw_init != ESP_OK) {
        ESP_LOGE(TAG, "app_gateway_init failed: %s", esp_err_to_name(gw_init));
    }
#endif

    esp_err_t esl_ret = app_esl_ap_start();
    if (esl_ret != ESP_OK) {
        ESP_LOGE(TAG, "app_esl_ap_start failed: %s (Gateway still starts)",
                 esp_err_to_name(esl_ret));
    }

#if CONFIG_ESL_AP_MODE_WITH_APP
    /* NUS ext-adv on instance 1 during AUX_CONNECT races the Tag handshake:
     * Tag enters UPDATING, AP never gets GAP CONNECT, recovery hits ACL timeout. */
    if (app_recovery_blocks_gateway()) {
        ESP_LOGI(TAG, "Defer NUS advertising until recovery radio protection ends");
    } else {
        ESP_LOGI(TAG, "Starting NUS Gateway for phone observability");
        esp_err_t gw_ret = app_gateway_start();
        if (gw_ret != ESP_OK) {
            ESP_LOGE(TAG, "app_gateway_start failed: %s", esp_err_to_name(gw_ret));
        }
    }
#endif
}

void app_main(void)
{
    ESP_ERROR_CHECK(esl_nvs_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               on_conn_event, NULL));

    /* Do not advertise ESL UUID (that would look like a Tag). PAwR uses
     * conn_mgr instance 0; NUS advertises on NimBLE instance 1. */
    esp_ble_conn_config_t config = {
        .device_name = "ESP-ESL-AP",
        .include_service_uuid = 0,
    };

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(app_esl_ap_init());

    esp_err_t ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_conn_start failed: %s", esp_err_to_name(ret));
        return;
    }
}
