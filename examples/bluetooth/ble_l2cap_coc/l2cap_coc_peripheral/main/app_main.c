/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"

static const char *TAG = "app_main";

#define BLE_CONN_MGR_L2CAP_COC_MTU CONFIG_BLE_CONN_MGR_L2CAP_COC_MTU
#define L2CAP_COC_PSM CONFIG_EXAMPLE_L2CAP_COC_PSM

/* Enable raw data logging for L2CAP CoC receive events */
#ifndef ENABLE_L2CAP_COC_RAW_DATA_LOG
#define ENABLE_L2CAP_COC_RAW_DATA_LOG  0
#endif

static uint16_t s_conn_handle = 0;
static esp_ble_conn_l2cap_coc_chan_t s_coc_chan = NULL;
static uint16_t s_peer_sdu_size = 0;
static uint8_t s_peer_addr[6];
static bool s_peer_addr_valid = false;
static TaskHandle_t s_send_task_handle = NULL;

static void app_ble_conn_l2cap_coc_test_send(esp_ble_conn_l2cap_coc_chan_t *chan)
{
    if (!chan || !*chan) {
        ESP_LOGW(TAG, "L2CAP CoC test send: invalid channel");
        return;
    }

    if (s_peer_sdu_size == 0) {
        ESP_LOGW(TAG, "L2CAP CoC test send: peer SDU size is 0");
        return;
    }

    uint8_t *resp_buf = (uint8_t *)malloc(s_peer_sdu_size);
    if (!resp_buf) {
        ESP_LOGW(TAG, "L2CAP CoC test send: alloc failed");
        return;
    }

    for (uint16_t i = 0; i < s_peer_sdu_size; i++) {
        resp_buf[i] = (uint8_t)(i & 0xFF);
    }

    esp_ble_conn_l2cap_coc_sdu_t sdu = {
        .data = resp_buf,
        .len = s_peer_sdu_size,
    };
    esp_err_t rc = esp_ble_conn_l2cap_coc_send(*chan, &sdu);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "L2CAP CoC send failed: %s", esp_err_to_name(rc));
    } else {
        ESP_LOGI(TAG, "[--TX--]L2CAP CoC data sent (%u bytes)", s_peer_sdu_size);
    }
    free(resp_buf);
}

static void app_ble_conn_l2cap_coc_send_task(void *arg)
{
    (void)arg;
    while (true) {
        if (s_coc_chan) {
            app_ble_conn_l2cap_coc_test_send(&s_coc_chan);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static int app_ble_conn_l2cap_coc_event_handler(esp_ble_conn_l2cap_coc_event_t *event, void *arg)
{
    esp_err_t rc = ESP_OK;

    if (!event) {
        return 0;
    }

    switch (event->type) {
    case ESP_BLE_CONN_L2CAP_COC_EVENT_CONNECTED:
        ESP_LOGI(TAG, "L2CAP CoC connected, handle=%u, status=%d",
                 event->connect.conn_handle, event->connect.status);
        if (event->connect.status == 0) {
            esp_ble_conn_l2cap_coc_chan_info_t chan_info = {0};
            if (esp_ble_conn_l2cap_coc_get_chan_info(event->connect.chan, &chan_info) == ESP_OK) {
                ESP_LOGI(TAG, "L2CAP CoC chan info: psm=0x%04x scid=0x%04x dcid=0x%04x "
                         "our_mps=%u our_mtu=%u peer_mps=%u peer_mtu=%u",
                         chan_info.psm, chan_info.scid, chan_info.dcid,
                         chan_info.our_l2cap_mtu, chan_info.our_coc_mtu,
                         chan_info.peer_l2cap_mtu, chan_info.peer_coc_mtu);
            }
            s_coc_chan = event->connect.chan;
            if (!s_send_task_handle) {
                BaseType_t ret = xTaskCreate(app_ble_conn_l2cap_coc_send_task, "l2cap_coc_send_task", 4096,
                                             NULL, 10, &s_send_task_handle);
                if (ret != pdPASS) {
                    ESP_LOGE(TAG, "Failed to create send task");
                }
            }
        }
        break;
    case ESP_BLE_CONN_L2CAP_COC_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "L2CAP CoC disconnected, handle=%u", event->disconnect.conn_handle);
        s_coc_chan = NULL;
        if (s_send_task_handle) {
            vTaskDelete(s_send_task_handle);
            s_send_task_handle = NULL;
        }
        break;
    case ESP_BLE_CONN_L2CAP_COC_EVENT_ACCEPT:
        ESP_LOGI(TAG, "L2CAP CoC accept, handle=%u, peer_sdu_size=%u",
                 event->accept.conn_handle, event->accept.peer_sdu_size);
        s_peer_sdu_size = event->accept.peer_sdu_size;
        rc = esp_ble_conn_l2cap_coc_accept(event->accept.conn_handle,
                                           event->accept.peer_sdu_size,
                                           event->accept.chan);
        if (rc != ESP_OK) {
            ESP_LOGW(TAG, "L2CAP CoC accept failed: %s", esp_err_to_name(rc));
        }
        break;
    case ESP_BLE_CONN_L2CAP_COC_EVENT_DATA_RECEIVED:
        ESP_LOGI(TAG, "[--RX--]L2CAP CoC data received, handle=%u, len=%u",
                 event->receive.conn_handle, event->receive.sdu.len);
        /* Note: event->receive.sdu.data buffer is managed internally and will be freed
         * immediately after this callback returns. Do not save the pointer for later use.
         * If asynchronous processing is needed, copy the data before returning.
         */
#if ENABLE_L2CAP_COC_RAW_DATA_LOG
        if (event->receive.sdu.data && event->receive.sdu.len) {
            ESP_LOG_BUFFER_HEX(TAG, event->receive.sdu.data, event->receive.sdu.len);
        }
#endif
        rc = esp_ble_conn_l2cap_coc_recv_ready(event->receive.chan, BLE_CONN_MGR_L2CAP_COC_MTU);
        if (rc != ESP_OK) {
            ESP_LOGW(TAG, "L2CAP CoC recv ready failed: %s", esp_err_to_name(rc));
        }
        break;
    default:
        break;
    }

    return 0;
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        if (esp_ble_conn_get_conn_handle(&s_conn_handle) == ESP_OK &&
                esp_ble_conn_get_peer_addr(s_peer_addr) == ESP_OK) {
            s_peer_addr_valid = true;
        }

        if (s_peer_addr_valid) {
            ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED, conn_id=%u, peer=" BLE_CONN_MGR_ADDR_STR,
                     s_conn_handle, BLE_CONN_MGR_ADDR_HEX(s_peer_addr));
        } else {
            ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED, conn_id=%u", s_conn_handle);
        }

        ESP_ERROR_CHECK(esp_ble_conn_l2cap_coc_create_server(L2CAP_COC_PSM, BLE_CONN_MGR_L2CAP_COC_MTU,
                                                             app_ble_conn_l2cap_coc_event_handler, NULL));
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED: {
        uint16_t reason = 0;
        bool has_reason = (esp_ble_conn_get_disconnect_reason(&reason) == ESP_OK);
        if (s_peer_addr_valid) {
            if (has_reason) {
                ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED, conn_id=%u, peer=" BLE_CONN_MGR_ADDR_STR ", reason=0x%04x",
                         s_conn_handle, BLE_CONN_MGR_ADDR_HEX(s_peer_addr), reason);
            } else {
                ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED, conn_id=%u, peer=" BLE_CONN_MGR_ADDR_STR,
                         s_conn_handle, BLE_CONN_MGR_ADDR_HEX(s_peer_addr));
            }
        } else if (has_reason) {
            ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED, conn_id=%u, reason=0x%04x",
                     s_conn_handle, reason);
        } else {
            ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED, conn_id=%u", s_conn_handle);
        }

        s_conn_handle = 0;
        s_peer_addr_valid = false;
        break;
    }
    default:
        break;
    }
}

void app_main(void)
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
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_l2cap_coc_mem_init());

    ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager: %s", esp_err_to_name(ret));
        esp_ble_conn_l2cap_coc_mem_release();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        esp_event_loop_delete_default();
        ESP_LOGE(TAG, "Application initialization failed.");
    }
}
