/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"

#include "esp_ble_conn_mgr.h"
#include "esp_esl.h"
#include "esp_eslp_tag.h"
#include "esl_nvs.h"
#include "app_esl_tag.h"
#include "app_tag_events.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"

static const char *TAG = "esl_tag_main";

#ifndef CONFIG_ESL_TAG_ADV_NAME
#define CONFIG_ESL_TAG_ADV_NAME "ESP_ESL_TAG"
#endif

static void on_conn_event(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)event_data;
    if (base != BLE_CONN_MGR_EVENTS || id != ESP_BLE_CONN_EVENT_STARTED) {
        return;
    }

    ESP_LOGI(TAG, "BLE host synced");
    uint8_t pub[6] = {0};
    int rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, pub, NULL);
    if (rc == 0) {
        ESP_LOGI(TAG, "Public identity " MACSTR, MAC2STR(pub));
    }
    esp_err_t ret = app_esl_tag_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "app_esl_tag_start failed: %s", esp_err_to_name(ret));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(esl_nvs_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_ESLP_EVENTS, ESP_EVENT_ANY_ID,
                                               app_tag_events_handle, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               on_conn_event, NULL));

    esp_ble_conn_config_t config = {
        .device_name = CONFIG_ESL_TAG_ADV_NAME,
        .include_service_uuid = 1,
        .adv_uuid_type = BLE_CONN_UUID_TYPE_16,
        .adv_uuid16 = BLE_ESL_UUID16,
    };

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));

    /* Inventory recovery keys off the advertised address. conn_mgr defaults to
     * a new NRPA on every advertise start, so a Tag reboot looks like a
     * different device. Use the public identity for this demo.
     *
     * adv_params_set() disables conn_mgr's connectable fallback (which only
     * applies when properties==0 and params were never set). Extended PDUs
     * cannot be both connectable and scannable; BIT0 = connectable AUX. */
    esp_ble_conn_adv_params_t adv_params = {0};
    if (esp_ble_conn_adv_params_get(&adv_params) == ESP_OK) {
        adv_params.own_addr_type = ESP_BLE_CONN_ADDR_PUBLIC;
        adv_params.adv_event_properties = (1u << 0);
        adv_params.primary_phy = ESP_BLE_CONN_PHY_1M;
        adv_params.secondary_phy = ESP_BLE_CONN_PHY_1M;
        ESP_ERROR_CHECK(esp_ble_conn_adv_params_set(&adv_params));
    }

    ESP_ERROR_CHECK(app_esl_tag_init());

    esp_err_t ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_conn_start failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "ESL Tag started. Advertising as %s", CONFIG_ESL_TAG_ADV_NAME);
}
