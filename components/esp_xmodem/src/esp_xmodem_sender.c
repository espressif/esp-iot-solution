/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
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

static const char *TAG = "xmodem_sender";

#ifdef CONFIG_IDF_TARGET_ESP8266
#define ESP_XMODEM_SENDER_TASK_SIZE 2048
#else
#define ESP_XMODEM_SENDER_TASK_SIZE CONFIG_ESP_XMODEM_SENDER_TASK_STACK_SIZE
#endif
#define ESP_XMODEM_SENDER_TASK_PRIO CONFIG_ESP_XMODEM_SENDER_TASK_PRIORITY

static void esp_xmodem_send_process(void *pvParameters)
{
    esp_xmodem_t *sender = (esp_xmodem_t *) pvParameters;
    esp_xmodem_config_t *p = sender->config;
    uint8_t ch = 0;
    int i = 0;

    for (i = 0; i < p->cycle_max_retry; i++ ) {
        if (esp_xmodem_read_byte(sender, &ch, p->cycle_timeout_ms) == ESP_FAIL) {
            ESP_LOGI(TAG, "Connecting to Xmodem server(%d/%d)", i + 1, p->cycle_max_retry);
            continue;
        } else {
            if (ch == CRC16 || ch == NAK) {
                sender->crc_type = (ch == CRC16) ? ESP_XMODEM_CRC16 : ESP_XMODEM_CHECKSUM;
                 /* Set state to connected */
                esp_xmodem_set_state(sender, XMODEM_STATE_CONNECTED);
                esp_xmodem_set_error_code(sender, ESP_OK);
                esp_xmodem_dispatch_event(sender, ESP_XMODEM_EVENT_CONNECTED, NULL, 0);
                break;
            } else {
                ESP_LOGE(TAG, "Receive 0x%02x data, only support 'C' or NAK", ch);
                esp_xmodem_set_error_code(sender, ESP_ERR_XMODEM_CRC_NOT_SUPPORT);
                esp_xmodem_transport_flush(sender);
                continue;
            }
        }
    }

    if (i == p->cycle_max_retry) {
        ESP_LOGE(TAG, "Connecting to Xmodem receiver fail");
        esp_xmodem_set_error_code(sender, ESP_ERR_NO_XMODEM_RECEIVER);
        sender->process_handle = NULL;
        esp_xmodem_dispatch_event(sender, ESP_XMODEM_EVENT_ERROR, NULL, 0);
    }
    sender->process_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t esp_xmodem_sender_start(esp_xmodem_handle_t sender)
{
    if (!sender) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->role != ESP_XMODEM_SENDER) {
        ESP_LOGE(TAG, "Now is sender mode,please set role to ESP_XMODEM_SENDER");
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->state != XMODEM_STATE_INIT) {
        ESP_LOGE(TAG, "sender state is not XMODEM_STATE_INIT");
        return ESP_ERR_XMODEM_STATE;
    }

    /* Set state to connecting */
    esp_xmodem_set_state(sender, XMODEM_STATE_CONNECTING);

    if (xTaskCreate(esp_xmodem_send_process, "xmodem_send", ESP_XMODEM_SENDER_TASK_SIZE, sender, ESP_XMODEM_SENDER_TASK_PRIO, &sender->process_handle) != pdPASS) {
        ESP_LOGE(TAG, "Create xmodem_sender fail");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t esp_xmodem_sender_process(esp_xmodem_t *sender, uint8_t *packet, uint32_t xmodem_packet_len, uint32_t *write_len, bool is_file)
{
    if (!sender) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_xmodem_config_t *p = sender->config;
    int i = 0;
    uint8_t ch = 0;

    for (i = 0; i < p->max_retry; i ++) {
        *write_len = esp_xmodem_send_data(sender, packet, &xmodem_packet_len);
        if (xmodem_packet_len != *write_len) {
            esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_DATA_SEND_ERROR, ESP_XMODEM_EVENT_ERROR, CAN);
            goto error;
        }
    
        if (esp_xmodem_wait_response(sender, &ch) == ESP_OK) {
            if (is_file) {
                if (ch == ACK) {
                    if (esp_xmodem_wait_response(sender, &ch) == ESP_OK) {
                        if (ch == CRC16 || ch == NAK) {
                            break;
                        } else if (ch == CAN) {
                            esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_RECEIVE_CAN, ESP_XMODEM_EVENT_ERROR, ACK);
                            goto error;
                        }
                    }
                    break;
                }
                esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_DATA_RECV_ERROR, ESP_XMODEM_EVENT_ERROR, CAN);
                goto error;
            } else {
                if (ch == ACK) {
                    break;
                } else if (ch == NAK) {
                    continue;
                } else if (ch == CAN) {
                    esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_RECEIVE_CAN, ESP_XMODEM_EVENT_ERROR, ACK);
                    goto error;
                } else {
                    esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_DATA_RECV_ERROR, ESP_XMODEM_EVENT_ERROR, CAN);
                    goto error;
                }
            }
        } else {
            ESP_LOGI(TAG, "esp_xmodem_wait_response timeout");
        }
    }

    if (i == p->max_retry) {
        esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_MAX_RETRY, ESP_XMODEM_EVENT_ERROR, CAN);
        goto error;
    }
    return ESP_OK;
error:
    return ESP_FAIL;
}

esp_err_t esp_xmodem_sender_send_file_packet(esp_xmodem_handle_t sender, char *filename, int filename_length, uint32_t total_length)
{
    uint8_t crc_len = 0;
    uint32_t write_len = 0, xmodem_packet_len = 0;
    esp_err_t ret = ESP_FAIL;
    crc_len = esp_xmodem_get_crc_len(sender);

    uint8_t *packet = sender->data;
    if (!packet) {
        ESP_LOGE(TAG, "No mem for Xmodem file packet");
        return ESP_ERR_NO_MEM;
    }
    memset(packet, 0, sender->data_len);
    packet[0] = SOH;
    packet[1] = 0x00;
    packet[2] = 0xFF;

    if (filename && filename_length && total_length) {
        memcpy(&packet[XMODEM_HEAD_LEN], filename, filename_length);
        sprintf((char *)&packet[XMODEM_HEAD_LEN + filename_length + 1], "%" PRIu32 "%c", total_length, ' ');
        sender->is_file_data = true;
        sender->file_data.file_length = total_length;
        memcpy(sender->file_data.filename, filename, filename_length);
    } else {
        if (sender->state != XMODEM_STATE_SENDER_SEND_EOT) {
            ESP_LOGE(TAG, "Please input the filename, filename length and total length");
            return ret;
        }
    }
  
    if (crc_len == 1) {
        uint8_t checksum = esp_xmodem_checksum(&packet[XMODEM_HEAD_LEN], XMODEM_DATA_LEN);
        packet[XMODEM_DATA_LEN + XMODEM_HEAD_LEN] = checksum;
    } else {
        uint16_t crc16 = esp_xmodem_crc16(&packet[XMODEM_HEAD_LEN], XMODEM_DATA_LEN);
        packet[XMODEM_DATA_LEN + XMODEM_HEAD_LEN] = (crc16 >> 8) & 0xFF;
        packet[XMODEM_DATA_LEN + XMODEM_HEAD_LEN + 1] = crc16 & 0xFF;
    }

    xmodem_packet_len = XMODEM_DATA_LEN + XMODEM_HEAD_LEN + crc_len;
    if (esp_xmodem_sender_process(sender, packet, xmodem_packet_len, &write_len, true) == ESP_OK) {
        sender->pack_num = 1;
        ret = ESP_OK;
    } else {
        sender->pack_num = 0;
    }

    return ret;
}

esp_err_t esp_xmodem_sender_send(esp_xmodem_handle_t sender, uint8_t *data, uint32_t len)
{
    if (!sender) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->role != ESP_XMODEM_SENDER) {
        ESP_LOGE(TAG, "Now is sender mode, please set role to ESP_XMODEM_SENDER");
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->state != XMODEM_STATE_CONNECTED) {
        ESP_LOGE(TAG, "sender state is not XMODEM_STATE_CONNECTED");
        return ESP_ERR_XMODEM_STATE;
    }

    if (!data) {
        ESP_LOGE(TAG, "The send data is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t crc_len = 0, head = SOH, *packet = NULL;
    uint32_t write_len = 0, xmodem_data_len = 0, xmodem_packet_len = 0, need_write_len = 0;
    int32_t left_len = len;
    esp_err_t ret = ESP_FAIL;
    crc_len = esp_xmodem_get_crc_len(sender);
    esp_xmodem_config_t *p = sender->config;

    while (left_len > 0) {
        if (p->support_xmodem_1k) {
            if (left_len <= XMODEM_DATA_LEN) {
                head = SOH;
                xmodem_data_len = XMODEM_DATA_LEN;
            } else {
                head = STX;
                xmodem_data_len = XMODEM_1K_DATA_LEN;
            }
        } else {
            head = SOH;
            xmodem_data_len = XMODEM_DATA_LEN;
        }
        packet = sender->data;
        if (!packet) {
            ESP_LOGE(TAG, "No mem for Xmodem packet");
            return ESP_ERR_NO_MEM;
        }
        memset(packet, 0, sender->data_len);
        if (sender->is_file_data) {
            memset(&packet[XMODEM_HEAD_LEN], 0x1A, xmodem_data_len);
        }
        need_write_len = (left_len > xmodem_data_len) ? xmodem_data_len : left_len;
        packet[0] = head;
        packet[1] = sender->pack_num & 0xFF;
        packet[2] = ~(sender->pack_num & 0xFF);

        memcpy(&packet[XMODEM_HEAD_LEN], data + write_len, need_write_len);
        if (crc_len == 1) {
            uint8_t checksum = esp_xmodem_checksum(&packet[XMODEM_HEAD_LEN], xmodem_data_len);
            packet[xmodem_data_len + XMODEM_HEAD_LEN] = checksum;
        } else {
            uint16_t crc16 = esp_xmodem_crc16(&packet[XMODEM_HEAD_LEN], xmodem_data_len);
            packet[xmodem_data_len + XMODEM_HEAD_LEN] = (crc16 >> 8) & 0xFF;
            packet[xmodem_data_len + XMODEM_HEAD_LEN + 1] = crc16 & 0xFF;
        }

        xmodem_packet_len = XMODEM_HEAD_LEN + xmodem_data_len + crc_len;
        if (esp_xmodem_sender_process(sender, packet, xmodem_packet_len, &write_len, false) == ESP_OK) {
            left_len -= need_write_len;
            sender->write_len += need_write_len;
            sender->pack_num ++;
            ret = ESP_OK;
        } else {
            sender->pack_num = 1;
            ret = ESP_FAIL;
            goto err;
        }
    }
err:
    return ret;
}

esp_err_t esp_xmodem_sender_send_cancel(esp_xmodem_handle_t sender)
{
    if (!sender) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->role != ESP_XMODEM_SENDER) {
        ESP_LOGE(TAG, "Now is sender mode, please set role to ESP_XMODEM_SENDER");
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->state != XMODEM_STATE_CONNECTED) {
        ESP_LOGE(TAG, "sender state is not XMODEM_STATE_CONNECTED");
        return ESP_ERR_XMODEM_STATE;
    }
    esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_INIT, CAN);
    return ESP_OK;
}

esp_err_t esp_xmodem_sender_send_eot(esp_xmodem_handle_t sender)
{
    if (!sender) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->role != ESP_XMODEM_SENDER) {
        ESP_LOGE(TAG, "Now is sender mode, please set role to ESP_XMODEM_SENDER");
        return ESP_ERR_INVALID_ARG;
    }

    if (sender->state != XMODEM_STATE_CONNECTED) {
        ESP_LOGE(TAG, "sender state is not XMODEM_STATE_CONNECTED");
        return ESP_ERR_XMODEM_STATE;
    }

    int i = 0;
    uint8_t ch = 0;
    esp_xmodem_config_t *p = sender->config;

    for (i = 0; i < p->max_retry; i ++) {
        if (esp_xmodem_get_state(sender) != XMODEM_STATE_SENDER_SEND_EOT) {
            esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_INIT, EOT);
        }
        if (esp_xmodem_read_byte(sender, &ch, p->cycle_timeout_ms) == ESP_OK) {
            if (esp_xmodem_get_state(sender) == XMODEM_STATE_SENDER_SEND_EOT) {
                if (ch == NAK) {
                    sender->crc_type = ESP_XMODEM_CHECKSUM;
                } else if (ch == CRC16) {
                    sender->crc_type = ESP_XMODEM_CRC16;
                } else {
                    return ESP_OK;
                }
                return esp_xmodem_sender_send_file_packet(sender, NULL, 0, 0);
            } else {
                if (ch == ACK) {
                    esp_xmodem_set_state(sender, XMODEM_STATE_SENDER_SEND_EOT);
                }
            }
        } else {
            if (esp_xmodem_get_state(sender) == XMODEM_STATE_SENDER_SEND_EOT) {
                break;
            }
        }
    }

    if (i == p->max_retry) {
        esp_xmodem_send_char_code_event(sender, ESP_ERR_XMODEM_MAX_RETRY, ESP_XMODEM_EVENT_ERROR, CAN);
        return ESP_FAIL;
    } else {
        return ESP_OK;
    }
}
