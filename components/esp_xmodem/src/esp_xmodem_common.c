/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include <esp_xmodem_priv.h>
#include <esp_xmodem.h>
#include <esp_xmodem_transport.h>

static const char *TAG = "xmodem_common";

#ifdef CONFIG_IDF_TARGET_ESP8266
#define ESP_XMODEM_HANDLE_TASK_SIZE 2048
#else
#define ESP_XMODEM_HANDLE_TASK_SIZE CONFIG_ESP_XMODEM_SENDER_TASK_STACK_SIZE
#endif
#define ESP_XMODEM_HANDLE_TASK_PRIO CONFIG_ESP_XMODEM_STOP_TASK_PRIORITY

static void esp_xmodem_stop_task(void *pvParameters)
{
    /* Wait for the sender/receiver task to either react to the CAN we sent and
     * self-delete, or time out waiting so we can force-kill it.
     * IMPORTANT: we receive the *parent handle* (not a raw TaskHandle_t) so
     * we can re-read process_handle after the delay.  If the task already
     * cleaned up and set process_handle = NULL we must NOT call vTaskDelete
     * on that stale handle – doing so is undefined behaviour in FreeRTOS. */
    esp_xmodem_t *handle = (esp_xmodem_t *) pvParameters;
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (handle && handle->process_handle) {
        vTaskDelete(handle->process_handle);
        handle->process_handle = NULL;
    }
    vTaskDelete(NULL);
}

esp_xmodem_handle_t esp_xmodem_init(const esp_xmodem_config_t *config, void *transport_config)
{
    if (!config) {
        ESP_LOGE(TAG, "esp_xmodem_config_t config is NULL");
        return NULL;
    }

    if (!transport_config) {
        ESP_LOGE(TAG, "esp_xmodem_transport_handle_t transport_config is NULL");
        return NULL;
    }

    esp_xmodem_handle_t handle = NULL;
    handle = calloc(1, sizeof(esp_xmodem_t));
    if (!handle) {
        ESP_LOGE(TAG, "Error allocate handle memory");
        goto error;
    }
    handle->config = calloc(1, sizeof(esp_xmodem_config_t));
    if (!handle->config) {
        ESP_LOGE(TAG, "Error allocate handle->config memory");
        free(handle);
        handle = NULL;
        goto error;
    }
    handle->data_len = config->user_data_size + XMODEM_1K_DATA_LEN + XMODEM_HEAD_LEN + 2 + 1; //2 byte for crc
    handle->data = calloc(1, handle->data_len);
    if (!handle->data) {
        ESP_LOGE(TAG, "Error allocate handle->data memory");
        free(handle->config);
        handle->config = NULL;
        free(handle);
        handle = NULL;
        goto error;
    }
    esp_xmodem_config_t *p = handle->config;

    /* Initial the default value and set value */
    if (config->role == ESP_XMODEM_RECEIVER) {
        p->role = ESP_XMODEM_RECEIVER;
        p->crc_type = !config->crc_type ? ESP_XMODEM_CHECKSUM : config->crc_type;
        p->cycle_timeout_ms = !config->cycle_timeout_ms ? 3 * 1000 : config->cycle_timeout_ms;
        p->recv_cb = config->recv_cb;
        handle->pack_num = 0;
    } else {
        p->role = ESP_XMODEM_SENDER;
        p->cycle_timeout_ms = !config->cycle_timeout_ms ? 10 * 1000 : config->cycle_timeout_ms;
        p->support_xmodem_1k = config->support_xmodem_1k ? true : false;
        handle->pack_num = 1;
    }

    p->timeout_ms = !config->timeout_ms ? 1000 : config->timeout_ms;
    p->max_retry = !config->max_retry ? 10 : config->max_retry;
    p->cycle_max_retry = !config->cycle_max_retry ? 25 : config->cycle_max_retry;
    p->user_data_size = config->user_data_size;
    handle->role = config->role;
    handle->transport = transport_config;
    handle->event_handler = config->event_handler;
    handle->event.handle = handle;
    handle->error_code = ESP_OK;
    handle->is_file_data = false;
    handle->write_len = 0;
    handle->state = XMODEM_STATE_INIT;
    return handle;
error:
    return NULL;
}

esp_err_t esp_xmodem_start(esp_xmodem_handle_t handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_FAIL;
    }

    if (handle->role == ESP_XMODEM_SENDER) {
        return esp_xmodem_sender_start(handle);
    } else {
        return esp_xmodem_receiver_start(handle);
    }
}

void esp_xmodem_print_packet(uint8_t *packet, uint32_t len)
{
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, packet, len, ESP_LOG_DEBUG);
}

esp_err_t esp_xmodem_wait_response(esp_xmodem_t *handle, uint8_t *ch)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    int timeout_ms = 0;
    uint8_t byte = 0;
    esp_xmodem_config_t *p = handle->config;
    timeout_ms = p->timeout_ms;

    if (esp_xmodem_read_byte(handle, &byte, timeout_ms) == ESP_FAIL) {
        return ESP_FAIL;
    } else {
        *ch = byte;
        return ESP_OK;
    }
}

/* CRC16 for 1024 Xmodem data */
uint16_t esp_xmodem_crc16(uint8_t *buffer, uint32_t len)
{
    uint16_t crc16 = 0;

    while(len != 0) {
        crc16  = (uint8_t)(crc16 >> 8) | (crc16 << 8);
        crc16 ^= *buffer;
        crc16 ^= (uint8_t)(crc16 & 0xff) >> 4;
        crc16 ^= (crc16 << 8) << 4;
        crc16 ^= ((crc16 & 0xff) << 4) << 1;
        buffer++;
        len--;
    }

    return crc16;
}

/* Checksum for 128 Xmodem data */
uint8_t esp_xmodem_checksum(uint8_t *buffer, uint32_t len)
{
    uint8_t checksum = 0;

    for (uint32_t i = 0; i < len; ++i) {
      checksum += buffer[i];
    }

    return checksum;
}

/* Send char to peer, set error code and send event to userspace*/
void esp_xmodem_send_char_code_event(esp_xmodem_t *handle, int err_code, int event, uint8_t ch)
{
    if (!handle) {
        return;
    }
    uint32_t len = 1;
    uint8_t *packet = handle->data;
    memset(packet, 0, handle->data_len);
    packet[0] = ch;
    if (ch == CAN) {
        esp_xmodem_send_data(handle, &ch, &len);
    }
    esp_xmodem_send_data(handle, &ch, &len);
    if (err_code != ESP_ERR_XMODEM_BASE) {
        esp_xmodem_set_error_code(handle, err_code);
    }
    if (event != ESP_XMODEM_EVENT_INIT) {
        esp_xmodem_dispatch_event(handle, event, NULL, 0);
    }
}

void esp_xmodem_set_state(esp_xmodem_t *handle, esp_xmodem_state_t state)
{
    if (handle) {
        handle->state = state;
    }
}

esp_xmodem_state_t esp_xmodem_get_state(esp_xmodem_t *handle)
{
    if (handle) {
        return handle->state;
    } else {
        return XMODEM_STATE_UNINIT;
    }
}

void esp_xmodem_set_error_code(esp_xmodem_t *handle, int code)
{
    if (handle) {
        handle->error_code = code;
    }
}

esp_err_t esp_xmodem_dispatch_event(esp_xmodem_t *handle, esp_xmodem_event_id_t event_id, void *data, uint32_t len)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_xmodem_event_t *event = &handle->event;

    if (handle->event_handler) {
        event->event_id = event_id;
        event->data = data;
        event->data_len = len;
        return handle->event_handler(event);
    }
    return ESP_OK;
}

int esp_xmodem_get_crc_len(esp_xmodem_t *handle)
{
    int crc_len = 0;
    if (!handle) {
        return crc_len;
    }

    if (handle->crc_type == ESP_XMODEM_CHECKSUM) {
        crc_len = 1;
    } else {
        crc_len = 2;
    }
    return crc_len;
}

/* Stop Xmodem handle */
esp_err_t esp_xmodem_stop(esp_xmodem_t *handle)
{
    if (!handle) {
        return ESP_FAIL;
    }

    if (handle->role == ESP_XMODEM_SENDER) {
        handle->pack_num = 1;
    } else {
        handle->pack_num = 0;
    }
    handle->error_code = ESP_OK;
    handle->is_file_data = false;
    handle->write_len = 0;
    handle->state = XMODEM_STATE_INIT;
    esp_xmodem_send_char_code_event(handle, ESP_ERR_XMODEM_BASE, ESP_XMODEM_EVENT_INIT, CAN);
    if (xTaskCreate(esp_xmodem_stop_task, "xmodem_stop", ESP_XMODEM_HANDLE_TASK_SIZE, handle, ESP_XMODEM_HANDLE_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Create xmodem_stop task fail");
        return ESP_FAIL;
    }
    esp_xmodem_transport_flush(handle);
    return ESP_OK;
}

/* Clean Xmodem handle */
esp_err_t esp_xmodem_clean(esp_xmodem_t *handle)
{
    if (!handle) {
        ESP_LOGE(TAG, "handle is NULL");
        return ESP_FAIL;
    }

    if (handle->config) {
        free(handle->config);
        handle->config = NULL;
    }

    if (handle->event_handler) {
        handle->event_handler = NULL;
    }

    if (handle->data) {
        free(handle->data);
        handle->data = NULL;
    }

    free(handle);
    handle = NULL;
    return ESP_OK;
}

/* Get Xmodem error code */
int esp_xmodem_get_error_code(esp_xmodem_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    } else {
        return handle->error_code;
    }
}
