/**
 * @copyright Copyright 2021 Espressif Systems (Shanghai) Co. Ltd.
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *               http://www.apache.org/licenses/LICENSE-2.0
 * 
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_bt.h"

#include "tinyusb.h"
#include "tusb_bth.h"

#define BUFFER_SIZE_MAX  256
#define LE_READ_BUFF_SIZE                  0x2002
#define HCI_H4_CMD_PREAMBLE_SIZE           (4)
#define UINT16_TO_STREAM(p, u16) {*(p)++ = (uint8_t)(u16); *(p)++ = (uint8_t)((u16) >> 8);}
#define UINT8_TO_STREAM(p, u8)   {*(p)++ = (uint8_t)(u8);}

static const char *TAG = "tusb_bth";
static uint16_t acl_buf_size_max = 0;
uint8_t * p_acl_buf = NULL;

void ble_controller_init(void) {
    esp_err_t ret;
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if ((ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGI(TAG, "Bluetooth controller release classic bt memory failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGI(TAG, "Bluetooth controller initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) {
        ESP_LOGI(TAG, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }
}

/*
 * @brief: BT controller callback function, used to notify the upper layer that
 *         controller is ready to receive command
 */
static void controller_rcv_pkt_ready(void)
{
    //TODO
}

/*
 * @brief: BT controller callback function, to transfer data packet to upper
 *         controller is ready to receive command
 */
static int host_rcv_pkt(uint8_t *data, uint16_t len)
{
    uint16_t act_len = len - 1;

    if(data[0] == HCIT_TYPE_EVENT) { // event data from controller
        uint8_t *hci_buf = (uint8_t *)malloc(len);
        memcpy(hci_buf, data +1 , act_len);
        ESP_LOGI(TAG, "evt_data from controller, evt_data_length: %d :", act_len);
        tud_bt_event_send(hci_buf, act_len);
        free(hci_buf);
    } else if(data[0] == HCIT_TYPE_ACL_DATA) { // acl data from controller
        uint8_t *hci_acl_buf_contr = (uint8_t *)malloc(len);
        memcpy(hci_acl_buf_contr, data +1 , act_len);
        ESP_LOGI(TAG, "acl_data from controller, acl_data_length: %d :", act_len);
        tud_bt_acl_data_send(hci_acl_buf_contr, act_len);
        free(hci_acl_buf_contr);
    }
    return 0;
}

static esp_vhci_host_callback_t vhci_host_cb = {
    controller_rcv_pkt_ready,
    host_rcv_pkt
};

static int host_rcv_pkt_test (uint8_t *data, uint16_t len) {
    
    ESP_LOGI(TAG, "host_rcv_pkt_test evt_data_length: %d :", len);
    if(data[1] == 0x0e && data[2] == 0x07 
        && data[4] == 0x02 && data[5] == 0x20) {
        // LE Read Buffer size command complete event
        uint16_t* size_p = NULL;
        uint16_t cmd_value;
        size_p = (uint8_t*)(&cmd_value);
        *size_p = data[7];
        *(size_p+1) = data[8];
        acl_buf_size_max = *size_p;
        ESP_LOGI(TAG, "acl_buf_size_max: %d", acl_buf_size_max);
    }
    if (!acl_buf_size_max) {
        acl_buf_size_max = BUFFER_SIZE_MAX;
    }

    p_acl_buf = (uint8_t *)malloc(acl_buf_size_max + 1);
    esp_vhci_host_register_callback(&vhci_host_cb);
    return 0;
}

uint16_t make_cmd_le_read_buff_size(uint8_t *buf)
{
    UINT8_TO_STREAM (buf, HCIT_TYPE_COMMAND);
    UINT16_TO_STREAM (buf, LE_READ_BUFF_SIZE);
    UINT8_TO_STREAM (buf, 0);
    return HCI_H4_CMD_PREAMBLE_SIZE;
}

static esp_vhci_host_callback_t vhci_host_cb_test = {
    controller_rcv_pkt_ready,
    host_rcv_pkt_test
};

void tusb_bth_init(void)
{
    ble_controller_init();
    // register vhci_host_cb_test, test le read buffer size
    esp_vhci_host_register_callback(&vhci_host_cb_test);
    uint8_t buf[6];
    uint16_t sz = make_cmd_le_read_buff_size(buf);
    esp_vhci_host_send_packet(buf, sz);
}

//--------------------------------------------------------------------+
// tinyusb callbacks
//--------------------------------------------------------------------+

// Invoked when HCI command was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.1.
// Length of the command is from 3 bytes (2 bytes for OpCode,
// 1 byte for parameter total length) to 258.
void tud_bt_hci_cmd_cb(void *hci_cmd, size_t cmd_len)
{
    uint8_t *hci_cmd_buf = (uint8_t *)malloc(cmd_len + 1);

    hci_cmd_buf[0] = HCIT_TYPE_COMMAND;
    memcpy(hci_cmd_buf+1, hci_cmd, cmd_len);
    esp_vhci_host_send_packet(hci_cmd_buf, cmd_len +1);

    free(hci_cmd_buf);
}

// Invoked when ACL data was received over USB from Bluetooth host.
// Detailed format is described in Bluetooth core specification Vol 2,
// Part E, 5.4.2.
// Length is from 4 bytes, (12 bits for Handle, 4 bits for flags
// and 16 bits for data total length) to endpoint size.
static bool prepare_write = false;
static uint16_t write_offset = 0;
static uint16_t acl_data_length = 0;
void tud_bt_acl_data_received_cb(void *acl_data, uint16_t data_len)
{

    // if acl_data is long data
    if(!prepare_write) {
        // first get acl_data_length
        acl_data_length = *(((uint16_t * )acl_data) + 1);
        if(acl_data_length > data_len) {
            prepare_write = true;
            p_acl_buf[0] = HCIT_TYPE_ACL_DATA;
            memcpy(p_acl_buf + 1, acl_data, data_len);
            write_offset = data_len + 1;
        } else {
            p_acl_buf[0] = HCIT_TYPE_ACL_DATA;
            memcpy(p_acl_buf + 1, acl_data, data_len);
            ESP_LOGI(TAG, "short acl_data from host, will send to controller, length: %d", (data_len + 1));
            esp_vhci_host_send_packet(p_acl_buf, data_len + 1);
        }
    } else {
        memcpy(p_acl_buf + write_offset, acl_data, data_len);
        write_offset += data_len;
        if(acl_data_length > write_offset) {
            ESP_LOGI(TAG, "Remaining bytes: %d", (acl_data_length - write_offset));
            return;
        }
        ESP_LOGI(TAG, "long acl_data from host, will send to controller, length: %d", (data_len + 1));
        prepare_write = false;
        esp_vhci_host_send_packet(p_acl_buf, data_len + write_offset);
    }
    //free(hci_data_buf);
}

// Called when event sent with tud_bt_event_send() was delivered to BT stack.
// Controller can release/reuse buffer with Event packet at this point.
void tud_bt_event_sent_cb(uint16_t sent_bytes)
{
    //TODO
}

// Called when ACL data that was sent with tud_bt_acl_data_send()
// was delivered to BT stack.
// Controller can release/reuse buffer with ACL packet at this point.
void tud_bt_acl_data_sent_cb(uint16_t sent_bytes)
{
    //TODO
}