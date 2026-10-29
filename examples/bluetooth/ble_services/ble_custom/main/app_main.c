/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"

static const char *TAG = "app_main";

/* 16 Bit Custom Service UUID */
#define APP_CUSTOM_SVC_UUID16               0xFFA0

/* 16 Bit Custom Notify Characteristic UUID */
#define APP_CUSTOM_NTF_CHR_UUID16           0xFFA1

/* 16 Bit Custom Indicate Characteristic UUID */
#define APP_CUSTOM_IND_CHR_UUID16           0xFFA2

static volatile bool s_notify_enabled;
static volatile bool s_indicate_enabled;
static volatile uint16_t s_conn_handle = BLE_CONN_HANDLE_INVALID;
static TaskHandle_t s_monitor_task;

static uint8_t s_value[64] = "hello";
static uint16_t s_value_len = 5;

static const uint8_t s_notify_payload[] = "enable notify";
static const uint8_t s_indicate_payload[] = "enable indicate";

static void app_monitor_wakeup(void)
{
    if (s_monitor_task) {
        xTaskNotifyGive(s_monitor_task);
    }
}

static esp_err_t app_get_chr_handle(uint16_t chr_uuid16, uint16_t *out_handle)
{
    esp_ble_conn_uuid_t svc_uuid = { .uuid16 = APP_CUSTOM_SVC_UUID16 };
    esp_ble_conn_uuid_t chr_uuid = { .uuid16 = chr_uuid16 };

    return esp_ble_conn_get_chr_handle(BLE_CONN_UUID_TYPE_16, svc_uuid,
                                       BLE_CONN_UUID_TYPE_16, chr_uuid,
                                       0, out_handle);
}

static void app_monitor_task(void *arg)
{
    bool last_notify_enabled = false;
    bool last_indicate_enabled = false;

    (void)arg;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        bool notify_enabled = s_notify_enabled;
        bool indicate_enabled = s_indicate_enabled;
        uint16_t conn_handle = s_conn_handle;

        if (conn_handle == BLE_CONN_HANDLE_INVALID) {
            last_notify_enabled = notify_enabled;
            last_indicate_enabled = indicate_enabled;
            continue;
        }

        if (notify_enabled && !last_notify_enabled) {
            uint16_t attr_handle = 0;
            esp_err_t err = app_get_chr_handle(APP_CUSTOM_NTF_CHR_UUID16, &attr_handle);
            if (err == ESP_OK) {
                err = esp_ble_conn_notify_by_attr_handle(conn_handle, attr_handle,
                                                         s_notify_payload, sizeof(s_notify_payload) - 1);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Notify sent on conn=%u attr=%u", conn_handle, attr_handle);
                } else {
                    ESP_LOGW(TAG, "Notify failed: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGW(TAG, "esp_ble_conn_get_chr_handle(notify) failed: %s", esp_err_to_name(err));
            }
        }

        if (indicate_enabled && !last_indicate_enabled) {
            uint16_t attr_handle = 0;
            esp_err_t err = app_get_chr_handle(APP_CUSTOM_IND_CHR_UUID16, &attr_handle);
            if (err == ESP_OK) {
                err = esp_ble_conn_indicate_by_attr_handle(conn_handle, attr_handle,
                                                           s_indicate_payload, sizeof(s_indicate_payload) - 1);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "Indicate sent on conn=%u attr=%u", conn_handle, attr_handle);
                } else {
                    ESP_LOGW(TAG, "Indicate failed: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGW(TAG, "esp_ble_conn_get_chr_handle(indicate) failed: %s", esp_err_to_name(err));
            }
        }

        last_notify_enabled = notify_enabled;
        last_indicate_enabled = indicate_enabled;
    }
}

static esp_err_t app_chr_cb(const uint8_t *inbuf, uint16_t inlen,
                            uint8_t **outbuf, uint16_t *outlen, void *priv_data, uint8_t *att_status)
{
    (void)priv_data;

    if (!outbuf || !outlen || !att_status) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!inbuf) {
        *outlen = s_value_len;
        *outbuf = malloc(s_value_len ? s_value_len : 1);
        if (!*outbuf) {
            *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
            return ESP_ERR_NO_MEM;
        }
        if (s_value_len > 0) {
            memcpy(*outbuf, s_value, s_value_len);
        }
    } else {
        ESP_LOGI(TAG, "Write received, len=%u", inlen);
        if (inlen > 0) {
            ESP_LOG_BUFFER_HEXDUMP(TAG, inbuf, inlen, ESP_LOG_INFO);
        }

        s_value_len = (inlen > sizeof(s_value)) ? sizeof(s_value) : inlen;
        if (s_value_len > 0) {
            memcpy(s_value, inbuf, s_value_len);
        }

        if (inlen == 0) {
            *outbuf = NULL;
            *outlen = 0;
            *att_status = ESP_IOT_ATT_SUCCESS;
            return ESP_OK;
        }

        *outlen = s_value_len;
        *outbuf = malloc(s_value_len);
        if (!*outbuf) {
            *att_status = ESP_IOT_ATT_INSUF_RESOURCE;
            return ESP_ERR_NO_MEM;
        }
        memcpy(*outbuf, s_value, s_value_len);
    }

    *att_status = ESP_IOT_ATT_SUCCESS;
    return ESP_OK;
}

static const esp_ble_conn_character_t s_nu_lookup_table[] = {
    {
        "custom_ntf_chr", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_NOTIFY,
        { APP_CUSTOM_NTF_CHR_UUID16 }, app_chr_cb
    },
    {
        "custom_ind_chr", BLE_CONN_UUID_TYPE_16,
        BLE_CONN_GATT_CHR_INDICATE,
        { APP_CUSTOM_IND_CHR_UUID16 }, NULL
    },
};

static const esp_ble_conn_svc_t s_custom_svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = APP_CUSTOM_SVC_UUID16,
    },
    .nu_lookup_count = sizeof(s_nu_lookup_table) / sizeof(s_nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)s_nu_lookup_table,
};

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;

    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_STARTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_STARTED");
        break;
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        esp_ble_conn_event_data_t *ev = (esp_ble_conn_event_data_t *)event_data;
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED");
        s_conn_handle = ev ? ev->connected.conn_handle : BLE_CONN_HANDLE_INVALID;
        s_notify_enabled = false;
        s_indicate_enabled = false;
        app_monitor_wakeup();
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED");
        s_notify_enabled = false;
        s_indicate_enabled = false;
        s_conn_handle = BLE_CONN_HANDLE_INVALID;
        app_monitor_wakeup();
        break;
    case ESP_BLE_CONN_EVENT_CCCD_UPDATE: {
        esp_ble_conn_cccd_update_t *cccd = (esp_ble_conn_cccd_update_t *)event_data;

        if (!cccd || cccd->uuid_type != BLE_CONN_UUID_TYPE_16) {
            break;
        }

        s_conn_handle = cccd->conn_handle;
        if (cccd->uuid.uuid16 == APP_CUSTOM_NTF_CHR_UUID16) {
            s_notify_enabled = cccd->notify_enable;
            ESP_LOGI(TAG, "CCCD update: notify_enable=%d conn=%u",
                     cccd->notify_enable, cccd->conn_handle);
        } else if (cccd->uuid.uuid16 == APP_CUSTOM_IND_CHR_UUID16) {
            s_indicate_enabled = cccd->indicate_enable;
            ESP_LOGI(TAG, "CCCD update: indicate_enable=%d conn=%u",
                     cccd->indicate_enable, cccd->conn_handle);
        } else {
            break;
        }

        app_monitor_wakeup();
        break;
    }
    default:
        break;
    }
}

void
app_main(void)
{
    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_ADV_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV,
        .include_service_uuid = 1,
        .adv_uuid_type = BLE_CONN_UUID_TYPE_16,
        .adv_uuid16 = APP_CUSTOM_SVC_UUID16,
    };
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));

    BaseType_t task_ok = xTaskCreate(app_monitor_task, "ble_custom_mon", 4096, NULL, 5, &s_monitor_task);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitor task");
        return;
    }

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_add_svc(&s_custom_svc));

    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        if (s_monitor_task) {
            vTaskDelete(s_monitor_task);
            s_monitor_task = NULL;
        }
    }
}
