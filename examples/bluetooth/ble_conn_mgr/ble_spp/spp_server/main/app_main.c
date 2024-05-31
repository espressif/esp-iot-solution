/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/uart.h"

#include "esp_ble_conn_mgr.h"

static const char *TAG = "app_main";

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16                                  0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define BLE_SVC_SPP_CHR_UUID16                              0xABF1

#ifdef CONFIG_BT_NIMBLE_ENABLED
#define ATTRIBUTE_MAX_CONNECTIONS                   CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#else
#define ATTRIBUTE_MAX_CONNECTIONS                   CONFIG_BT_ACL_CONNECTIONS
#endif

QueueHandle_t spp_common_uart_queue = NULL;
uint16_t connection_handle[ATTRIBUTE_MAX_CONNECTIONS];

static esp_err_t esp_spp_chr_cb(const uint8_t *inbuf, uint16_t inlen,
                                uint8_t **outbuf, uint16_t *outlen, void *priv_data)
{
    if (!outbuf || !outlen) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!inbuf) {
        ESP_LOGI(TAG, "Callback for read");
        *outlen = strlen("SPP_CHR");
        *outbuf = (uint8_t *)strndup("SPP_CHR", *outlen);
    } else {
        ESP_LOGI(TAG, "Callback for write");
        *outbuf = calloc(1, inlen);
        memcpy(*outbuf, inbuf, inlen);
        *outlen = inlen;
    }

    return ESP_OK;
}

static const esp_ble_conn_character_t spp_nu_lookup_table[] = {
    {
        "spp_chr",           BLE_CONN_UUID_TYPE_16,  BLE_CONN_GATT_CHR_READ | BLE_CONN_GATT_CHR_WRITE | \
        BLE_CONN_GATT_CHR_NOTIFY | BLE_CONN_GATT_CHR_INDICATE, \
        { BLE_SVC_SPP_CHR_UUID16 }, esp_spp_chr_cb
    },
};

static const esp_ble_conn_svc_t spp_svc = {
    .type = BLE_CONN_UUID_TYPE_16,
    .uuid = {
        .uuid16 = BLE_SVC_SPP_UUID16,
    },
    .nu_lookup_count = sizeof(spp_nu_lookup_table) / sizeof(spp_nu_lookup_table[0]),
    .nu_lookup = (esp_ble_conn_character_t *)spp_nu_lookup_table
};

int gatt_svr_register(void)
{
    return esp_ble_conn_add_svc(&spp_svc);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED\n");
        connection_handle[0] = 1;
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED\n");
        connection_handle[0] = 0;
        break;
    default:
        break;
    }
}

void ble_server_uart_task(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE server UART_task started\n");
    uart_event_t event;
    int rc = 0;
    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(spp_common_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            switch (event.type) {
            //Event of UART receiving data
            case UART_DATA:
                if (event.size) {
                    static uint8_t ntf[1];
                    ntf[0] = 90;
                    for (int i = 0; i < ATTRIBUTE_MAX_CONNECTIONS; i++) {
                        if (connection_handle[i] != 0) {
                            esp_ble_conn_data_t inbuff = {
                                .type = BLE_CONN_UUID_TYPE_16,
                                .uuid = {
                                    .uuid16 = BLE_SVC_SPP_CHR_UUID16,
                                },
                                .data = ntf,
                                .data_len = 1,
                            };
                            rc = esp_ble_conn_notify(&inbuff);
                            if (rc == 0) {
                                ESP_LOGI(TAG, "Notification sent successfully");
                            } else {
                                ESP_LOGI(TAG, "Error in sending notification rc = %d", rc);
                            }
                        }
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    vTaskDelete(NULL);
}

static void ble_spp_uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_RTS,
        .rx_flow_ctrl_thresh = 122,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .source_clk = UART_SCLK_DEFAULT,
#else
        .source_clk = UART_SCLK_APB,
#endif
    };
    //Install UART driver, and get the queue.
    uart_driver_install(UART_NUM_0, 4096, 8192, 10, &spp_common_uart_queue, 0);
    //Set UART parameters
    uart_param_config(UART_NUM_0, &uart_config);
    //Set UART pins
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    xTaskCreate(ble_server_uart_task, "uTask", 4096, (void*)UART_NUM_0, 8, NULL);
}

void
app_main(void)
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

    esp_event_loop_create_default();
    esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL);

    ble_spp_uart_init();

    esp_ble_conn_init(&config);

    ret = gatt_svr_register();
    assert(ret == 0);

    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
    }
}
