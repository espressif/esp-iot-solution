/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

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
esp_err_t debug_init(uart_port_t uart_num, int baud_rate, int tx_io, int rx_io);

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
esp_err_t debug_add_custom_cmd(const char *cmd, debug_cb_t cb, void *arg);

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
esp_err_t debug_add_cmd(const char *cmd, debug_cmd_type_t cmd_type);

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
esp_err_t debug_add_cmd_group(debug_cmd_info_t debug_cmds[], int len);

#ifdef __cplusplus
}
#endif
#endif