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

#ifndef _IOT_SOCKET_DEVICE_H_
#define _IOT_SOCKET_DEVICE_H_

typedef void* socket_handle_t;

typedef enum {
    SOCKET_OFF = 0,
    SOCKET_ON,
} socket_status_t;

typedef enum {
    SOCKET_STA_DISCONNECTED,    
    SOCKET_CONNECTING_CLOUD,    
    SOCKET_CLOUD_CONNECTED,
} socket_net_status_t;

/**
  * @brief  socket device initilize
  *
  *
  * @return the handle of the socket
  */
socket_handle_t socket_init();

/**
  * @brief  set the state of socket
  *
  * @param  socket_handle handle of the socket
  * @param  state
  * @param  unit_id
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: pm_handle is NULL
  */
esp_err_t socket_state_write(socket_handle_t socket_handle, socket_status_t state, uint8_t unit_id);

/**
  * @brief  get the state of socket
  *
  * @param  socket_handle handle of the socket
  * @param  state_ptr the state would be returned here
  * @param  unit_id
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: pm_handle is NULL
  */
esp_err_t socket_state_read(socket_handle_t socket_handle, socket_status_t* state_ptr, uint8_t unit_id);

/**
  * @brief  set the net state of socket
  *
  * @param  socket_handle handle of the socket
  * @param  net_sta the state of net or server connect
  *
  * @return 
  *     - ESP_OK: succeed
  *     - ESP_FAIL: pm_handle is NULL
  */
esp_err_t socket_net_status_write(socket_handle_t socket_handle, socket_net_status_t net_sta);
#endif
