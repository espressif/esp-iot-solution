/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_idf_version.h"
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))
#include "esp_random.h"
#include "esp_mac.h"
#else
#include "esp_system.h"
#endif

#include "esp_ble_conn_mgr.h"
#include "esp_ots.h"

static const char *TAG = "app_main";

/* OTS Object Size characteristic value */
static esp_ble_ots_size_t obj_size = {
    .allocated_size = 256,
    .current_size = 0
};

static void app_ble_ots_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_ble_ots_prop_t *recv_obj_prop = NULL;

    if ((base != BLE_OTS_EVENTS) || (event_data == NULL)) {
        return;
    }

    switch (id) {
    case BLE_OTS_CHR_UUID16_OBJECT_NAME:
        ESP_LOGI(TAG, "Object Name write event");
        break;
    case BLE_OTS_CHR_UUID16_OBJECT_PROP:
        recv_obj_prop = (esp_ble_ots_prop_t *)event_data;
        ESP_LOGI(TAG, "Object prop = 0x%x", recv_obj_prop->delete_prop);
        break;
    default:
        break;
    }
}

static void app_ble_ots_init(void)
{
    ESP_ERROR_CHECK(esp_ble_ots_init());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_OTS_EVENTS, ESP_EVENT_ANY_ID, app_ble_ots_event_handler, NULL));
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        if (esp_ble_ots_set_size(&obj_size) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to set OTS size.");
        }
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED");
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

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    app_ble_ots_init();
    ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager: %s", esp_err_to_name(ret));
        esp_ble_conn_deinit();
        esp_ble_ots_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        esp_event_handler_unregister(BLE_OTS_EVENTS, ESP_EVENT_ANY_ID, app_ble_ots_event_handler);
        esp_event_loop_delete_default();
        ESP_LOGE(TAG, "Application initialization failed.");
    }
}
