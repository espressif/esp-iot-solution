/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "iot_usbh.h"

#if (CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3)
#define CDC_INTERFACE_NUM_MAX                3                /*!< Support 3 cdc interface at most (each only contains two bulk channel) */
#else
#define CDC_INTERFACE_NUM_MAX                0
#error "USB Host CDC not supported"
#endif

/**
 * @brief USB receive callback type
 */
typedef void(*usbh_cdc_cb_t)(void *arg);

/**
 * @brief USB host CDC configuration type
 * itf_num is the total enabled interface numbers, can not exceed CDC_INTERFACE_NUM_MAX,
 * itf_num = 0 is same as itf_num = 1 for back compatibility,
 * if itf_num > 1, user should config params with `s` ending like `bulk_in_ep_addrs` to config each interface,
 * callbacks should not in block state.
 */
typedef struct usbh_cdc_config {
    int itf_num;                                                /*!< interface numbers enabled */
    union {
        uint8_t bulk_in_ep_addr;                                /*!< USB CDC bulk in endpoint address, will be overwritten if bulk_in_ep is specified */
        uint8_t bulk_in_ep_addrs[CDC_INTERFACE_NUM_MAX];        /*!< USB CDC bulk in endpoints addresses, saved as an array, will be overwritten if bulk_in_ep is specified */
    };
    union {
        uint8_t bulk_out_ep_addr;                               /*!< USB CDC bulk out endpoint address, will be overwritten if bulk_out_ep is specified */
        uint8_t bulk_out_ep_addrs[CDC_INTERFACE_NUM_MAX];       /*!< USB CDC bulk out endpoint addresses, saved as an array, will be overwritten if bulk_out_ep is specified */
    };
    union {
        int rx_buffer_size;                                     /*!< USB receive/in ringbuffer size */
        int rx_buffer_sizes[CDC_INTERFACE_NUM_MAX];             /*!< USB receive/in ringbuffer size of each interface */
    };
    union {
        int tx_buffer_size;                                     /*!< USB transmit/out ringbuffer size */
        int tx_buffer_sizes[CDC_INTERFACE_NUM_MAX];             /*!< USB transmit/out ringbuffer size of each interface */
    };
    union {
        usb_ep_desc_t *bulk_in_ep;                              /*!< USB CDC bulk in endpoint descriptor, set NULL if using default param */
        usb_ep_desc_t *bulk_in_eps[CDC_INTERFACE_NUM_MAX];      /*!< USB CDC bulk in endpoint descriptors of each interface, set NULL if using default param */
    };
    union {
        usb_ep_desc_t *bulk_out_ep;                             /*!< USB CDC bulk out endpoint descriptor, set NULL if using default param */
        usb_ep_desc_t *bulk_out_eps[CDC_INTERFACE_NUM_MAX];     /*!< USB CDC bulk out endpoint descriptors of each interface, set NULL if using default param */
    };
    usbh_cdc_cb_t conn_callback;                                /*!< USB connect callback, set NULL if not use */
    usbh_cdc_cb_t disconn_callback;                             /*!< USB disconnect callback, set NULL if not use */
    union {
        usbh_cdc_cb_t rx_callback;                              /*!< packet receive callback, set NULL if not use */
        usbh_cdc_cb_t rx_callbacks[CDC_INTERFACE_NUM_MAX];      /*!< packet receive callbacks of each interface, set NULL if not use */
    };
    void *conn_callback_arg;                                    /*!< USB connect callback arg, set NULL if not use  */
    void *disconn_callback_arg;                                 /*!< USB disconnect callback arg, set NULL if not use */
    union {
        void *rx_callback_arg;                                  /*!< packet receive callback arg, set NULL if not use */
        void *rx_callback_args[CDC_INTERFACE_NUM_MAX];          /*!< packet receive callback arg of each interface, set NULL if not use */
    };
} usbh_cdc_config_t;

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
 * @brief Waiting until CDC device connected
 *
 * @param ticks_to_wait timeout value, count in RTOS ticks
 * @return
 *         ESP_ERR_INVALID_STATE driver not installed
 *         ESP_ERR_TIMEOUT wait timeout
 *         ESP_OK device connect succeed
 */
esp_err_t usbh_cdc_wait_connect(TickType_t ticks_to_wait);

/**
 * @brief Send data to interface 0 of connected USB device from a given buffer and length,
 * this function will return after copying all the data to tx ringbuffer.
 *
 * @param buf data buffer address
 * @param length data length to send
 * @return int The number of bytes pushed to the tx buffer
 */
int usbh_cdc_write_bytes(const uint8_t *buf, size_t length);

/**
 * @brief Send data to specified interface of connected USB device from a given buffer and length,
 * this function will return after copying all the data to tx buffer.
 *
 * @param itf the interface index
 * @param buf data buffer address
 * @param length data length to send
 * @return int The number of bytes pushed to the tx buffer
 */
int usbh_cdc_itf_write_bytes(uint8_t itf, const uint8_t *buf, size_t length);

/**
 * @brief Get USB interface 0 rx buffered data length.
 *
 * @param size data length buffered
 * @return
 *         ESP_ERR_INVALID_STATE cdc not configured, or not running
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_OK start succeed
 */
esp_err_t usbh_cdc_get_buffered_data_len(size_t *size);

/**
 * @brief Get USB specified interface rx buffered data length.
 *
 * @param itf the interface index
 * @param size data length buffered
 * @return
 *         ESP_ERR_INVALID_STATE cdc not configured, or not running
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_OK start succeed
 */
esp_err_t usbh_cdc_itf_get_buffered_data_len(uint8_t itf, size_t *size);

/**
 * @brief Read data bytes from interface 0 rx buffer.
 *
 * @param buf data buffer address
 * @param length data buffer size
 * @param ticks_to_wait timeout value, count in RTOS ticks
 * @return int the number of bytes actually read from rx buffer
 */
int usbh_cdc_read_bytes(uint8_t *buf, size_t length, TickType_t ticks_to_wait);

/**
 * @brief Read data bytes from specified interface rx buffer.
 *
 * @param itf the interface index
 * @param buf data buffer address
 * @param length data buffer size
 * @param ticks_to_wait timeout value, count in RTOS ticks
 * @return int the number of bytes actually read from rx buffer
 */
int usbh_cdc_itf_read_bytes(uint8_t itf, uint8_t *buf, size_t length, TickType_t ticks_to_wait);

/**
 * @brief Get the connect state of given interface
 *
 * @param itf the interface index
 * @return ** int true is ready, false is not ready
 */
int usbh_cdc_get_itf_state(uint8_t itf);

/**
 * @brief Flush rx buffer, discard all the data in the ring-buffer.
 *
 * @param itf the interface index
 * @return ** esp_err_t
 *         ESP_ERR_INVALID_STATE cdc not configured, or not running
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_OK start succeed
 */
esp_err_t usbh_cdc_flush_rx_buffer(uint8_t itf);

/**
 * @brief Flush tx buffer, discard all the data in the ring-buffer.
 *
 * @param itf the interface index
 * @return ** esp_err_t
 *         ESP_ERR_INVALID_STATE cdc not configured, or not running
 *         ESP_ERR_INVALID_ARG args not supported
 *         ESP_OK start succeed
 */
esp_err_t usbh_cdc_flush_tx_buffer(uint8_t itf);
