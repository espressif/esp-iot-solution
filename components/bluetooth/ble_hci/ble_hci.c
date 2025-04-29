/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ble_hci.h"
#include "bt_hci_common.h"

static const char *TAG = "ble_hci";
#define SCAN_RESULT_LEN_MAX  25
#define CMD_WAIT_TIME (100/portTICK_PERIOD_MS)

typedef struct {
    TaskHandle_t ble_hci_task_handle;
    QueueHandle_t hci_data_queue;
    QueueHandle_t hci_cmd_evt_queue;
    uint8_t scan_result_len;
    ble_hci_scan_result_t scan_result[SCAN_RESULT_LEN_MAX];
    ble_hci_scan_cb_t cb;
    uint8_t cmd_buf[128];
} ble_hci_t;

ble_hci_t *s_ble_hci = NULL;

typedef struct {
    uint8_t *data;
    uint16_t data_len;
} host_rcv_data_t;

typedef struct {
    uint8_t opcode;
    uint8_t reason;
} hci_cmd_event_t;

static void controller_rcv_pkt_ready(void)
{
    ESP_LOGI(TAG, "controller rcv pkt ready");
}

static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
    /* Check second byte for HCI event. If event opcode is 0x0e, the event is
     * HCI Command Complete event. Sice we have received "0x0e" event, we can
     * check for byte 4 for command opcode and byte 6 for it's return status. */
    host_rcv_data_t send_data;
    send_data.data_len = len;
    send_data.data = malloc(sizeof(uint8_t) * len);
    memcpy(send_data.data, data, len);
    if (xQueueSend(s_ble_hci->hci_data_queue, (void *)&send_data, (TickType_t) 0) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to enqueue advertising report. Queue full.");
        /* If data sent successfully, then free the pointer in `xQueueReceive'
         * after processing it. Or else if enqueue in not successful, free it
         * here. */
        free(send_data.data);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_vhci_host_callback_t vhci_host_cb = {
    controller_rcv_pkt_ready,
    host_rcv_pkt
};

static void hci_evt_process(void *pvParameters)
{
    host_rcv_data_t rcv_data;
    uint8_t *queue_data;
    uint16_t data_ptr, total_data_len;
    uint8_t hci_event_opcode, sub_event, num_responses;
    while (1) {
        total_data_len = 0;
        data_ptr = 0;
        if (xQueueReceive(s_ble_hci->hci_data_queue, &rcv_data, portMAX_DELAY) == pdPASS) {
            queue_data = rcv_data.data;
            hci_event_opcode = queue_data[++data_ptr];
            if (hci_event_opcode == LE_META_EVENTS) {
                /* Set `data_ptr' to 4th entry, which will point to sub event. */
                data_ptr += 2;
                sub_event = queue_data[data_ptr++];
                /* Check if sub event is LE advertising report event. */
                if (sub_event == HCI_LE_ADV_REPORT) {
                    /* Get number of advertising reports. */
                    num_responses = queue_data[data_ptr++];
                    s_ble_hci->scan_result_len = num_responses;
                    for (uint8_t i = 0; i < num_responses; i += 1) {
                        s_ble_hci->scan_result[i].search_evt = queue_data[data_ptr++];
                    }
                    for (uint8_t i = 0; i < num_responses; i += 1) {
                        s_ble_hci->scan_result[i].ble_addr_type = queue_data[data_ptr++];
                    }
                    for (int i = 0; i < num_responses; i += 1) {
                        for (int j = 0; j < 6; j += 1) {
                            s_ble_hci->scan_result[i].bda[5 - j] = queue_data[data_ptr++];
                        }
                    }
                    for (int i = 0; i < num_responses; i += 1) {
                        s_ble_hci->scan_result[i].adv_data_len = queue_data[data_ptr++];
                        total_data_len += s_ble_hci->scan_result[i].adv_data_len;
                    }
                    if (total_data_len != 0) {
                        for (uint8_t i = 0; i < num_responses; i += 1) {
                            for (uint8_t j = 0; j < s_ble_hci->scan_result[i].adv_data_len; j += 1) {
                                s_ble_hci->scan_result[i].ble_adv[j] = queue_data[data_ptr++];
                            }
                        }
                    }
                    for (int i = 0; i < num_responses; i += 1) {
                        s_ble_hci->scan_result[i].rssi = -(0xFF - queue_data[data_ptr++]);
                    }
                    s_ble_hci->cb(s_ble_hci->scan_result, s_ble_hci->scan_result_len);
                }
            } else if (hci_event_opcode == LE_CMD_COMPLETE_EVENT) {
                hci_cmd_event_t hci_cmd_evt;
                hci_cmd_evt.opcode = queue_data[4];
                hci_cmd_evt.reason = queue_data[6];
                if (queue_data[6] == 0) {
                    ESP_LOGD(TAG, "Event opcode 0x%02x success.", queue_data[4]);
                } else {
                    ESP_LOGE(TAG, "Event opcode 0x%02x fail with reason: 0x%02x.", queue_data[4], queue_data[6]);
                }
                xQueueSend(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, portMAX_DELAY);
            } else {
                ESP_LOGW(TAG, "Unhandled HCI event code: 0x%02x", hci_event_opcode);
            }
            free(rcv_data.data);
        }
    }
}

esp_err_t ble_hci_reset(void)
{
    hci_cmd_event_t hci_cmd_evt;
    uint16_t sz = make_cmd_reset(s_ble_hci->cmd_buf);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_RESET) {
            if (hci_cmd_evt.reason != 0) {
                ESP_LOGE(TAG, "Reset failed with reason: 0x%02x", hci_cmd_evt.reason);
            }
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_enable_meta_event(void)
{
    hci_cmd_event_t hci_cmd_evt;
    /* Set bit 61 in event mask to enable LE Meta events. */
    uint8_t evt_mask[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
    uint16_t sz = make_cmd_set_evt_mask(s_ble_hci->cmd_buf, evt_mask);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_SET_EVT_MASK) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_set_adv_param(ble_hci_adv_param_t *param)
{
    uint16_t sz = make_cmd_ble_set_adv_param(s_ble_hci->cmd_buf,
                                             param->adv_int_min,
                                             param->adv_int_max,
                                             param->adv_type,
                                             param->own_addr_type,
                                             param->peer_addr_type,
                                             param->peer_addr,
                                             param->channel_map,
                                             param->adv_filter_policy);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    hci_cmd_event_t hci_cmd_evt;
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_WRITE_ADV_PARAMS) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_set_adv_data(uint8_t len, uint8_t *data)
{
    uint16_t sz = make_cmd_ble_set_adv_data(s_ble_hci->cmd_buf, len, data);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    hci_cmd_event_t hci_cmd_evt;
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_WRITE_ADV_DATA) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_set_adv_enable(bool enable)
{
    uint16_t sz = make_cmd_ble_set_adv_enable(s_ble_hci->cmd_buf, enable);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    hci_cmd_event_t hci_cmd_evt;
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_WRITE_ADV_ENABLE) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_set_scan_param(ble_hci_scan_param_t *param)
{
    hci_cmd_event_t hci_cmd_evt;
    uint16_t sz = make_cmd_ble_set_scan_params(s_ble_hci->cmd_buf, param->scan_type, param->scan_interval,
                                               param->scan_window, param->own_addr_type, param->filter_policy);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_WRITE_SCAN_PARAM) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_set_scan_enable(bool enable, bool filter_duplicates)
{
    uint16_t sz = make_cmd_ble_set_scan_enable(s_ble_hci->cmd_buf, enable, filter_duplicates);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    hci_cmd_event_t hci_cmd_evt;
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_WRITE_SCAN_ENABLE) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_add_to_accept_list(ble_hci_addr_t addr, ble_hci_addr_type_t addr_type)
{
    uint16_t sz = make_cmd_ble_add_to_filter_accept_list(s_ble_hci->cmd_buf, addr_type, addr);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    hci_cmd_event_t hci_cmd_evt;
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_ADD_TO_ACCEPT_LIST) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_clear_accept_list()
{
    uint16_t sz = make_cmd_ble_clear_white_list(s_ble_hci->cmd_buf);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    hci_cmd_event_t hci_cmd_evt;
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_CLEAR_ACCEPT_LIST) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_set_random_address(ble_hci_addr_t addr)
{
    uint16_t sz = make_cmd_set_random_address(s_ble_hci->cmd_buf, addr);
    esp_vhci_host_send_packet(s_ble_hci->cmd_buf, sz);
    hci_cmd_event_t hci_cmd_evt;
    if (xQueueReceive(s_ble_hci->hci_cmd_evt_queue, &hci_cmd_evt, CMD_WAIT_TIME) == pdTRUE) {
        if (hci_cmd_evt.opcode == HCI_OCF_SET_RANDOM_ADDR) {
            return hci_cmd_evt.reason;
        }
    }
    return ESP_FAIL;
}

esp_err_t ble_hci_set_register_scan_callback(ble_hci_scan_cb_t cb)
{
    if (s_ble_hci == NULL) {
        ESP_LOGE(TAG, "ble_hci not init");
        return ESP_ERR_INVALID_STATE;
    }
    s_ble_hci->cb = cb;
    return ESP_OK;
}

esp_err_t ble_hci_init(void)
{
    if (s_ble_hci != NULL) {
        ESP_LOGW(TAG, "ble_hci already init");
        return ESP_OK;
    }
    s_ble_hci = calloc(1, sizeof(ble_hci_t));
    ESP_RETURN_ON_FALSE(s_ble_hci, ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for s_ble_hci");

    s_ble_hci->hci_data_queue = xQueueCreate(15, sizeof(host_rcv_data_t));
    s_ble_hci->hci_cmd_evt_queue = xQueueCreate(5, sizeof(hci_cmd_event_t));

    esp_bt_controller_config_t config_opts = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_bt_controller_init(&config_opts));

    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    esp_vhci_host_register_callback(&vhci_host_cb);

    BaseType_t res = xTaskCreatePinnedToCore(&hci_evt_process, "hci_evt_process", 2048, NULL, 6, &s_ble_hci->ble_hci_task_handle, 0);
    ESP_RETURN_ON_FALSE(res, ESP_ERR_NO_MEM, TAG, "Failed create hci evt process");

    return ESP_OK;
}

esp_err_t ble_hci_deinit(void)
{
    if (s_ble_hci == NULL) {
        ESP_LOGW(TAG, "ble_hci already deinit");
        return ESP_OK;
    }
    while (eTaskGetState(s_ble_hci->ble_hci_task_handle) == eRunning) {
        vTaskDelay(pdTICKS_TO_MS(10));
    }
    vTaskDelete(s_ble_hci->ble_hci_task_handle);
    ESP_ERROR_CHECK(esp_bt_controller_disable());
    ESP_ERROR_CHECK(esp_bt_controller_deinit());

    vQueueDelete(s_ble_hci->hci_data_queue);
    vQueueDelete(s_ble_hci->hci_cmd_evt_queue);

    free(s_ble_hci);
    s_ble_hci = NULL;
    return ESP_OK;
}
