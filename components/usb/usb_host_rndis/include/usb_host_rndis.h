/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB Host Ethernet ECM Configuration
 */
typedef struct {
    uint16_t vid;                      /*!< USB device vendor ID */
    uint16_t pid;                      /*!< USB device product ID */
    uint16_t out_buffer_size;          /*!< Size of the USB OUT buffer */
    uint16_t in_buffer_size;           /*!< Size of the USB IN buffer */
    uint32_t connection_timeout_ms;    /*!< Connection timeout in milliseconds */
    void (*on_connection_cb)(bool connected); /*!< Connection status change callback */
} usb_host_rndis_config_t;

esp_err_t usbh_rndis_init(const usb_host_rndis_config_t *config);

esp_err_t usbh_rndis_create(void);

esp_err_t usbh_rndis_keepalive(void);

esp_err_t usbh_rndis_open(void);

esp_err_t usbh_rndis_close(void);

esp_err_t usbh_rndis_eth_output(void *h, void *buffer, size_t buflen);

void usbh_rndis_rx_thread(void *arg);

uint8_t *usbh_rndis_get_mac(void);

#ifdef __cplusplus
}
#endif
