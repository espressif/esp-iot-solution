/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_hid_gap.h"
#include "esp_hidd.h"
#include "ble_hid.h"

#define TAG "BLE-HID"

extern uint8_t const desc_hid_report[];
extern const uint16_t desc_hid_report_len;

enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_FULL_KEY_KEYBOARD, // 2
    REPORT_ID_CONSUMER, // 3
    REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES, // 4
    REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST, // 5
    REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE, // 6
    REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE, // 7
    REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE, // 8
    REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL, // 9
    REPORT_ID_COUNT
};

typedef struct {
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    bool is_connected;
} local_param_t;

static local_param_t s_ble_hid_param = {0};

static esp_hid_raw_report_map_t ble_report_maps[] = {
    {
        .data = desc_hid_report,
        .len = 0,
    }
};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id          = 0xe502,
    .product_id         = 0xbbab,
    .version            = 0x0100,
    .device_name        = "ESP-KEYBOARD",
    .manufacturer_name  = "Espressif",
    .serial_number      = "1234567890",
    .report_maps        = ble_report_maps,
    .report_maps_len    = 1
};
static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT: {
        ESP_LOGI(TAG, "START");
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
        s_ble_hid_param.is_connected = true;

        ESP_LOGI(TAG, "CONNECT");
        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT: {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT: {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        break;
    }
    case ESP_HIDD_OUTPUT_EVENT: {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
        break;
    }
    case ESP_HIDD_FEATURE_EVENT: {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
        s_ble_hid_param.is_connected = false;
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_STOP_EVENT: {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}

esp_err_t ble_hid_init(void)
{
    esp_err_t ret;
    ret = esp_hid_gap_init();
    ESP_ERROR_CHECK(ret);

    ble_hid_config.report_maps[0].len = desc_hid_report_len;

    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, ble_hid_config.device_name);
    ESP_ERROR_CHECK(ret);

    if ((ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler)) != ESP_OK) {
        ESP_LOGE(TAG, "GATTS register callback failed: %d", ret);
    }
    ESP_LOGI(TAG, "setting ble device");
    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev));
    return ret;
}
esp_err_t ble_hid_deinit(void)
{
    ESP_ERROR_CHECK(esp_hidd_dev_deinit(s_ble_hid_param.hid_dev));
    ESP_ERROR_CHECK(esp_hid_gap_deinit());
    return ESP_OK;
}
void ble_hid_keyboard_report(hid_report_t report)
{
    static bool use_full_key = false;
    if (s_ble_hid_param.is_connected == false) {
        return;
    }

    switch (report.report_id) {
    case REPORT_ID_FULL_KEY_KEYBOARD:
        use_full_key = true;
        esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, REPORT_ID_FULL_KEY_KEYBOARD, (uint8_t *)&report.keyboard_full_key_report, sizeof(report.keyboard_full_key_report));
        break;
    case REPORT_ID_KEYBOARD: {
        if (use_full_key) {
            hid_report_t _report = {0};
            esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, REPORT_ID_FULL_KEY_KEYBOARD, (uint8_t *)&_report.keyboard_full_key_report, sizeof(_report.keyboard_full_key_report));
            use_full_key = false;
        }
        esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, REPORT_ID_KEYBOARD, (uint8_t *)&report.keyboard_report, sizeof(report.keyboard_report));
        break;
    }
    case REPORT_ID_CONSUMER:
        esp_hidd_dev_input_set(s_ble_hid_param.hid_dev, 0, REPORT_ID_CONSUMER, (uint8_t *)&report.consumer_report, sizeof(report.consumer_report));
        break;
    default:
        break;
    }
}
