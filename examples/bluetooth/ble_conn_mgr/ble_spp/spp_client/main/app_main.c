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
#define GATT_SPP_SVC_UUID                                  0xABF0

/* 16 Bit SPP Service Characteristic UUID */
#define GATT_SPP_CHR_UUID                                  0xABF1

#ifdef CONFIG_BT_NIMBLE_ENABLED
#define ATTRIBUTE_MAX_CONNECTIONS                   CONFIG_BT_NIMBLE_MAX_CONNECTIONS
#else
#define ATTRIBUTE_MAX_CONNECTIONS                   CONFIG_BT_ACL_CONNECTIONS
#endif

QueueHandle_t spp_common_uart_queue = NULL;
uint16_t attribute_handle[ATTRIBUTE_MAX_CONNECTIONS];

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_CONNECTED\n");
        attribute_handle[0] = 1;
        break;
    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DISCONNECTED\n");
        break;
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE:
        ESP_LOGI(TAG, "ESP_BLE_CONN_EVENT_DATA_RECEIVE\n");
        esp_ble_conn_data_t *conn_data = (esp_ble_conn_data_t *)event_data;
        switch (conn_data->type) {
        case BLE_CONN_UUID_TYPE_16:
            ESP_LOGI(TAG, "%u", conn_data->uuid.uuid16);
            break;
        case BLE_CONN_UUID_TYPE_32:
            ESP_LOG_BUFFER_HEX(TAG, &conn_data->uuid, sizeof(conn_data->uuid.uuid32));
            break;
        case BLE_CONN_UUID_TYPE_128:
            ESP_LOG_BUFFER_HEX(TAG, &conn_data->uuid, BLE_UUID128_VAL_LEN);
            break;
        default:
            break;
        }

        ESP_LOG_BUFFER_CHAR(TAG, conn_data->data, conn_data->data_len);

        esp_ble_conn_data_t inbuff = {
            .type = BLE_CONN_UUID_TYPE_16,
            .uuid = {
                .uuid16 = GATT_SPP_CHR_UUID,
            },
            .data = NULL,
            .data_len = 0,
        };
        esp_err_t rc = esp_ble_conn_read(&inbuff);
        if (rc == 0) {
            ESP_LOGI(TAG, "Read data success!");
            ESP_LOG_BUFFER_CHAR(TAG, inbuff.data, inbuff.data_len);
        } else {
            ESP_LOGE(TAG, "Error in reading characteristic rc=%d", rc);
        }
        break;
    default:
        break;
    }
}

static void ble_client_uart_task(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE client UART task started\n");
    int rc;
    int i;
    uart_event_t event;
    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(spp_common_uart_queue, (void *)&event, (TickType_t)portMAX_DELAY)) {
            switch (event.type) {
            //Event of UART receiving data
            case UART_DATA:
                if (event.size) {
                    /* Writing characteristics */
                    uint8_t *temp = NULL;
                    temp = (uint8_t *)malloc(sizeof(uint8_t) * event.size);
                    if (temp == NULL) {
                        ESP_LOGE(TAG, "malloc failed,%s L#%d\n", __func__, __LINE__);
                        break;
                    }
                    memset(temp, 0x0, event.size);
                    uart_read_bytes(UART_NUM_0, temp, event.size, portMAX_DELAY);
                    for (i = 0; i < ATTRIBUTE_MAX_CONNECTIONS; i++) {
                        if (attribute_handle[i] != 0) {
                            esp_ble_conn_data_t inbuff = {
                                .type = BLE_CONN_UUID_TYPE_16,
                                .uuid = {
                                    .uuid16 = GATT_SPP_CHR_UUID,
                                },
                                .data = temp,
                                .data_len = 1,
                            };
                            rc = esp_ble_conn_write(&inbuff);
                            if (rc == 0) {
                                ESP_LOGI(TAG, "Write in uart task success!");
                            } else {
                                ESP_LOGI(TAG, "Error in writing characteristic rc=%d", rc);
                            }
                            vTaskDelay(10);
                        }
                    }
                    free(temp);
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
    xTaskCreate(ble_client_uart_task, "uTask", 4096, (void*)UART_NUM_0, 8, NULL);
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
    if (esp_ble_conn_start() != ESP_OK) {
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler);
    }
}
