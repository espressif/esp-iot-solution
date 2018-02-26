// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _IOT_DEBUG_H_
#define _IOT_DEBUG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/uart.h"

typedef enum {
    DEBUG_CMD_WIFI_INFO = 0,        /*!< command show wifi information*/
    DEBUG_CMD_WAKEUP_INFO,          /*!< command show wakeup reason*/
    DEBUG_CMD_RESTART,              /*!< command restart the chiip*/
} debug_cmd_type_t;

typedef void (* debug_cb_t)(void*);

typedef struct {
    char *cmd;                      /*!< command string*/
    debug_cb_t cb;                  /*!< callback function of the command*/
    void *arg;                      /*!< argument of the command*/
} debug_cmd_info_t;

/**
  * @brief  debug initialize
  *
  * @param  uart_num choose an uart port to send command
  * @param  baud_rate
  * @param  tx_io set the tx pin
  * @param  rx_io set the rx pin
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_debug_init(uart_port_t uart_num, int baud_rate, int tx_io, int rx_io);

/**
  * @brief  add custom debug command
  *
  * @param  cmd the command string
  * @param  cb the callback function of command
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_debug_add_custom_cmd(const char *cmd, debug_cb_t cb, void *arg);

/**
  * @brief  add debug command
  *
  * @param  cmd the command string
  * @param  cmd_type refer to enum debug_cmd_type_t
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_debug_add_cmd(const char *cmd, debug_cmd_type_t cmd_type);

/**
  * @brief  add custom debug commands in batch
  *
  * @param  debug_cmds 
  * @param  len lenght of debug_cmds
  *
  * @return
  *     - ESP_OK: succeed
  *     - others: fail
  */
esp_err_t iot_debug_add_cmd_group(debug_cmd_info_t debug_cmds[], int len);

#ifdef __cplusplus
}
#endif
#endif
