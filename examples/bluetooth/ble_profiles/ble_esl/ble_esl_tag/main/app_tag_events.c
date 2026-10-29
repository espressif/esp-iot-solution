/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "app_tag_events.h"

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "esl_tag_hal.h"
#include "esl_tag_storage.h"
#include "esl_tag_image_store.h"
#include "esl_tag_assoc.h"
#include "ble_esl.h"
#include "esp_eslp_tag.h"

static const char *TAG = "tag_events";

static const char *pawr_opcode_name(uint8_t opcode)
{
    switch (opcode) {
    case BLE_ESL_CMD_PING: return "ping";
    case BLE_ESL_CMD_REFRESH_DISPLAY: return "refresh_display";
    case BLE_ESL_CMD_DISPLAY_IMAGE: return "display_image";
    case BLE_ESL_CMD_LED_CONTROL: return "led_control";
    case BLE_ESL_CMD_READ_SENSOR: return "read_sensor";
    default: return NULL;
    }
}

static void save_association_snapshot(void)
{
    ble_esl_persisted_tag_t info;
    esp_err_t ret = ble_esl_export_persisted_tag(&info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "export persisted tag failed: %s — restart will not restore",
                 esp_err_to_name(ret));
        return;
    }

    esl_tag_assoc_blob_t blob;
    memset(&blob, 0, sizeof(blob));
    blob.esl_address = info.esl_address;
    blob.ap_sync_key = info.ap_sync_key;
    blob.resp_key = info.resp_key;
    blob.peer_addr_type = info.peer_addr_type;
    memcpy(blob.peer_addr, info.peer_addr, 6);

    ret = esl_tag_storage_save_assoc(&blob);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "save tag_assoc failed: %s — restart will not restore",
                 esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Association snapshot saved");
}

static void erase_association_snapshot(const char *reason)
{
    esp_err_t ret = esl_tag_storage_erase_assoc();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "CRITICAL: tag_assoc erase failed during %s: %s",
                 reason, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "tag_assoc erased (%s)", reason);
    }
}

void app_tag_events_handle(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_ESLP_EVENTS) {
        return;
    }

    switch (id) {
    case BLE_ESLP_EVENT_STATE_CHANGED: {
        const esp_ble_eslp_state_changed_t *ev = event_data;
        if (!ev) {
            break;
        }
        ESP_LOGI(TAG, "State changed: %d -> %d", ev->prev_state, ev->new_state);
        if (ev->prev_state == BLE_ESLP_STATE_CONFIGURING &&
                ev->new_state == BLE_ESLP_STATE_SYNCHRONIZED) {
            save_association_snapshot();
        }
        break;
    }
    case BLE_ESLP_EVENT_IMAGE_WRITTEN: {
        const esp_ble_eslp_image_written_t *ev = event_data;
        if (!ev || ev->status != ESP_OK) {
            break;
        }
        esp_err_t pret = esl_tag_image_store_queue_save(ev->image_index);
        if (pret != ESP_OK) {
            ESP_LOGE(TAG, "image persist queue failed index=%u: %s",
                     ev->image_index, esp_err_to_name(pret));
        }
        break;
    }
    case BLE_ESLP_EVENT_DISPLAY_IMAGE: {
        const esp_ble_eslp_display_image_t *ev = event_data;
        if (ev) {
            ESP_LOGI(TAG, "Display image display=%u image=%u",
                     ev->display_index, ev->image_index);
            esp_err_t ret = esl_tag_display_show(ev->display_index, ev->image_index);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "display_show failed: %s", esp_err_to_name(ret));
            }
        }
        break;
    }
    case BLE_ESLP_EVENT_REFRESH_DISPLAY: {
        const esp_ble_eslp_refresh_display_t *ev = event_data;
        if (ev) {
            ESP_LOGI(TAG, "Refresh display index=%u", ev->display_index);
            esp_err_t ret = esl_tag_display_refresh(ev->display_index);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "display_refresh failed: %s (need a prior display_image)",
                         esp_err_to_name(ret));
            }
        }
        break;
    }
    case BLE_ESLP_EVENT_SENSOR_READ: {
        const esp_ble_eslp_sensor_read_t *ev = event_data;
        if (!ev) {
            break;
        }
        uint8_t sensor_data[8];
        size_t sensor_len = 0;
        if (esl_tag_sensor_read(ev->sensor_index, sensor_data, sizeof(sensor_data),
                                &sensor_len) == ESP_OK) {
            (void)esp_ble_eslp_report_sensor_data(ev->sensor_index, 0, sensor_data, sensor_len);
        }
        break;
    }
    case BLE_ESLP_EVENT_LED_CONTROL: {
        const esp_ble_eslp_led_control_t *ev = event_data;
        if (ev) {
            esl_tag_led_apply(ev->led_index, ev);
        }
        break;
    }
    case BLE_ESLP_EVENT_PAWR_COMMAND: {
        const esp_ble_eslp_pawr_command_t *ev = event_data;
        if (!ev || ev->data_len < 2) {
            break;
        }
        const uint8_t *tlvs = &ev->data[1];
        uint8_t tlvs_len = (uint8_t)(ev->data_len - 1);
        uint8_t offset = 0;
        while (offset < tlvs_len) {
            uint8_t opcode = tlvs[offset];
            uint8_t total = (uint8_t)BLE_ESL_TLV_TOTAL_LEN(opcode);
            if (total < 2 || (uint16_t)(offset + total) > tlvs_len) {
                break;
            }
            uint8_t esl_id = tlvs[offset + 1];
            const char *name = pawr_opcode_name(opcode);
            if (name != NULL) {
                ESP_LOGI(TAG, "PAwR %s esl_id=%u group=%u", name, esl_id,
                         (unsigned)(ev->data[0] & 0x7F));
            } else {
                ESP_LOGI(TAG, "PAwR opcode=0x%02x esl_id=%u group=%u", opcode, esl_id,
                         (unsigned)(ev->data[0] & 0x7F));
            }
            offset = (uint8_t)(offset + total);
        }
        break;
    }
    case BLE_ESLP_EVENT_UNASSOCIATED:
        ESP_LOGI(TAG, "Unassociated");
        if (esl_tag_image_store_queue_erase_all() != ESP_OK) {
            ESP_LOGE(TAG, "CRITICAL: image erase-all queue failed (unassociate); assoc kept");
            break;
        }
        erase_association_snapshot("unassociate");
        break;
    default:
        break;
    }
}
