// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_log.h"
#include "esp_modem.h"
#include "esp_modem_dte.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

/**
 * @brief Main lifecycle states of the esp-modem
 *
 * The bits below are used to indicate events in process_group
 * field of the esp_modem_dte_internal_t
 */
#define ESP_MODEM_START_BIT     BIT0
#define ESP_MODEM_COMMAND_BIT   BIT1
#define ESP_MODEM_STOP_PPP_BIT  BIT2
#define ESP_MODEM_STOP_BIT      BIT3

/**
 * @brief ESP32 Modem DTE
 *
 */
typedef struct {
    uart_port_t uart_port;                  /*!< UART port */
    uint8_t *buffer;                        /*!< Internal buffer to store response lines/data from DCE */
    QueueHandle_t event_queue;              /*!< UART event queue handle */
    esp_event_loop_handle_t event_loop_hdl; /*!< Event loop handle */
    TaskHandle_t uart_event_task_hdl;       /*!< UART event task handle */
    EventGroupHandle_t process_group;       /*!< Event group used for indicating DTE state changes */
    esp_modem_dte_t parent;                 /*!< DTE interface that should extend */
    esp_modem_on_receive receive_cb;        /*!< ptr to data reception */
    void *receive_cb_ctx;                   /*!< ptr to rx fn context data */
    int line_buffer_size;                   /*!< line buffer size in command mode */
    int pattern_queue_size;                 /*!< UART pattern queue size */
} esp_modem_dte_internal_t;

#ifdef __cplusplus
}
#endif
