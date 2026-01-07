/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_ble_conn_mgr.h"
#include "esp_ble_midi.h"
#include "esp_ble_midi_svc.h"

static const char *TAG = "app_main";
static TaskHandle_t s_midi_task = NULL;

static void midi_rx_cb(const uint8_t *data, uint16_t len, void *user_ctx)
{
    (void)user_ctx; /* Unused in this example */
    ESP_LOGI(TAG, "MIDI RX (%u):", len);
    ESP_LOG_BUFFER_HEX(TAG, data, len);
}

static void midi_evt_cb(uint16_t ts_ms, esp_ble_midi_event_type_t event_type, const uint8_t *msg, uint16_t msg_len)
{
    if (event_type == ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW) {
        ESP_LOGW(TAG, "MIDI EVT ts=%u: SysEx buffer overflow detected", ts_ms);
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
            pos += written;
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

static uint16_t midi_now_ts_ms(void)
{
    return esp_ble_midi_get_timestamp_ms();
}

static void midi_scale_task(void *arg)
{
    const uint8_t channel = 0; /* ch 1 */
    const uint8_t scale[] = { 60, 62, 64, 65, 67, 69, 71, 72 }; /* C major */
    const TickType_t on_time = pdMS_TO_TICKS(100);
    const TickType_t step_time = pdMS_TO_TICKS(300);

    /* Check if notification is still enabled before sending */
    if (!esp_ble_midi_is_notify_enabled()) {
        ESP_LOGW(TAG, "Notification disabled, exiting MIDI task");
        goto exit_task;
    }

    /* Send only one cycle to verify that the uplink data path is functioning correctly */
    for (size_t i = 0; i < sizeof(scale); i++) {
        /* Check notification state before each note */
        if (!esp_ble_midi_is_notify_enabled()) {
            ESP_LOGW(TAG, "Notification disabled during playback, exiting MIDI task");
            goto exit_task;
        }

        uint16_t ts = midi_now_ts_ms();
        esp_err_t ret = esp_ble_midi_send_note_on(channel, scale[i], 100, ts);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send note_on: %d", ret);
            /* If notification was disabled, exit */
            if (ret == ESP_ERR_INVALID_STATE) {
                goto exit_task;
            }
        }

        vTaskDelay(on_time);

        /* Check again before note_off */
        if (!esp_ble_midi_is_notify_enabled()) {
            goto exit_task;
        }

        ts = midi_now_ts_ms();
        ret = esp_ble_midi_send_note_off(channel, scale[i], 64, ts);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send note_off: %d", ret);
            if (ret == ESP_ERR_INVALID_STATE) {
                goto exit_task;
            }
        }

        vTaskDelay(step_time);
    }

exit_task:
    ESP_LOGI(TAG, "MIDI scale task exiting");
    s_midi_task = NULL;
    vTaskDelete(NULL);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        /* Reset notification and indication state on new connection */
        esp_ble_midi_set_notify_enabled(false);
        /* Ask for low-latency params via conn_mgr: 7.5 ms, latency 0, timeout 4 s.
         * The central has the final say and may refuse or adjust. */
        {
            uint16_t conn_handle = 0;
            if (esp_ble_conn_get_conn_handle(&conn_handle) == ESP_OK && conn_handle != 0) {
                esp_ble_conn_params_t params = {
                    .itvl_min = 0x0006,
                    .itvl_max = 0x0006,
                    .latency  = 0x0000,
                    .supervision_timeout = 400,
                    .min_ce_len = 0,
                    .max_ce_len = 0,
                };
                esp_err_t rc = esp_ble_conn_update_params(conn_handle, &params);
                if (rc != ESP_OK) {
                    ESP_LOGW(TAG, "esp_ble_conn_update_params rc=%d", rc);
                }
            }
        }
        break;
    case ESP_BLE_CONN_EVENT_CCCD_UPDATE: {
        esp_ble_conn_cccd_update_t *cccd_update = (esp_ble_conn_cccd_update_t *)event_data;
        ESP_LOGI(TAG, "CCCD updated: notify=%d, indicate=%d for char handle=%d",
                 cccd_update->notify_enable, cccd_update->indicate_enable, cccd_update->char_handle);

        /* Check if this is the MIDI characteristic CCCD */
        static const uint8_t midi_char_uuid128[] = BLE_MIDI_CHAR_UUID128;
        bool is_midi_char = false;

        if (cccd_update->uuid_type == BLE_CONN_UUID_TYPE_128) {
            is_midi_char = (memcmp(cccd_update->uuid.uuid128, midi_char_uuid128, sizeof(midi_char_uuid128)) == 0);
        }

        if (is_midi_char) {
            /* Start MIDI task only when notification is enabled */
            if (cccd_update->notify_enable && s_midi_task == NULL) {
                /* Update MIDI notification and indication state */
                esp_ble_midi_set_notify_enabled(true);
                ESP_LOGI(TAG, "MIDI notification enabled, starting MIDI task");
                BaseType_t task_result = xTaskCreate(midi_scale_task, "midi_scale", 4096, NULL, 5, &s_midi_task);
                if (task_result != pdPASS) {
                    ESP_LOGE(TAG, "Failed to create MIDI task, disabling notifications");
                    esp_ble_midi_set_notify_enabled(false);
                    s_midi_task = NULL;
                }
            } else if (!cccd_update->notify_enable && s_midi_task != NULL) {
                ESP_LOGI(TAG, "MIDI notification disabled, task will exit on next check");
                /* Update MIDI notification and indication state */
                esp_ble_midi_set_notify_enabled(false);
                /* Optionally delete the task immediately for cleaner lifecycle */
                vTaskDelete(s_midi_task);
                s_midi_task = NULL;
            }
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED");
        /* Reset notification and indication state on disconnect */
        esp_ble_midi_set_notify_enabled(false);
        break;
    default:
        break;
    }
}

void app_main(void)
{
    esp_err_t ret;

    /* BLE MIDI Service UUID (128-bit): 03B80E5A-EDE8-4B33-A751-6CE34EC4C700 (LSB first) */
    static const uint8_t midi_service_uuid128[] = BLE_MIDI_SERVICE_UUID128;
    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV,
        .include_service_uuid = 1,                    /* Include service UUID in advertising */
        .adv_uuid_type = BLE_CONN_UUID_TYPE_128,      /* Use 128-bit UUID */
    };
    /* Copy UUID to config structure (LSB first format) */
    memcpy(config.adv_uuid128, midi_service_uuid128, sizeof(config.adv_uuid128));

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    /* Prefer a larger ATT MTU for better BLE-MIDI throughput */
    ESP_ERROR_CHECK(esp_ble_conn_set_mtu(185));
    ESP_ERROR_CHECK(esp_ble_midi_svc_init()); /* Register BLE-MIDI service and IO characteristic */
    ESP_ERROR_CHECK(esp_ble_midi_profile_init()); /* Initialize BLE MIDI profile (must be called before registering callbacks) */
    ESP_ERROR_CHECK(esp_ble_midi_register_rx_cb(midi_rx_cb, NULL)); /* Register RX callback with NULL user context */
    ESP_ERROR_CHECK(esp_ble_midi_register_event_cb(midi_evt_cb));

    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_midi_profile_deinit();
        esp_ble_midi_svc_deinit();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        return;
    }
}
