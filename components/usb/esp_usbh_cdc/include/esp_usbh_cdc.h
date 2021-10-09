// Copyright 2019-2021 Espressif Systems (Shanghai) PTE LTD
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

#pragma once
#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "hal/usbh_ll.h"
#include "hcd.h"

/**
 * @brief USB receive callback type
 */
typedef void(*usbh_cdc_cb_t)(void *arg);

/**
 * @brief USB host CDC configuration type, callbacks should not in block state
 */
typedef struct usbh_cdc_config{
    uint8_t bulk_in_ep_addr;         /*!< USB CDC bulk in endpoint address, will be overwritten if bulk_in_ep is specified */
    uint8_t bulk_out_ep_addr;        /*!< USB CDC bulk out endpoint address, will be overwritten if bulk_out_ep is specified */
    int rx_buffer_size;              /*!< USB receive/in ringbuffer size */
    int tx_buffer_size;              /*!< USB transport/out ringbuffer size */
    usb_ep_desc_t *bulk_in_ep;       /*!< USB CDC bulk in endpoint descriptor, set NULL if using default param */
    usb_ep_desc_t *bulk_out_ep;      /*!< USB CDC bulk out endpoint descriptor, set NULL if using default param */
    usbh_cdc_cb_t conn_callback;     /*!< USB connect calllback, set NULL if not use */
    usbh_cdc_cb_t disconn_callback;  /*!< USB disconnect calllback, usb cdc driver reset to connect waitting after disconnect, set NULL if not use */
    usbh_cdc_cb_t rx_callback;       /*!< packet receive callback, set NULL if not use */
    void *conn_callback_arg;         /*!< USB connect calllback args, set NULL if not use  */
    void *disconn_callback_arg;      /*!< USB disconnect calllback args, set NULL if not use */
    void *rx_callback_arg;           /*!< packet receive callback args, set NULL if not use */
}usbh_cdc_config_t;

/**
 * @brief Install USB CDC driver.
 * 
 * @param config USB Host CDC configs
 * @return
 *         ESP_ERR_INVALID_STATE driver has been installed 
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_FAIL driver install failed
 *         ESP_OK driver install succeed
 */
esp_err_t usbh_cdc_driver_install(const usbh_cdc_config_t *config);

/**
 * @brief Uninstall USB driver.
 * 
 * @return
 *         ESP_ERR_INVALID_STATE driver not installed 
 *         ESP_OK start succeed
 */
esp_err_t usbh_cdc_driver_delete(void);

/**
 * @brief Waitting until CDC device connect
 * 
 * @param ticks_to_wait Wait timeout value, count in RTOS ticks
 * @return
 *         ESP_ERR_INVALID_STATE driver not installed 
 *         ESP_ERR_TIMEOUT wait timeout
 *         ESP_OK device connect succeed 
 */
esp_err_t usbh_cdc_wait_connect(TickType_t ticks_to_wait);

/**
 * @brief Send data to connected USB device from a given buffer and length,
 * this function will return after copying all the data to tx ring buffer.
 * 
 * @param buf data buffer address
 * @param length data length to send
 * @return int The number of bytes pushed to the tx buffer
 */
int usbh_cdc_write_bytes(const uint8_t *buf, size_t length);

/**
 * @brief Get USB receive ring buffer cached data length.
 * 
 * @param size 
 * @return
 *         ESP_ERR_INVALID_STATE cdc not configured, or not running 
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_FAIL start failed
 *         ESP_OK start succeed
 */
esp_err_t usbh_cdc_get_buffered_data_len(size_t *size);

/**
 * @brief Read data bytes from USB receive/in buffer.
 * 
 * @param buf data buffer address
 * @param length data length to read
 * @param ticks_to_wait sTimeout, count in RTOS ticks
 * @return int The number of bytes read from USB FIFO
 */
int usbh_cdc_read_bytes(uint8_t *buf, size_t length, TickType_t ticks_to_wait);

/**
 * @brief print internal memory usage for debug
 * @return void
 */
void usbh_cdc_print_buffer_msg(void);
