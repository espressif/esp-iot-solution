/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "unity.h"
#include "driver/uart.h"
#include "at_3gpp_ts_27_007.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "test_modem_at_usb";

extern ssize_t TEST_MEMORY_LEAK_THRESHOLD;

#define EX_UART_NUM UART_NUM_1
#define TEST_TXD (1)
#define TEST_RXD (2)
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

#define TEST_STOP_BIT BIT(1)
#define TEST_DONE_BIT BIT(2)

static EventGroupHandle_t event_group;
static at_handle_t at_handle = NULL;                /*!< AT command parser handle */
static QueueHandle_t uart_queue;

int force_link = 0;

static esp_err_t _uart_at_send_cmd(const char *cmd, size_t length, void *usr_data)
{
    return uart_write_bytes(EX_UART_NUM, (const char *) cmd, length);
}

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *) malloc(RD_BUF_SIZE);
    assert(dtmp);
    for (;;) {
        //Waiting for UART event.
        if (xQueueReceive(uart_queue, (void *)&event, pdMS_TO_TICKS(500))) {
            bzero(dtmp, RD_BUF_SIZE);
            switch (event.type) {
            //Event of UART receiving data
            case UART_DATA: {
                int length = event.size;
                char *buffer;
                size_t buffer_remain;
                modem_at_get_response_buffer(at_handle, &buffer, &buffer_remain);
                if (buffer_remain < length) {
                    length = buffer_remain;
                    ESP_LOGE(TAG, "data size is too big, truncated to %d", length);
                }
                uart_read_bytes(EX_UART_NUM, buffer, length, portMAX_DELAY);
                // Parse the AT command response
                modem_at_write_response_done(at_handle, length);
                break;
            }
            //Event of HW FIFO overflow detected
            case UART_FIFO_OVF:
                ESP_LOGI(TAG, "hw fifo overflow");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGI(TAG, "ring buffer full");
                uart_flush_input(EX_UART_NUM);
                xQueueReset(uart_queue);
                break;
            case UART_BREAK:
                ESP_LOGI(TAG, "uart rx break");
                break;
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "uart parity error");
                break;
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "uart frame error");
                break;
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        } else if (xEventGroupGetBits(event_group) & TEST_STOP_BIT) {
            break;
        }
    }
    free(dtmp);
    dtmp = NULL;
    uart_driver_delete(EX_UART_NUM);
    xEventGroupSetBits(event_group, TEST_DONE_BIT);
    ESP_LOGI(TAG, "uart_event_task deleted");
    vTaskDelete(NULL);
}

static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // .source_clk = UART_SCLK_XTAL,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, TEST_TXD, TEST_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    event_group = xEventGroupCreate();
    //Create a task to handler UART event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 3072, NULL, 12, NULL);
}

TEST_CASE("modem at set command", "[modem_at][uart][auto]")
{
    static bool is_first = true;
    if (is_first) {
        TEST_MEMORY_LEAK_THRESHOLD = -130;
    } else {
        TEST_MEMORY_LEAK_THRESHOLD = 0;
    }
    uart_init();

    // init the AT command parser
    modem_at_config_t at_config = {
        .recv_buffer_length = 128,
        .send_buffer_length = 256,
        .io = {
            .send_cmd = _uart_at_send_cmd,
            .usr_data = NULL,
        }
    };
    at_handle = modem_at_parser_create(&at_config);
    TEST_ASSERT_NOT_NULL(at_handle);

    TEST_ASSERT_EQUAL(ESP_OK, modem_at_start(at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, at_cmd_at(at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, at_cmd_set_echo(at_handle, true));

    char str[64] = {0};
    at_cmd_get_manufacturer_id(at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem manufacturer ID: \"%s\"", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_module_id(at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem module ID: \"%s\"", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_revision_id(at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem revision ID: \"%s\"", str);
    str[0] = '\0'; // clear the string buffer
    at_cmd_get_operator_name(at_handle, str, sizeof(str));
    ESP_LOGI(TAG, "Modem operator name: \"%s\"", str);

    esp_modem_at_csq_t result;
    at_cmd_get_signal_quality(at_handle, &result);
    ESP_LOGI(TAG, "Modem rssi: %d", result.rssi);

    esp_modem_pin_state_t pin_state;
    at_cmd_read_pin(at_handle, &pin_state);
    ESP_LOGI(TAG, "SIM pin state: %d", pin_state);

    TEST_ASSERT_EQUAL(ESP_OK, at_cmd_set_echo(at_handle, false));

    xEventGroupSetBits(event_group, TEST_STOP_BIT);
    xEventGroupWaitBits(event_group, TEST_DONE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    vEventGroupDelete(event_group);
    // free the AT command parser
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, modem_at_parser_destroy(at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, modem_at_stop(at_handle));
    TEST_ASSERT_EQUAL(ESP_OK, modem_at_parser_destroy(at_handle));

    at_handle = NULL;
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "TEST_DONE");
}
