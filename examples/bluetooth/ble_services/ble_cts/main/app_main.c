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
#include "esp_cts.h"

static const char *TAG = "app_main";

static void app_ble_cts_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_ble_cts_cur_time_t *cur_time = NULL;
    esp_ble_cts_local_time_t *local_time = NULL;
    esp_ble_cts_ref_time_t *ref_time = NULL;

    if ((base != BLE_CTS_EVENTS) || (event_data == NULL)) {
        return;
    }

    switch (id) {
#ifdef CONFIG_BLE_CTS_CURRENT_TIME_WRITE_ENABLE
    case BLE_CTS_CHR_UUID16_CURRENT_TIME:
        cur_time = (esp_ble_cts_cur_time_t *)event_data;
        ESP_LOGI(TAG, "Current time, year = %d, month = %d", cur_time->timestamp.year, cur_time->timestamp.month);
        break;
#endif
#if defined(CONFIG_BLE_CTS_LOCAL_TIME_CHAR_ENABLE) && defined(CONFIG_BLE_CTS_LOCAL_TIME_WRITE_ENABLE)
    case BLE_CTS_CHR_UUID16_LOCAL_TIME:
        local_time = (esp_ble_cts_local_time_t *)event_data;
        ESP_LOGI(TAG, "Local time, timezone = %d", local_time->timezone);
        break;
#endif
#ifdef CONFIG_BLE_CTS_REF_TIME_CHAR_ENABLE
    case BLE_CTS_CHR_UUID16_REFERENCE_TIME:
        ref_time = (esp_ble_cts_ref_time_t *)event_data;
        ESP_LOGI(TAG, "Reference time, time accuracy = %d", ref_time->time_accuracy);
        break;
#endif
    default:
        break;
    }
}

static void app_ble_cts_init(void)
{
    esp_ble_cts_init();
    esp_event_handler_register(BLE_CTS_EVENTS, ESP_EVENT_ANY_ID, app_ble_cts_event_handler, NULL);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_ble_cts_cur_time_t cur_time = {
        .timestamp.year = 2024,
        .timestamp.month = 10,
        .timestamp.day = 1,
        .timestamp.hours = 9,
        .timestamp.minutes = 10,
        .timestamp.seconds = 25,
        .day_of_week = 1,
        .fractions_256 = 6,
        .adjust_reason = BLE_CTS_MANUAL_TIME_UPDATE_MASK
    };

    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        esp_ble_cts_set_current_time(&cur_time, true);
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
    app_ble_cts_init();
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        esp_event_handler_unregister(BLE_CTS_EVENTS, ESP_EVENT_ANY_ID, app_ble_cts_event_handler);
    }
}
