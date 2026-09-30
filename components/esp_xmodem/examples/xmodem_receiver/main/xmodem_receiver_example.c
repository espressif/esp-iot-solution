/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include <esp_ota_ops.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_xmodem.h"
#include "esp_xmodem_transport.h"

static const char *TAG = "xmodem_receiver_example";
static esp_ota_handle_t update_handle = 0;
static const esp_partition_t *update_partition = NULL;
static bool ota_init = false;

static esp_err_t xmodem_ota_init(void)
{
    ESP_LOGI(TAG, "Starting OTA...");
    if (!ota_init) {
        update_partition = esp_ota_get_next_update_partition(NULL);
        if (update_partition == NULL) {
            ESP_LOGE(TAG, "Passive OTA partition not found");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%" PRIx32,
                update_partition->subtype, update_partition->address);

        esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
            return err;
        }
        ota_init = true;
    }
    ESP_LOGI(TAG, "esp_ota_begin succeeded");
    ESP_LOGI(TAG, "Please Wait. This may take time");
    return ESP_OK;
}

static esp_err_t xmodem_ota_end(void)
{
    if (!update_handle) {
        ESP_LOGE(TAG, "update handle is NULL");
        return ESP_FAIL;
    }

    esp_err_t ota_end_err = esp_ota_end(update_handle);
    if (ota_end_err != ESP_OK) {
        ESP_LOGE(TAG, "Error: esp_ota_end failed! err=%s. Image is invalid", esp_err_to_name(ota_end_err));
        return ota_end_err;
    }

    ota_end_err = esp_ota_set_boot_partition(update_partition);
    if (ota_end_err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=%s", esp_err_to_name(ota_end_err));
        return ota_end_err;
    }
    ESP_LOGI(TAG, "esp_ota_set_boot_partition succeeded");
    return ESP_OK;
}

static esp_err_t xmodem_receiver_event_handler(esp_xmodem_event_t *evt)
{
    switch(evt->event_id) {
        case ESP_XMODEM_EVENT_ERROR:
            ESP_LOGI(TAG, "ESP_XMODEM_EVENT_ERROR, err_code is 0x%x, heap=%" PRIu32, (unsigned int)esp_xmodem_get_error_code(evt->handle), esp_get_free_heap_size());
            ota_init = false;
            xmodem_ota_end();
            if (esp_xmodem_stop(evt->handle) == ESP_OK) {
                esp_xmodem_start(evt->handle);
            } else {
                ESP_LOGE(TAG, "esp_xmodem_stop fail");
                esp_xmodem_transport_close(evt->handle);
                esp_xmodem_clean(evt->handle);
            }
            break;
        case ESP_XMODEM_EVENT_CONNECTED:
            ESP_LOGI(TAG, "ESP_XMODEM_EVENT_CONNECTED");
            if (xmodem_ota_init() != ESP_OK) {
                ESP_LOGE(TAG, "xmodem_ota_init fail");
                esp_xmodem_transport_close(evt->handle);
                esp_xmodem_clean(evt->handle);
            }
            break;
        case ESP_XMODEM_EVENT_FINISHED:
            ESP_LOGI(TAG, "ESP_XMODEM_EVENT_FINISHED");
            if (xmodem_ota_end() == ESP_OK) {
                ESP_LOGI(TAG, "OTA successfully, will restart now...");
                vTaskDelay(1000/portTICK_PERIOD_MS);
                esp_restart();
            } else {
                ESP_LOGE(TAG, "xmodem_ota_end fail");
                esp_xmodem_transport_close(evt->handle);
                esp_xmodem_clean(evt->handle);
            }
            break;
        case ESP_XMODEM_EVENT_ON_SEND_DATA:
            ESP_LOGD(TAG, "ESP_XMODEM_EVENT_ON_SEND_DATA, heap=%" PRIu32, esp_get_free_heap_size());
            break;
        case ESP_XMODEM_EVENT_ON_RECEIVE_DATA:
            ESP_LOGD(TAG, "ESP_XMODEM_EVENT_ON_RECEIVE_DATA, heap=%" PRIu32 " data_len=%" PRIu32, esp_get_free_heap_size(), evt->data_len);
            break;
        case ESP_XMODEM_EVENT_ON_FILE:
            ESP_LOGI(TAG, "ESP_XMODEM_EVENT_ON_FILE");
            ESP_LOGI(TAG, "file_name is %s, file_length is %" PRIu32, (char *)evt->data, evt->data_len);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t xmodem_data_recv(uint8_t *data, uint32_t len)
{
    if (!update_handle) {
        ESP_LOGE(TAG, "update handle is NULL");
        return ESP_FAIL;
    }

    esp_err_t ota_write = esp_ota_write(update_handle, (const void *)data, len);
    if (ota_write != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t xmodem_receiver_start(void)
{
    esp_xmodem_transport_config_t transport_config = {
        .baud_rate = 921600,
        #if defined(CONFIG_IDF_TARGET_ESP32)
        .uart_num = UART_NUM_1,
        .swap_pin = true,
        .tx_pin = 17,
        .rx_pin = 16,
        .cts_pin = 15,
        .rts_pin = 14,
        #elif defined(CONFIG_IDF_TARGET_ESP32S3)
        .uart_num = UART_NUM_1,
        .swap_pin = true,
        .tx_pin = 17,
        .rx_pin = 18,
        .cts_pin = UART_PIN_NO_CHANGE,
        .rts_pin = UART_PIN_NO_CHANGE,
        #else
        .uart_num = UART_NUM_1,
        #endif
    };
    esp_xmodem_transport_handle_t transport_handle = esp_xmodem_transport_init(&transport_config);
    if (!transport_handle) {
        ESP_LOGE(TAG, "esp_xmodem_transport_init fail");
        return ESP_FAIL;
    }

    esp_xmodem_config_t config = {
        .role = ESP_XMODEM_RECEIVER,
        .crc_type = ESP_XMODEM_CRC16,
        .event_handler = xmodem_receiver_event_handler,
        .recv_cb = xmodem_data_recv,
        .cycle_max_retry = 25,
    };
    esp_xmodem_handle_t receiver = esp_xmodem_init(&config, transport_handle);
    if (!receiver) {
        ESP_LOGE(TAG, "esp_xmodem_init fail");
        ESP_ERROR_CHECK(uart_driver_delete(transport_config.uart_num));
        if (transport_handle) {
            free(transport_handle);
            transport_handle = NULL;
        }
        return ESP_FAIL;
    }

    ota_init = false;
    esp_xmodem_start(receiver);
    return ESP_OK;
}

void app_main()
{
    xmodem_receiver_start();
}
