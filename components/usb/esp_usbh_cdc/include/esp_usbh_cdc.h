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
 * @brief usb receive callback type
 */
typedef void(*usbh_cdc_cb_t)(void *arg);

/**
 * @brief usb host cdc configuration type
 */
typedef struct usbh_cdc_config{
    usb_desc_ep_t *bulk_in_ep;       /*!< usb in endpoint descriptor */
    usb_desc_ep_t *bulk_out_ep;      /*!< usb out endpoint descriptor */
    int rx_buffer_size;              /*!< usb in ringbuffer size */
    int tx_buffer_size;              /*!< usb out ringbuffer size */
    usbh_cdc_cb_t rx_callback;       /*!< packet receive callback, should not block */
    void *rx_callback_arg;           /*!< packet receive callback args */
}usbh_cdc_config_t;

/**
 * @brief Install USB driver and set the USB to the default configuration.
 * 
 * @param config USB Host cdc configs
 * @return esp_err_t
 */
esp_err_t usbh_cdc_driver_install(const usbh_cdc_config_t *config);

/**
 * @brief Uninstall USB driver.
 * 
 * @return esp_err_t
 */
esp_err_t usbh_cdc_driver_delete(void);

/**
 * @brief Send data to the USB port from a given buffer and length,
 * this function will return after copying all the data to tx ring buffer.
 * 
 * @param buf data buffer address
 * @param length data length to send
 * @return int The number of bytes pushed to the TX FIFO
 */
int usbh_cdc_write_bytes(const uint8_t *buf, size_t length);

/**
 * @brief USB get RX ring buffer cached data length.
 * 
 * @param size 
 * @return esp_err_t
 */
esp_err_t usbh_cdc_get_buffered_data_len(size_t *size);

/**
 * @brief USB read bytes from USB buffer.
 * 
 * @param buf data buffer address
 * @param length data length to read
 * @param ticks_to_wait sTimeout, count in RTOS ticks
 * @return int The number of bytes read from USB FIFO
 */
int usbh_cdc_read_bytes(uint8_t *buf, size_t length, TickType_t ticks_to_wait);

/**
 * @brief print internal memory high water mark
 * @return void
 */
void usbh_cdc_print_buffer_msg(void);
