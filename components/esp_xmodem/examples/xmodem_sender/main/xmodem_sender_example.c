/* SPDX-FileCopyrightText: 2020-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "driver/uart.h"
#include "esp_xmodem.h"
#include "esp_xmodem_transport.h"

#define WEB_PORT CONFIG_SERVER_PORT
#define WEB_IP CONFIG_SERVER_IP
#define FILE_NAME CONFIG_EXAMPLE_FILENAME
#define OTA_BUF_SIZE 1024

static const char *TAG = "xmodem_sender_example";
static const char *xmodem_url = "http://" WEB_IP ":" WEB_PORT "/" FILE_NAME;
static char upgrade_data_buf[OTA_BUF_SIZE + 1];

static void http_client_task(void *pvParameters)
{
    esp_xmodem_handle_t xmodem_sender = (esp_xmodem_handle_t) pvParameters;
    esp_err_t err;
    esp_http_client_config_t config = {
        .url = xmodem_url,
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&config);
    if (http_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        goto err;
    }
    err = esp_http_client_open(http_client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(http_client);
        goto err;
    }
    int content_length = esp_http_client_fetch_headers(http_client);

    if (content_length <= 0) {
        ESP_LOGE(TAG, "No content found in the image");
        goto FAIL;
    }

    int image_data_remaining = content_length;
    int binary_file_len = 0;

#ifdef CONFIG_SUPPORT_FILE
    if (esp_xmodem_sender_send_file_packet(xmodem_sender, FILE_NAME, strlen(FILE_NAME), content_length) != ESP_OK) {
        ESP_LOGE(TAG, "Send filename fail");
        goto FAIL;
    }
#endif
    while (image_data_remaining != 0) {
        int data_max_len;
        if (image_data_remaining < OTA_BUF_SIZE) {
            data_max_len = image_data_remaining;
        } else {
            data_max_len = OTA_BUF_SIZE;
        }
        int data_read = esp_http_client_read(http_client, upgrade_data_buf, data_max_len);
        if (data_read == 0) {
            if (errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGE(TAG, "Connection closed, errno = %d", errno);
                break;
            }
            if (content_length == binary_file_len) {
                ESP_LOGI(TAG, "Connection closed,all data received");
                break;
            }
        } else if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            break;
        } else if (data_read > 0) {
            err = esp_xmodem_sender_send(xmodem_sender, (uint8_t *)upgrade_data_buf, data_read);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error:esp_xmodem_sender_send failed!");
                goto FAIL;
            }
            image_data_remaining -= data_read;
            binary_file_len += data_read;
            ESP_LOGD(TAG, "Written image length %d", binary_file_len);
        }
    }
    ESP_LOGD(TAG, "Total binary data length writen: %d", binary_file_len);

    if (content_length != binary_file_len) {
        ESP_LOGE(TAG, "Error in receiving complete file");
        esp_xmodem_sender_send_cancel(xmodem_sender);
        goto FAIL;
    } else {
        err = esp_xmodem_sender_send_eot(xmodem_sender);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error:esp_xmodem_sender_send_eot FAIL!");
            goto FAIL;
        }
        ESP_LOGI(TAG, "Send image success");
        esp_xmodem_transport_close(xmodem_sender);
        esp_xmodem_clean(xmodem_sender);
    }
FAIL:
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
err:
    vTaskDelete(NULL);
}

esp_err_t xmodem_sender_event_handler(esp_xmodem_event_t *evt)
{
    switch(evt->event_id) {
        case ESP_XMODEM_EVENT_ERROR:
            ESP_LOGI(TAG, "ESP_XMODEM_EVENT_ERROR, err_code is 0x%" PRIx32 ", heap size %" PRIu32, (uint32_t)esp_xmodem_get_error_code(evt->handle), esp_get_free_heap_size());
            if (esp_xmodem_stop(evt->handle) == ESP_OK) {
                esp_xmodem_start(evt->handle);
            } else {
                ESP_LOGE(TAG, "esp_xmodem_stop fail");
                esp_xmodem_transport_close(evt->handle);
                esp_xmodem_clean(evt->handle);
            }
            break;
        case ESP_XMODEM_EVENT_CONNECTED:
            ESP_LOGI(TAG, "ESP_XMODEM_EVENT_CONNECTED, heap size %" PRIu32, esp_get_free_heap_size());
            if (xTaskCreate(&http_client_task, "http_client_task", 8192, evt->handle, 5, NULL) != pdPASS) {
                ESP_LOGE(TAG, "http_client_task create fail");
                return ESP_FAIL;
            }
            break;
        case ESP_XMODEM_EVENT_FINISHED:
            ESP_LOGI(TAG, "ESP_XMODEM_EVENT_FINISHED");
            break;
        case ESP_XMODEM_EVENT_ON_SEND_DATA:
            ESP_LOGD(TAG, "ESP_XMODEM_EVENT_ON_SEND_DATA, %" PRIu32, esp_get_free_heap_size());
            break;
        case ESP_XMODEM_EVENT_ON_RECEIVE_DATA:
            ESP_LOGD(TAG, "ESP_XMODEM_EVENT_ON_RECEIVE_DATA, %" PRIu32, esp_get_free_heap_size());
            break;
        default:
            break;
    }
    return ESP_OK;
}

void app_main()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http client task");

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
        return;
    }

    esp_xmodem_config_t config = {
        .role = ESP_XMODEM_SENDER,
        .event_handler = xmodem_sender_event_handler,
        .support_xmodem_1k = true,
    };
    esp_xmodem_handle_t sender = esp_xmodem_init(&config, transport_handle);
    if (sender) {
        esp_xmodem_start(sender);
    } else {
        ESP_LOGE(TAG, "esp_xmodem_init fail");
    }
}
