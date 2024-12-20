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
#include "esp_bcs.h"

static const char *TAG = "app_main";

static void app_ble_bcs_init(void)
{
    esp_ble_bcs_init();
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_bcs_val_t bcs_val = {
        .bcs_flag = BLE_BCS_FLAG_TIME_STAMP |
        BLE_BCS_FLAG_MULTI_USER |
        BLE_BCS_FLAG_BASAL_METABOLISM |
        BLE_BCS_FLAG_MUSCLE_PERCENTAGE |
        BLE_BCS_FLAG_MUSCLE_MASS |
        BLE_BCS_FLAG_FAT_FREE_MASS |
        BLE_BCS_FLAG_SOFT_LEAN_MASS |
        BLE_BCS_FLAG_BODY_WATER_MASS |
        BLE_BCS_FLAG_IMPEDENCE |
        BLE_BCS_FLAG_WEIGHT |
        BLE_BCS_FLAG_HEIGHT,
        .timestamp.year = 2024,
        .timestamp.month = 11,
        .timestamp.day = 5,
        .timestamp.hours = 15,
        .timestamp.minutes = 15,
        .timestamp.seconds = 6,
        .user_id = 1,
        .basal_metabolism = 0x01,
        .muscle_percentage = 0x02,
        .muscle_mass = 0x03,
        .fat_free_mass = 0x04,
        .soft_lean_mass = 0x05,
        .body_water_mass = 0x06,
        .impedance = 0x01,
        .weight = 0x42,
        .height = 0x33
    };

    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        esp_ble_bcs_set_measurement(&bcs_val, true);
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
    app_ble_bcs_init();
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
    }
}
