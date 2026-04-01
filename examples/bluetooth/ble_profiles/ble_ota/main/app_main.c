/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"
#include "esp_ble_ota_raw.h"

#include "ble_ota_raw.h"

#define BLE_OTA_RAW_ADV_UUID16 0x8018

/** Connection interval in HCI units (1 unit = 1.25 ms). 12 => 15 ms. */
#define APP_BLE_CONN_INTERVAL_UNITS           12

static const char *TAG = "ble_ota_raw";

static const uint8_t s_ble_ota_raw_manu_data[] = {
    0x0b, 0xff, 0x01, 0x01, 0x27, 0x95, 0x01, 0x00, 0x00, 0x00, 0xff, 0xff
};

static void app_ble_ota_raw_recv_fw_cb(uint8_t *buf, uint32_t length)
{
    if (!ble_ota_raw_recv_fw_cb((const uint8_t *)buf, length)) {
        ESP_LOGE(TAG, "Failed to queue firmware chunk");
    }
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base,
                                       int32_t id, void *event_data)
{
    (void)handler_args;

    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    const esp_ble_conn_event_data_t *d = (const esp_ble_conn_event_data_t *)event_data;

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        if (d) {
            ESP_LOGI(TAG, "CONNECTED conn_handle=%u peer_addr_type=%u peer=%02x:%02x:%02x:%02x:%02x:%02x",
                     (unsigned)d->connected.conn_handle,
                     (unsigned)d->connected.peer_addr_type,
                     d->connected.peer_addr[0], d->connected.peer_addr[1],
                     d->connected.peer_addr[2], d->connected.peer_addr[3],
                     d->connected.peer_addr[4], d->connected.peer_addr[5]);
        } else {
            ESP_LOGI(TAG, "CONNECTED (event_data is NULL)");
        }
        break;

    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        if (d) {
            ESP_LOGI(TAG, "DISCONNECTED conn_handle=%u reason=0x%04x peer_addr_type=%u peer=%02x:%02x:%02x:%02x:%02x:%02x",
                     (unsigned)d->disconnected.conn_handle,
                     (unsigned)d->disconnected.reason,
                     (unsigned)d->disconnected.peer_addr_type,
                     d->disconnected.peer_addr[0], d->disconnected.peer_addr[1],
                     d->disconnected.peer_addr[2], d->disconnected.peer_addr[3],
                     d->disconnected.peer_addr[4], d->disconnected.peer_addr[5]);
        } else {
            ESP_LOGI(TAG, "DISCONNECTED (event_data is NULL)");
        }
        break;

    case ESP_BLE_CONN_EVENT_MTU:
        if (d) {
            ESP_LOGI(TAG, "MTU conn_handle=%u channel_id=%u mtu=%u",
                     (unsigned)d->mtu_update.conn_handle,
                     (unsigned)d->mtu_update.channel_id,
                     (unsigned)d->mtu_update.mtu);
        } else {
            ESP_LOGI(TAG, "MTU (event_data is NULL)");
        }
        break;

    case ESP_BLE_CONN_EVENT_CONN_PARAM_UPDATE:
        if (d) {
            const esp_ble_conn_params_t *p = &d->conn_param_update.params;
            ESP_LOGI(TAG, "CONN_PARAM_UPDATE conn_handle=%u status=%d",
                     (unsigned)d->conn_param_update.conn_handle,
                     d->conn_param_update.status);
            ESP_LOGI(TAG, "  itvl_min=%u itvl_max=%u (x1.25ms) latency=%u supervision_timeout=%u (x10ms) min_ce_len=%u max_ce_len=%u",
                     (unsigned)p->itvl_min, (unsigned)p->itvl_max, (unsigned)p->latency,
                     (unsigned)p->supervision_timeout, (unsigned)p->min_ce_len, (unsigned)p->max_ce_len);
        } else {
            ESP_LOGI(TAG, "CONN_PARAM_UPDATE (event_data is NULL)");
        }
        break;

    default:
        break;
    }
}

void app_main(void)
{
    esp_ble_conn_config_t config = {0};
    size_t broadcast_data_size = sizeof(config.broadcast_data);
    size_t manu_data_size = sizeof(s_ble_ota_raw_manu_data);
    size_t copy_size = (broadcast_data_size < manu_data_size) ? broadcast_data_size : manu_data_size;

    strlcpy((char *)config.device_name, CONFIG_EXAMPLE_BLE_OTA_RAW_DEVICE_NAME,
            sizeof(config.device_name));
    memcpy(config.broadcast_data, s_ble_ota_raw_manu_data, copy_size);
    if (copy_size < manu_data_size) {
        ESP_LOGW(TAG, "Manufacturer data truncated: src=%u dst=%u",
                 (unsigned)manu_data_size, (unsigned)broadcast_data_size);
    }
    config.include_service_uuid = 1;
    config.adv_uuid_type = BLE_CONN_UUID_TYPE_16;
    config.adv_uuid16 = BLE_OTA_RAW_ADV_UUID16;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!ble_ota_raw_ringbuf_init(BLE_OTA_RAW_RINGBUF_DEFAULT_SIZE)) {
        ESP_LOGE(TAG, "%s init ringbuf fail", __func__);
        return;
    }

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_ota_raw_init());
    ESP_ERROR_CHECK(esp_ble_ota_raw_recv_fw_data_callback(app_ble_ota_raw_recv_fw_cb));
    if (!ble_ota_raw_task_init()) {
        ESP_LOGE(TAG, "%s init ota task fail", __func__);
        return;
    }
    ESP_ERROR_CHECK(esp_ble_conn_start());
}
