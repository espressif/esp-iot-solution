/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * BLE MIDI Central (MIDI In / host role)
 *
 * Scans for the ble_midi_peripheral example (BLE-MIDI service UUID and/or name),
 * connects, discovers GATT, enables notifications on the MIDI I/O characteristic,
 * and prints received BLE-MIDI data using the same profile parsers as the peripheral.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"
#include "esp_ble_midi.h"
#include "esp_ble_midi_svc.h"

static const char *TAG = "ble_midi_central";

static const uint8_t s_midi_svc_uuid[] = BLE_MIDI_SERVICE_UUID128;
static const uint8_t s_midi_char_uuid[] = BLE_MIDI_CHAR_UUID128;

static bool s_target_found;
static uint16_t s_conn_handle = BLE_CONN_HANDLE_INVALID;

static bool uuid128_adv_match(const uint8_t *adv, uint8_t adv_len)
{
    const uint8_t *p = NULL;
    uint8_t plen = 0;

    if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_UUID128_COMPLETE, &p, &plen) == ESP_OK) {
        if (plen == 16 && memcmp(p, s_midi_svc_uuid, 16) == 0) {
            return true;
        }
    }
    if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_UUID128_INCOMP, &p, &plen) == ESP_OK) {
        if (plen == 16 && memcmp(p, s_midi_svc_uuid, 16) == 0) {
            return true;
        }
    }
    return false;
}

static bool peer_name_match(const uint8_t *adv, uint8_t adv_len)
{
    const char *want = CONFIG_EXAMPLE_PEER_NAME;
    if (want[0] == '\0') {
        return false;
    }

    const uint8_t *name_ptr = NULL;
    uint8_t name_len = 0;
    size_t want_len = strlen(want);

    if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_NAME_COMPLETE, &name_ptr, &name_len) != ESP_OK) {
        if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_NAME_SHORT, &name_ptr, &name_len) != ESP_OK) {
            return false;
        }
    }

    if (name_len != want_len) {
        return false;
    }
    return (memcmp(name_ptr, want, want_len) == 0);
}

static bool app_ble_conn_scan_cb(const esp_ble_conn_scan_result_t *result, void *arg)
{
    (void)arg;
    if (s_target_found || !result) {
        return false;
    }

    ESP_LOGD(TAG, "scan: addr=" BLE_CONN_MGR_ADDR_STR " rssi=%d adv_len=%u",
             BLE_CONN_MGR_ADDR_HEX(result->addr), result->rssi, result->adv_data_len);

    if (uuid128_adv_match(result->adv_data, result->adv_data_len) || peer_name_match(result->adv_data, result->adv_data_len)) {
        ESP_LOGI(TAG, "Matched BLE-MIDI peripheral, stopping scan");
        s_target_found = true;
        esp_ble_conn_scan_stop();
        return true;
    }

    return false;
}

static void midi_rx_cb(const uint8_t *data, uint16_t len, void *user_ctx)
{
    (void)user_ctx;
    ESP_LOGI(TAG, "MIDI RX BEP (%u bytes):", len);
    ESP_LOG_BUFFER_HEX(TAG, data, len);
}

static void midi_evt_cb(uint16_t ts_ms, esp_ble_midi_event_type_t event_type, const uint8_t *msg, uint16_t msg_len)
{
    if (event_type == ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW) {
        ESP_LOGW(TAG, "MIDI EVT ts=%u: SysEx buffer overflow", ts_ms);
        return;
    }
    ESP_LOGI(TAG, "MIDI EVT ts=%u len=%u", ts_ms, msg_len);
    if (msg == NULL || msg_len == 0) {
        return;
    }
    char line[96] = {0};
    size_t pos = 0;
    for (int i = 0; i < msg_len; i++) {
        int written = snprintf(&line[pos], sizeof(line) - pos, "%02X ", msg[i]);
        if (written > 0) {
            pos += (size_t)written;
            if (pos >= sizeof(line)) {
                pos = sizeof(line) - 1;
            }
        }
        if (pos > (sizeof(line) - 4)) {
            ESP_LOGI(TAG, "%s", line);
            pos = 0;
            line[0] = 0;
        }
    }
    if (pos) {
        ESP_LOGI(TAG, "%s", line);
    }
}

static bool conn_data_is_midi_io(const esp_ble_conn_data_t *d)
{
    if (!d || d->type != BLE_CONN_UUID_TYPE_128) {
        return false;
    }
    return memcmp(d->uuid.uuid128, s_midi_char_uuid, sizeof(d->uuid.uuid128)) == 0;
}

static void subscribe_midi_notifications(uint16_t conn_handle)
{
    static uint8_t cccd_notify[2] = {0x01, 0x00};

    esp_ble_conn_data_t sub = {
        .type = BLE_CONN_UUID_TYPE_128,
        .write_conn_id = conn_handle,
        .data = cccd_notify,
        .data_len = sizeof(cccd_notify),
    };
    memcpy(sub.uuid.uuid128, s_midi_char_uuid, sizeof(sub.uuid.uuid128));

    esp_err_t err = esp_ble_conn_subscribe_by_handle(conn_handle, ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Subscribe MIDI notify failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Subscribed to MIDI I/O notifications");
    }
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        if (esp_ble_conn_get_conn_handle(&s_conn_handle) == ESP_OK) {
            if (s_conn_handle == BLE_CONN_HANDLE_INVALID) {
                ESP_LOGW(TAG, "Connected event but connection handle is invalid");
            } else {
                ESP_LOGI(TAG, "Connected, conn_handle=%u", s_conn_handle);
                esp_err_t mtu_rc = esp_ble_conn_mtu_update(s_conn_handle, 185);
                if (mtu_rc != ESP_OK) {
                    ESP_LOGW(TAG, "MTU exchange failed: %s", esp_err_to_name(mtu_rc));
                }
            }
        }
        break;

    case ESP_BLE_CONN_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "GATT discovery complete");
        if (s_conn_handle != BLE_CONN_HANDLE_INVALID) {
            subscribe_midi_notifications(s_conn_handle);
        }
        break;

    case ESP_BLE_CONN_EVENT_DATA_RECEIVE: {
        esp_ble_conn_data_t *conn_data = (esp_ble_conn_data_t *)event_data;
        if (!conn_data || !conn_data->data || conn_data->data_len == 0) {
            break;
        }
        if (conn_data_is_midi_io(conn_data)) {
            esp_ble_midi_on_bep_received(conn_data->data, conn_data->data_len);
        } else {
            ESP_LOGW(TAG, "DATA_RECEIVE for non-MIDI UUID, len=%u", conn_data->data_len);
        }
        break;
    }

    case ESP_BLE_CONN_EVENT_MTU: {
        esp_ble_conn_event_data_t *evt = (esp_ble_conn_event_data_t *)event_data;
        if (evt) {
            ESP_LOGI(TAG, "MTU updated: effective=%u", evt->mtu_update.mtu);
        }
        break;
    }

    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        s_conn_handle = BLE_CONN_HANDLE_INVALID;
        s_target_found = false;
        if (esp_ble_conn_scan_start() != ESP_OK) {
            ESP_LOGW(TAG, "Failed to restart scan");
        }
        break;

    default:
        break;
    }
}

void app_main(void)
{
    esp_ble_conn_config_t config = {0};
    strncpy((char *)config.device_name, CONFIG_EXAMPLE_LOCAL_NAME, sizeof(config.device_name) - 1);
    strncpy((char *)config.broadcast_data, CONFIG_EXAMPLE_BLE_SUB_ADV, sizeof(config.broadcast_data) - 1);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_register_scan_callback(app_ble_conn_scan_cb, NULL));

    ESP_ERROR_CHECK(esp_ble_midi_svc_init());
    ESP_ERROR_CHECK(esp_ble_midi_profile_init());
    ESP_ERROR_CHECK(esp_ble_midi_register_rx_cb(midi_rx_cb, NULL));
    ESP_ERROR_CHECK(esp_ble_midi_register_event_cb(midi_evt_cb));

    ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE: %s", esp_err_to_name(ret));
        esp_ble_midi_profile_deinit();
        esp_ble_midi_svc_deinit();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        return;
    }

    ESP_LOGI(TAG, "Central started; scanning for BLE-MIDI peripheral (UUID%s%s)",
             CONFIG_EXAMPLE_PEER_NAME[0] ? "; name match: " : "",
             CONFIG_EXAMPLE_PEER_NAME[0] ? CONFIG_EXAMPLE_PEER_NAME : "");
}
