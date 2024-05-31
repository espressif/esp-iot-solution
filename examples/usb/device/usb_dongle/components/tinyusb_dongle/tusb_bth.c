/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_bt.h"

#include "tinyusb.h"
#include "tusb_bth.h"

static const char *TAG = "tusb_bth";

static SemaphoreHandle_t evt_sem = NULL;
static SemaphoreHandle_t acl_sem = NULL;

static acl_data_t acl_tx_data = {
    .is_new_pkt = true,
    .pkt_total_len = 0,
    .pkt_cur_offset = 0,
    .pkt_val = NULL
};

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void controller_rcv_pkt_ready(void)
{
    // Do nothing, will check by esp_vhci_host_check_send_available().
}

/*
 * @brief: BT controller callback function, to transfer data packet to upper
 *         controller is ready to receive command
 */
static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
    uint16_t act_len = len - 1;
    uint8_t type = data[0];
    uint8_t * acl_rx_buf = NULL;
    uint8_t * evt_rx_buf = NULL;

    HCI_DUMP_BUFFER("Recv Pkt", data, len);

    switch (type) {
    case HCIT_TYPE_COMMAND:
        break;
    case HCIT_TYPE_ACL_DATA:
        acl_rx_buf = (uint8_t *)malloc(act_len * sizeof(uint8_t));
        assert(acl_rx_buf);

        memcpy(acl_rx_buf, data + 1, act_len);
        tud_bt_acl_data_send(acl_rx_buf, act_len);

        xSemaphoreTake(acl_sem, portMAX_DELAY);
        free(acl_rx_buf);
        break;
    case HCIT_TYPE_SCO_DATA:
        break;
    case HCIT_TYPE_EVENT:
        evt_rx_buf = (uint8_t *)malloc(act_len * sizeof(uint8_t));
        assert(evt_rx_buf);

        memcpy(evt_rx_buf, data + 1, act_len);
        tud_bt_event_send(evt_rx_buf, act_len);

        xSemaphoreTake(evt_sem, portMAX_DELAY);
        free(evt_rx_buf);
        break;
    default:
        ESP_LOGE(TAG, "Unknown type [0x%02x]", type);
        break;
    }

    return 0;
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+

// Invoked when HCI command was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.1.
// Length of the command is from 3 bytes (2 bytes for OpCode,
// 1 byte for parameter total length) to 258.
void tud_bt_hci_cmd_cb(void * hci_cmd, size_t cmd_len)
{
    uint8_t * cmd_tx_buf = (uint8_t *)malloc((cmd_len + 1) * sizeof(uint8_t));

    assert(cmd_tx_buf);

    cmd_tx_buf[0] = HCIT_TYPE_COMMAND;
    memcpy(cmd_tx_buf + 1, hci_cmd, cmd_len);

    HCI_DUMP_BUFFER("Transmit Pkt", cmd_tx_buf, cmd_len + 1);

    while (!esp_vhci_host_check_send_available()) {
        vTaskDelay(1);
    }
    esp_vhci_host_send_packet(cmd_tx_buf, cmd_len + 1);

    free(cmd_tx_buf);
    cmd_tx_buf = NULL;
}

// Invoked when ACL data was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.2.
// Length is from 4 bytes, (12 bits for Handle, 4 bits for flags
// and 16 bits for data total length) to endpoint size.
void tud_bt_acl_data_received_cb(void *acl_data, uint16_t data_len)
{
    if (acl_tx_data.is_new_pkt) {
        acl_tx_data.pkt_total_len = *(((uint16_t *)acl_data) + 1) + 4;
        acl_tx_data.pkt_cur_offset = 0;

        acl_tx_data.pkt_val = (uint8_t *)malloc((acl_tx_data.pkt_total_len + 1) * sizeof(uint8_t));
        assert(acl_tx_data.pkt_val);
        memset(acl_tx_data.pkt_val, 0x0, (acl_tx_data.pkt_total_len + 1));

        acl_tx_data.pkt_val[0] = HCIT_TYPE_ACL_DATA;
        acl_tx_data.pkt_cur_offset++;
        memcpy(acl_tx_data.pkt_val + acl_tx_data.pkt_cur_offset, acl_data, data_len);
        acl_tx_data.pkt_cur_offset += data_len;

        if (data_len < acl_tx_data.pkt_total_len) {
            acl_tx_data.is_new_pkt = false;
            return;
        }

        while (!esp_vhci_host_check_send_available()) {
            vTaskDelay(1);
        }

        HCI_DUMP_BUFFER("Transmit Pkt", acl_tx_data.pkt_val, data_len + 1);
        esp_vhci_host_send_packet(acl_tx_data.pkt_val, data_len + 1);
        goto reset_params;
    } else {
        memcpy(acl_tx_data.pkt_val + acl_tx_data.pkt_cur_offset, acl_data, data_len);
        acl_tx_data.pkt_cur_offset += data_len;

        if ((acl_tx_data.pkt_cur_offset - 1) == acl_tx_data.pkt_total_len) {
            while (!esp_vhci_host_check_send_available()) {
                vTaskDelay(1);
            }

            HCI_DUMP_BUFFER("Transmit Pkt", acl_tx_data.pkt_val, acl_tx_data.pkt_total_len + 1);
            esp_vhci_host_send_packet(acl_tx_data.pkt_val, acl_tx_data.pkt_total_len + 1);
            goto reset_params;
        }
    }

    return;

reset_params:

    acl_tx_data.is_new_pkt = true;
    acl_tx_data.pkt_total_len = 0;
    acl_tx_data.pkt_cur_offset = 0;

    if (acl_tx_data.pkt_val) {
        free(acl_tx_data.pkt_val);
        acl_tx_data.pkt_val = NULL;
    }

    return;
}

// Called when event sent with tud_bt_event_send() was delivered to BT stack.
// Controller can release/reuse buffer with Event packet at this point.
void tud_bt_event_sent_cb(uint16_t sent_bytes)
{
    if (evt_sem) {
        xSemaphoreGive(evt_sem);
    }
}

// Called when ACL data that was sent with tud_bt_acl_data_send()
// was delivered to BT stack.
// Controller can release/reuse buffer with ACL packet at this point.
void tud_bt_acl_data_sent_cb(uint16_t sent_bytes)
{
    if (acl_sem) {
        xSemaphoreGive(acl_sem);
    }
}

static esp_vhci_host_callback_t vhci_host_cb = {
    controller_rcv_pkt_ready,
    host_rcv_pkt
};

/**
 * @brief Initialize BTH Device.
 */
void tusb_bth_init(void)
{
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if (esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller release classic bt memory failed.");
        return;
    }

    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller initialize failed.");
        return;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller enable failed.");
        return;
    }

    evt_sem = xSemaphoreCreateBinary();
    assert(evt_sem);
    acl_sem = xSemaphoreCreateBinary();
    assert(acl_sem);

    acl_tx_data.is_new_pkt = true;
    acl_tx_data.pkt_total_len = 0;
    acl_tx_data.pkt_cur_offset = 0;
    acl_tx_data.pkt_val = NULL;

    esp_vhci_host_register_callback(&vhci_host_cb);
}

/**
 * @brief Deinitialization BTH Device.
 */
void tusb_bth_deinit(void)
{
    if (esp_bt_controller_disable() != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller disable failed.");
        return;
    }

    if (esp_bt_controller_deinit() != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller deinit failed.");
        return;
    }

    if (evt_sem) {
        vSemaphoreDelete(evt_sem);
        evt_sem = NULL;
    }

    if (acl_sem) {
        vSemaphoreDelete(acl_sem);
        acl_sem = NULL;
    }

    acl_tx_data.is_new_pkt = true;
    acl_tx_data.pkt_total_len = 0;
    acl_tx_data.pkt_cur_offset = 0;

    if (acl_tx_data.pkt_val) {
        free(acl_tx_data.pkt_val);
        acl_tx_data.pkt_val = NULL;
    }
}
