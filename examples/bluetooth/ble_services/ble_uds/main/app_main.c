/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
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
#include "esp_uds.h"

static const char *TAG = "app_main";

/* UDS database change increment characteristic value */
esp_ble_uds_db_t db_chg_val = {
    .len = 2,
    .db_buf = {0x12, 0x34}
};

static void app_ble_uds_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_ble_uds_db_t * db_val = NULL;
    esp_ble_uds_user_ctrl_t * ctrl_val = NULL;

    if ((base != BLE_UDS_EVENTS) || (event_data == NULL)) {
        return;
    }

    switch (id) {
    case BLE_UDS_CHR_UUID16_DATABASH_CHG:
        db_val = (esp_ble_uds_db_t *)event_data;
        ESP_LOGI(TAG, "Database Change characteristic, write len = %d", db_val->len);
        break;
    case BLE_UDS_CHR_UUID16_USER_CTRL:
        ctrl_val = (esp_ble_uds_user_ctrl_t *)event_data;
        ESP_LOGI(TAG, "User control point, opcode = 0x%x", ctrl_val->op_code);
        break;
    default:
        break;
    }
}

static void app_ble_uds_init(void)
{
    esp_ble_uds_init();
    esp_event_handler_register(BLE_UDS_EVENTS, ESP_EVENT_ANY_ID, app_ble_uds_event_handler, NULL);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        esp_ble_uds_set_db_incre(&db_chg_val, true);
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

    esp_event_loop_create_default();
    esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL);

    esp_ble_conn_init(&config);
    app_ble_uds_init();
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        esp_event_handler_unregister(BLE_UDS_EVENTS, ESP_EVENT_ANY_ID, app_ble_uds_event_handler);
    }
}
