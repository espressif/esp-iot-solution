/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * BLE OTP Client Example
 * ---------------------
 * This example runs a fixed 9-step demo flow against an OTP server (e.g. ble_otp_server):
 *   1–2. Discover OTS, read feature, select first object
 *   3.    Read first object content and print
 *   4–5. Append string A, then full read
 *   6–7. Append string B, then full read
 *   8.    Delete current object
 *   9.    Select first again (no object if all deleted), optionally read next object
 * The demo flow task and event handler communicate via a message queue so that
 * async OTP events (OLCP/OACP responses, transfer complete) drive the next step.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_conn_mgr.h"
#include "esp_otp.h"

static const char *TAG = "ble_otp_client";

/*
 * Demo flow control: the demo flow task blocks on the queue; the event handler
 * posts these message types when OTP events arrive so the task can proceed.
 */
typedef enum {
    MSG_OTS_DISCOVERED = 0,
    MSG_OLCP_FIRST_OK,
    MSG_OLCP_FIRST_FAIL,
    MSG_TRANSFER_COMPLETE,
    MSG_TRANSFER_ERROR,
    MSG_OACP_RESPONSE,
} otp_demo_msg_type_t;

typedef struct {
    otp_demo_msg_type_t msg;
    uint8_t oacp_op;   /* For MSG_OACP_RESPONSE: request op code (e.g. BLE_OTS_OACP_DELETE) */
    uint8_t oacp_rsp;  /* For MSG_OACP_RESPONSE: response code (e.g. BLE_OTS_OACP_RSP_SUCCESS) */
} otp_demo_msg_t;

#define OTP_DEMO_QUEUE_LEN    8
#define OTP_DEMO_STACK        4096

static QueueHandle_t s_otp_demo_queue = NULL;
static uint16_t s_conn_handle = 0;
static bool s_target_found = false;

/* Object size used as append offset; updated after read_object_info and after each write complete */
static uint32_t s_expected_size = 0;
/* For append steps: payload sent on TRANSFER_CHANNEL_CONNECTED, then channel closed.
 * Access to s_pending_write / s_pending_write_len / s_write_pending is protected by s_pending_write_mux
 * so the event handler observes consistent pointer/length when it sees s_write_pending == true. */
static const uint8_t *s_pending_write = NULL;
static uint32_t s_pending_write_len = 0;
static bool s_write_pending = false;
static portMUX_TYPE s_pending_write_mux = portMUX_INITIALIZER_UNLOCKED;

/* Demo payloads appended to the first object in steps 4 and 6 */
static const uint8_t s_append_1[] = "-Append-line-A";
static const uint8_t s_append_2[] = "-Append-line-B";

/** Post a message to the demo flow task queue (called from event handler). */
static void otp_demo_send_msg(otp_demo_msg_type_t msg, uint8_t oacp_op, uint8_t oacp_rsp)
{
    otp_demo_msg_t m = { .msg = msg, .oacp_op = oacp_op, .oacp_rsp = oacp_rsp };
    if (s_otp_demo_queue) {
        xQueueSend(s_otp_demo_queue, &m, pdMS_TO_TICKS(100));
    }
}

/** Scan callback: look for advertiser with OTS UUID; on match stop scan and connect */
static bool app_ble_conn_scan_cb(const esp_ble_conn_scan_result_t *result, void *arg)
{
    (void)arg;
    if (s_target_found || !result) {
        return false;
    }

    const uint8_t *uuid_ptr = NULL;
    uint8_t uuid_len = 0;
    bool uuid_matched = false;

    ESP_LOGI(TAG, "scan result: addr=" BLE_CONN_MGR_ADDR_STR " type=%u rssi=%d adv_len=%u",
             BLE_CONN_MGR_ADDR_HEX(result->addr), result->addr_type,
             result->rssi, result->adv_data_len);

    if (esp_ble_conn_parse_adv_data(result->adv_data, result->adv_data_len,
                                    ESP_BLE_CONN_ADV_TYPE_UUID16_COMPLETE,
                                    &uuid_ptr, &uuid_len) == ESP_OK ||
            esp_ble_conn_parse_adv_data(result->adv_data, result->adv_data_len,
                                        ESP_BLE_CONN_ADV_TYPE_UUID16_INCOMP,
                                        &uuid_ptr, &uuid_len) == ESP_OK) {
        for (uint8_t i = 0; i + 1 < uuid_len; i += 2) {
            uint16_t uuid16 = (uint16_t)uuid_ptr[i] | ((uint16_t)uuid_ptr[i + 1] << 8);
            if (uuid16 == BLE_OTS_UUID16) {
                uuid_matched = true;
                break;
            }
        }
    }

    if (uuid_matched) {
        ESP_LOGI(TAG, "Matched OTS UUID, stop scan and connect");
        s_target_found = true;
        esp_ble_conn_scan_stop();
        return true;
    }

    return false;
}

/**
 * OTP event handler: translate BLE_OTP_EVENTS into demo queue messages so the
 * demo flow task can wait for OTS discovered, OLCP First result, OACP response,
 * or transfer complete/error. For append steps, on TRANSFER_CHANNEL_CONNECTED we
 * send s_pending_write and then disconnect the channel.
 */
static void app_ble_otp_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_OTP_EVENTS) {
        return;
    }

    esp_ble_otp_event_data_t *evt = (esp_ble_otp_event_data_t *)event_data;

    switch (id) {
    case BLE_OTP_EVENT_OTS_DISCOVERED:
        ESP_LOGI(TAG, "OTS discovered");
        otp_demo_send_msg(MSG_OTS_DISCOVERED, 0, 0);
        break;

    case BLE_OTP_EVENT_OLCP_RESPONSE:
        if (evt) {
            uint8_t op = evt->olcp_response.response.req_op_code;
            uint8_t rsp = evt->olcp_response.response.rsp_code;
            ESP_LOGI(TAG, "OLCP rsp op=0x%02x code=0x%02x", op, rsp);
            if (op == BLE_OTS_OLCP_FIRST) {
                if (rsp == BLE_OTS_OLCP_RSP_SUCCESS) {
                    otp_demo_send_msg(MSG_OLCP_FIRST_OK, 0, 0);
                } else {
                    otp_demo_send_msg(MSG_OLCP_FIRST_FAIL, 0, 0);
                }
            }
        }
        break;

    case BLE_OTP_EVENT_OACP_RESPONSE:
        if (evt) {
            uint8_t op = evt->oacp_response.response.req_op_code;
            uint8_t rsp = evt->oacp_response.response.rsp_code;
            ESP_LOGI(TAG, "OACP rsp op=0x%02x code=0x%02x", op, rsp);
            otp_demo_send_msg(MSG_OACP_RESPONSE, op, rsp);
        }
        break;

    case BLE_OTP_EVENT_TRANSFER_CHANNEL_CONNECTED: {
        ESP_LOGI(TAG, "Transfer channel connected");
        /* Append flow: read pending write under lock, then send and close channel */
        const uint8_t *ptr = NULL;
        uint32_t len = 0;
        bool pending = false;
        portENTER_CRITICAL(&s_pending_write_mux);
        pending = s_write_pending;
        if (pending && s_pending_write && s_pending_write_len > 0) {
            ptr = s_pending_write;
            len = s_pending_write_len;
        }
        portEXIT_CRITICAL(&s_pending_write_mux);
        if (pending && ptr && len > 0 && evt) {
            esp_ble_otp_client_send_data(&evt->transfer_channel_connected.transfer_info,
                                         ptr, len);
            esp_ble_otp_client_disconnect_transfer_channel(
                &evt->transfer_channel_connected.transfer_info);
        }
        break;
    }

    case BLE_OTP_EVENT_TRANSFER_DATA_RECEIVED: {
        if (!evt) {
            break;
        }
        uint16_t data_len = evt->transfer_data_received.data_len;
        uint16_t total_len = evt->transfer_data_received.total_len;
        if (data_len > 0) {
            char preview[65];
            size_t n = (data_len < sizeof(preview) - 1) ? data_len : sizeof(preview) - 1;
            memcpy(preview, evt->transfer_data_received.data, n);
            preview[n] = '\0';
            ESP_LOGI(TAG, "Received %u/%u bytes: %s%s", data_len, total_len, preview, (n < data_len) ? "..." : "");
        }
        break;
    }

    case BLE_OTP_EVENT_TRANSFER_EOF:
        ESP_LOGI(TAG, "Transfer EOF");
        break;

    case BLE_OTP_EVENT_TRANSFER_COMPLETE: {
        ESP_LOGI(TAG, "Transfer complete");
        portENTER_CRITICAL(&s_pending_write_mux);
        if (s_write_pending && s_pending_write_len > 0) {
            s_expected_size += s_pending_write_len;  /* Next append starts after this */
            s_write_pending = false;
        }
        portEXIT_CRITICAL(&s_pending_write_mux);
        otp_demo_send_msg(MSG_TRANSFER_COMPLETE, 0, 0);
        break;
    }

    case BLE_OTP_EVENT_TRANSFER_CHANNEL_DISCONNECTED:
        ESP_LOGI(TAG, "Transfer channel disconnected");
        break;

    case BLE_OTP_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "Transfer error");
        portENTER_CRITICAL(&s_pending_write_mux);
        s_write_pending = false;
        portEXIT_CRITICAL(&s_pending_write_mux);
        otp_demo_send_msg(MSG_TRANSFER_ERROR, 0, 0);
        break;

    default:
        break;
    }
}

/** Connection manager events: track conn_handle and reset on disconnect */
static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    (void)event_data;
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        if (esp_ble_conn_get_conn_handle(&s_conn_handle) == ESP_OK) {
            ESP_LOGI(TAG, "Connected, conn_id=%u", s_conn_handle);
            s_expected_size = 0;
        }
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        s_conn_handle = 0;
        s_target_found = false;
        break;
    default:
        break;
    }
}

/** Block until a message is received or timeout; returns false on timeout */
static bool otp_demo_wait_msg(otp_demo_msg_t *out, uint32_t timeout_ms)
{
    if (!s_otp_demo_queue || !out) {
        return false;
    }
    return xQueueReceive(s_otp_demo_queue, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

/** Empty the demo queue (e.g. before append to drop stale TRANSFER_COMPLETE from previous read) */
static void otp_demo_drain_queue(void)
{
    otp_demo_msg_t m;
    while (s_otp_demo_queue && xQueueReceive(s_otp_demo_queue, &m, 0) == pdTRUE) {
        /* drop */
    }
}

/** Wait for TRANSFER_COMPLETE or TRANSFER_ERROR only; ignore OACP/OLCP. true = complete, false = error/timeout */
static bool otp_demo_wait_transfer_complete_or_error(uint32_t timeout_ms)
{
    otp_demo_msg_t m;
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        if (!otp_demo_wait_msg(&m, 500)) {
            continue;
        }
        if (m.msg == MSG_TRANSFER_COMPLETE) {
            return true;
        }
        if (m.msg == MSG_TRANSFER_ERROR) {
            return false;
        }
        /* ignore MSG_OACP_RESPONSE and others, keep waiting */
    }
    return false;
}

/**
 * OTP demo flow task: runs the 9-step demo sequence. Each step waits for the
 * appropriate demo queue message(s) before calling the next OTP API. Step
 * completion is driven by BLE_OTP_EVENTS (handled in app_ble_otp_event_handler).
 */
static void otp_demo_flow_task(void *arg)
{
    (void)arg;
    otp_demo_msg_t m;
    uint32_t step = 0;

    ESP_LOGI(TAG, "OTP demo flow task started, waiting for OTS discovered");

    for (;;) {
        if (!s_conn_handle) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        switch (step) {
        case 0:
            /* Wait for OTS discovered (auto-discover on connect) */
            if (!otp_demo_wait_msg(&m, UINT32_MAX) || m.msg != MSG_OTS_DISCOVERED) {
                continue;
            }
            step = 1;
            break;

        case 1:
            /* Step 1: read OTS feature; Step 2: select first object */
        {
            esp_ble_ots_feature_t feature = {0};
            if (esp_ble_otp_client_read_feature(s_conn_handle, &feature) == ESP_OK) {
                ESP_LOGI(TAG, "Step 1: Feature read_op=%u write_op=%u execute_op=%u appending=%u",
                         feature.oacp.read_op, feature.oacp.write_op, feature.oacp.execute_op,
                         feature.oacp.appending_op);
            }
        }
            /* 2. select_first */
        esp_ble_otp_client_select_first(s_conn_handle);
        if (!otp_demo_wait_msg(&m, 10000) || m.msg != MSG_OLCP_FIRST_OK) {
            ESP_LOGW(TAG, "Step 2: select_first failed or timeout");
            step = 0;
            continue;
        }
        step = 2;
        break;

        case 2:
            /* Step 3: read object info (sets metadata valid), then full read and wait for transfer complete */
        {
            esp_ble_otp_object_info_t info = {0};
            if (esp_ble_otp_client_read_object_info(s_conn_handle, &info) == ESP_OK) {
                char name_buf[BLE_OTS_ATT_VAL_LEN + 1] = {0};
                size_t nlen = BLE_OTS_ATT_VAL_LEN;
                if (info.object_name[0] != 0) {
                    while (nlen > 0 && info.object_name[nlen - 1] == 0) {
                        nlen--;
                    }
                    memcpy(name_buf, info.object_name, nlen);
                }
                s_expected_size = info.object_size.current_size;
                ESP_LOGI(TAG, "Step 3: Object name=%s size=%lu", name_buf, (unsigned long)s_expected_size);
            }
            esp_ble_otp_client_read_object(s_conn_handle, 0, 0);
        }
        if (!otp_demo_wait_transfer_complete_or_error(15000)) {
            ESP_LOGW(TAG, "Step 3: read object timeout or error");
            step = 0;
            continue;
        }
        ESP_LOGI(TAG, "Step 3 done: first object received and printed above");
        step = 3;
        break;

        case 3:
            /* Step 4: append first string; wait for TRANSFER_COMPLETE (not OACP response) */
            otp_demo_drain_queue();
            {
                uint32_t append_len = (uint32_t)strlen((const char *)s_append_1);
                portENTER_CRITICAL(&s_pending_write_mux);
                s_pending_write = s_append_1;
                s_pending_write_len = append_len;
                s_write_pending = true;
                portEXIT_CRITICAL(&s_pending_write_mux);
                esp_ble_otp_client_write_object(s_conn_handle, s_expected_size, append_len, BLE_OTP_WRITE_MODE_APPEND);
            }
            {
                uint32_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(15000);
                bool done = false;
                bool success = false;
                while (xTaskGetTickCount() < deadline) {
                    if (!otp_demo_wait_msg(&m, 500)) {
                        continue;
                    }
                    if (m.msg == MSG_TRANSFER_COMPLETE) {
                        done = true;
                        success = true;
                        break;
                    }
                    if (m.msg == MSG_TRANSFER_ERROR) {
                        done = true;
                        success = false;
                        break;
                    }
                    /* ignore MSG_OACP_RESPONSE (Write 0x06) and others, keep waiting */
                }
                if (!done || !success) {
                    ESP_LOGW(TAG, "Step 4: append 1 failed or timeout");
                    portENTER_CRITICAL(&s_pending_write_mux);
                    s_write_pending = false;
                    portEXIT_CRITICAL(&s_pending_write_mux);
                    step = 0;
                    continue;
                }
            }
            ESP_LOGI(TAG, "Step 4 done: appended first string");
            step = 4;
            break;

        case 4:
            /* Step 5: full read after first append */
            esp_ble_otp_client_read_object(s_conn_handle, 0, 0);
            if (!otp_demo_wait_transfer_complete_or_error(15000)) {
                ESP_LOGW(TAG, "Step 5: read timeout or error");
                step = 0;
                continue;
            }
            ESP_LOGI(TAG, "Step 5 done: full read after first append");
            step = 5;
            break;

        case 5:
            /* Step 6: append second string */
            otp_demo_drain_queue();
            {
                uint32_t append_len = (uint32_t)strlen((const char *)s_append_2);
                portENTER_CRITICAL(&s_pending_write_mux);
                s_pending_write = s_append_2;
                s_pending_write_len = append_len;
                s_write_pending = true;
                portEXIT_CRITICAL(&s_pending_write_mux);
                esp_ble_otp_client_write_object(s_conn_handle, s_expected_size, append_len, BLE_OTP_WRITE_MODE_APPEND);
            }
            {
                uint32_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(15000);
                bool done = false;
                bool success = false;
                while (xTaskGetTickCount() < deadline) {
                    if (!otp_demo_wait_msg(&m, 500)) {
                        continue;
                    }
                    if (m.msg == MSG_TRANSFER_COMPLETE) {
                        done = true;
                        success = true;
                        break;
                    }
                    if (m.msg == MSG_TRANSFER_ERROR) {
                        done = true;
                        success = false;
                        break;
                    }
                    /* ignore MSG_OACP_RESPONSE and others, keep waiting */
                }
                if (!done || !success) {
                    ESP_LOGW(TAG, "Step 6: append 2 failed or timeout");
                    portENTER_CRITICAL(&s_pending_write_mux);
                    s_write_pending = false;
                    portEXIT_CRITICAL(&s_pending_write_mux);
                    step = 0;
                    continue;
                }
            }
            ESP_LOGI(TAG, "Step 6 done: appended second string");
            step = 6;
            break;

        case 6:
            /* Step 7: full read after second append */
            esp_ble_otp_client_read_object(s_conn_handle, 0, 0);
            if (!otp_demo_wait_transfer_complete_or_error(15000)) {
                ESP_LOGW(TAG, "Step 7: read timeout or error");
                step = 0;
                continue;
            }
            ESP_LOGI(TAG, "Step 7 done: full read after second append");
            step = 7;
            break;

        case 7:
            /* Step 8: delete current object; wait for OACP response with op=DELETE */
            otp_demo_drain_queue();
            esp_ble_otp_client_delete_object(s_conn_handle);
            {
                uint32_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(10000);
                bool got_delete = false;
                while (xTaskGetTickCount() < deadline) {
                    if (!otp_demo_wait_msg(&m, 500)) {
                        continue;
                    }
                    if (m.msg == MSG_OACP_RESPONSE && m.oacp_op == BLE_OTS_OACP_DELETE) {
                        got_delete = true;
                        break;
                    }
                }
                if (!got_delete) {
                    ESP_LOGW(TAG, "Step 8: delete timeout");
                    step = 0;
                    continue;
                }
                if (m.oacp_rsp != BLE_OTS_OACP_RSP_SUCCESS) {
                    ESP_LOGW(TAG, "Step 8: delete failed op=0x%02x rsp=0x%02x", m.oacp_op, m.oacp_rsp);
                    step = 0;
                    continue;
                }
            }
            ESP_LOGI(TAG, "Step 8 done: object deleted");
            step = 8;
            break;

        case 8:
            /* Step 9: select first again; if no object (all deleted) stop; else read_object_info + read */
            esp_ble_otp_client_select_first(s_conn_handle);
            if (!otp_demo_wait_msg(&m, 5000)) {
                ESP_LOGW(TAG, "Step 9: select_first timeout");
                step = 0;
                continue;
            }
            if (m.msg == MSG_OLCP_FIRST_FAIL) {
                ESP_LOGI(TAG, "Step 9: no object (expected if all deleted)");
                step = 0;
                continue;
            }
            /* After OLCP First success metadata is UNKNOWN; read_object_info sets VALID for read_object */
            {
                esp_ble_otp_object_info_t info = {0};
                if (esp_ble_otp_client_read_object_info(s_conn_handle, &info) != ESP_OK) {
                    ESP_LOGW(TAG, "Step 9: read_object_info failed");
                    step = 0;
                    continue;
                }
                esp_ble_otp_client_read_object(s_conn_handle, 0, 0);
            }
            if (!otp_demo_wait_transfer_complete_or_error(10000)) {
                ESP_LOGI(TAG, "Step 9: read after delete timeout or error");
            } else {
                ESP_LOGI(TAG, "Step 9: read after delete completed");
            }
            step = 0;
            break;

        default:
            step = 0;
            break;
        }
    }
}

void app_main(void)
{
    /* BLE connection config: scan for OTS server and connect */
    esp_ble_conn_config_t config = {
        .device_name = CONFIG_EXAMPLE_BLE_DEVICE_NAME,
        .broadcast_data = CONFIG_EXAMPLE_BLE_SUB_ADV,
    };

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_otp_event_handler, NULL));

    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_l2cap_coc_mem_init());
    ESP_ERROR_CHECK(esp_ble_conn_register_scan_callback(app_ble_conn_scan_cb, NULL));

    esp_ble_otp_config_t otp_cfg = {
        .role = BLE_OTP_ROLE_CLIENT,
        .psm = CONFIG_EXAMPLE_OTP_PSM,
        .l2cap_coc_mtu = CONFIG_BLE_CONN_MGR_L2CAP_COC_MTU,
        .auto_discover_ots = true,
    };
    ESP_ERROR_CHECK(esp_ble_otp_init(&otp_cfg));

    /* Demo flow task: runs 9-step demo; receives messages from OTP event handler */
    static TaskHandle_t s_demo_task_handle = NULL;
    s_otp_demo_queue = xQueueCreate(OTP_DEMO_QUEUE_LEN, sizeof(otp_demo_msg_t));
    if (s_otp_demo_queue) {
        if (xTaskCreate(otp_demo_flow_task, "otp_demo_flow", OTP_DEMO_STACK, NULL, 5, &s_demo_task_handle) != pdPASS) {
            ESP_LOGE(TAG, "Failed to create demo flow task");
            vQueueDelete(s_otp_demo_queue);
            s_otp_demo_queue = NULL;
        }
    } else {
        ESP_LOGE(TAG, "Failed to create demo queue");
    }

    ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager: %s", esp_err_to_name(ret));
        if (s_demo_task_handle) {
            vTaskDelete(s_demo_task_handle);
            s_demo_task_handle = NULL;
        }
        if (s_otp_demo_queue) {
            vQueueDelete(s_otp_demo_queue);
            s_otp_demo_queue = NULL;
        }
        esp_ble_otp_deinit();
        esp_ble_conn_l2cap_coc_mem_release();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
        esp_event_handler_unregister(BLE_OTP_EVENTS, ESP_EVENT_ANY_ID, app_ble_otp_event_handler);
        esp_event_loop_delete_default();
    }
}
