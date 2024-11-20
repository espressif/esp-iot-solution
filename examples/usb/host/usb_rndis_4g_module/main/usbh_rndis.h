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

esp_err_t usbh_rndis_init(void);

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
