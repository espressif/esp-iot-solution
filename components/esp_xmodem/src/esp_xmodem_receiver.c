/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include <esp_xmodem_priv.h>
#include <esp_xmodem.h>
#include <esp_xmodem_transport.h>

static const char *TAG = "xmodem_receiver";

#ifdef CONFIG_IDF_TARGET_ESP8266
#define ESP_XMODEM_RECEIVER_TASK_SIZE 2048
#else
#define ESP_XMODEM_RECEIVER_TASK_SIZE CONFIG_ESP_XMODEM_RECEIVER_TASK_STACK_SIZE
#endif
#define ESP_XMODEM_RECEIVER_TASK_PRIO CONFIG_ESP_XMODEM_SENDER_TASK_PRIORITY

static int esp_xmodem_receiver_send_crc(esp_xmodem_t *receiver)
{
    int crc_len = 0;

    if (!receiver) {
        return crc_len;
    }

    esp_xmodem_config_t *p = receiver->config;

    if (p->crc_type == ESP_XMODEM_CHECKSUM) {
        esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_INIT, NAK);
        crc_len = 1;
    } else {
        esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_INIT, CRC16);
        crc_len = 2;
    }

    return crc_len;
}

static esp_err_t esp_xmodem_parse_file_data(esp_xmodem_file_data_t *file_data, uint8_t *data)
{
    int i = 0, j = 0;
    char *file_ptr = file_data->filename;
    char file_size[16] = {0};
    for (i = 0; ((data[i] != 0) && (i < 64)); i++) {
        *file_ptr = data[i];
        file_ptr ++;
    }
    *file_ptr = '\0';

    for (j = 0; ((data[i + j + 1] != ' ') && (j < 16)); j++) {
        file_size[j] = data[i + j + 1];
    }
    file_size[j] = '\0';
    file_data->file_length = strtol(file_size, NULL, 10);
    return ESP_OK;
}

static esp_err_t esp_xmodem_receiver_process_data(esp_xmodem_t *receiver, uint8_t *packet, int crc_len)
{
    if (!receiver || !packet) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xmodem_config_t *p = receiver->config;
    int buffer_size = 0, packet_size = 0;
    esp_err_t ret = ESP_FAIL;
    switch (packet[0]) {
        case SOH:
            buffer_size = XMODEM_DATA_LEN;
            break;
        case STX:
            buffer_size = XMODEM_1K_DATA_LEN;
            break;
        case EOT:
            if (esp_xmodem_get_state(receiver) == XMODEM_STATE_RECEIVER_RECEIVE_EOT) {
                ESP_LOGI(TAG, "Receive EOT data again");
                esp_xmodem_send_char_code_event(receiver, ESP_OK, ESP_XMODEM_EVENT_INIT, ACK);
                esp_xmodem_receiver_send_crc(receiver);
            } else {
                ESP_LOGI(TAG, "Receive EOT data");
                if (!receiver->is_file_data) {
                    esp_xmodem_send_char_code_event(receiver, ESP_OK, ESP_XMODEM_EVENT_FINISHED, ACK);
                    /* Set state to XMODEM_STATE_FINISH */
                    esp_xmodem_set_state(receiver, XMODEM_STATE_FINISH);
                } else {
                    esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_INIT, NAK);
                    /* Set state to XMODEM_STATE_RECEIVER_RECEIVE_EOT */
                    esp_xmodem_set_state(receiver, XMODEM_STATE_RECEIVER_RECEIVE_EOT);
                }
            }
            return ESP_OK;
        default:
            esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_DATA_RECV_ERROR, ESP_XMODEM_EVENT_INIT, NAK);
            ESP_LOGE(TAG, "Receive data %02x is error", packet[0]);
            esp_xmodem_transport_flush(receiver);
            return ret;
    }

    packet_size = buffer_size + XMODEM_HEAD_LEN + crc_len;
    esp_xmodem_print_packet(packet, packet_size);
    if (receiver->pack_num == 0) {
        if (packet[1] == 0) {
            ESP_LOGI(TAG, "This is a file begin transfer");
            esp_xmodem_parse_file_data(&receiver->file_data, &packet[XMODEM_HEAD_LEN]);
            esp_xmodem_dispatch_event(receiver, ESP_XMODEM_EVENT_ON_FILE, receiver->file_data.filename, receiver->file_data.file_length);
            esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_INIT, ACK);
            esp_xmodem_receiver_send_crc(receiver);
            receiver->pack_num ++;
            receiver->is_file_data = true;
            return ESP_OK;
        } else {
            receiver->is_file_data = false;
            receiver->pack_num ++;
        }
    } else {
        if (receiver->is_file_data) {
            if (packet[1] == 0 && packet[0] == SOH && packet[XMODEM_HEAD_LEN] == 0 && esp_xmodem_get_state(receiver) == XMODEM_STATE_RECEIVER_RECEIVE_EOT) {
                ESP_LOGI(TAG, "This is a file end transfer");
                esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_FINISHED, ACK);
                /* Set state to XMODEM_STATE_FINISH */
                esp_xmodem_set_state(receiver, XMODEM_STATE_FINISH);
                return ESP_OK;
            }
        }
    }

    if (packet[1] != ((packet[2] ^ 0xff) & 0xff) || packet[1] != (receiver->pack_num & 0xFF)) {
        esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_DATA_NUM_ERROR, ESP_XMODEM_EVENT_INIT, NAK);
        goto error;
    }

    if (crc_len == 2) {
        uint16_t crc16 = ((packet[packet_size - 2] << 8) & 0xFF00) + packet[packet_size - 1];
        if (esp_xmodem_crc16(&packet[XMODEM_HEAD_LEN], buffer_size) != crc16) {
            esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_DATA_CRC_ERROR, ESP_XMODEM_EVENT_INIT, NAK);
            goto error;
        }
    } else {
        uint8_t checksum = packet[packet_size - 1];
        if (esp_xmodem_checksum(&packet[XMODEM_HEAD_LEN], buffer_size) != checksum) {
            esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_DATA_CRC_ERROR, ESP_XMODEM_EVENT_INIT, NAK);
            goto error;
        }
    }

    if (p->recv_cb) {
        if (receiver->is_file_data && (receiver->write_len + buffer_size) > receiver->file_data.file_length) {
            ret = p->recv_cb(&packet[XMODEM_HEAD_LEN], receiver->file_data.file_length - receiver->write_len);
        } else {
            ret = p->recv_cb(&packet[XMODEM_HEAD_LEN], buffer_size);
        }
        receiver->pack_num ++;
        receiver->write_len += buffer_size;
        esp_xmodem_send_char_code_event(receiver, ESP_OK, ESP_XMODEM_EVENT_INIT, ACK);
    } else {
        ESP_LOGI(TAG, "recv_cb callback is NULL");
    }
error:
    return ret;
}

static void esp_xmodem_receive_process(void *pvParameters)
{
    esp_xmodem_t *receiver = (esp_xmodem_t *) pvParameters;
    esp_xmodem_config_t *p = receiver->config;
    uint8_t *packet = receiver->data;
    int i = 0, j = 0, crc_len = 0, fail_count = 0;
    esp_err_t ret = ESP_OK;

    for (i = 0; i < p->cycle_max_retry; i++) {
        crc_len = esp_xmodem_receiver_send_crc(receiver);
        if (esp_xmodem_read_data(receiver, p->cycle_timeout_ms) == 0) {
            ESP_LOGI(TAG, "Waiting for Xmodem sender to send data...");
            continue;
        } else {
            /* Set state to connected */
            esp_xmodem_set_state(receiver, XMODEM_STATE_CONNECTED);
            esp_xmodem_dispatch_event(receiver, ESP_XMODEM_EVENT_CONNECTED, NULL, 0);
            for (;;) {
                ret = esp_xmodem_receiver_process_data(receiver, packet, crc_len);
                if (ret != ESP_OK) {
                    fail_count ++;
                    ESP_LOGI(TAG, "err_code is 0x%x", (unsigned int)esp_xmodem_get_error_code(receiver));
                    if (fail_count == p->max_retry) {
                        receiver->process_handle = NULL;
                        esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_MAX_RETRY, ESP_XMODEM_EVENT_ERROR, CAN);
                        goto error;
                    }
                } else {
                    fail_count = 0;
                    if (esp_xmodem_get_state(receiver) == XMODEM_STATE_FINISH) {
                        goto error;
                    }
                }
                for (j = 0; j < p->max_retry; j++) {
                    uint32_t read_len = esp_xmodem_read_data(receiver, p->timeout_ms);
                    if (read_len) {
                        break;
                    }
                }
                if (j == p->max_retry) {
                    receiver->process_handle = NULL;
                    esp_xmodem_send_char_code_event(receiver, ESP_ERR_XMODEM_MAX_RETRY, ESP_XMODEM_EVENT_ERROR, CAN);
                    goto error;
                }
            }
        }
    }

    if (i == p->cycle_max_retry) {
        esp_xmodem_set_error_code(receiver, ESP_ERR_NO_XMODEM_SENDER);
        receiver->process_handle = NULL;
        esp_xmodem_dispatch_event(receiver, ESP_XMODEM_EVENT_ERROR, NULL, 0);
    }

error:
    receiver->process_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t esp_xmodem_receiver_start(esp_xmodem_handle_t receiver)
{
    if (!receiver) {
        return ESP_ERR_INVALID_ARG;
    }

    if (receiver->role != ESP_XMODEM_RECEIVER) {
        ESP_LOGE(TAG, "Now is receiver mode, please set role to ESP_XMODEM_RECEIVER");
        return ESP_ERR_INVALID_ARG;
    }

    if (receiver->state != XMODEM_STATE_INIT) {
        ESP_LOGE(TAG, "receiver state is not XMODEM_STATE_INIT");
        return ESP_ERR_XMODEM_STATE;
    }

    /* Set state to connecting */
    esp_xmodem_set_state(receiver, XMODEM_STATE_CONNECTING);

    if (xTaskCreate(esp_xmodem_receive_process, "xmodem_receive", ESP_XMODEM_RECEIVER_TASK_SIZE, receiver, ESP_XMODEM_RECEIVER_TASK_PRIO, &receiver->process_handle) != pdPASS) {
        ESP_LOGE(TAG, "Create xmodem_receiver fail");
        return ESP_FAIL;
    }

    return ESP_OK;
}
