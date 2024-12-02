/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_HOST_MSC_H
#define USB_HOST_MSC_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Host MSC
 *
 * @return
 *       - ESP_OK
 *       - ESP_FAIL
 */
esp_err_t host_msc_init(void);

/**
 * @brief Initialize Host MSC
 *
 * @return
 *       - ESP_OK
 *       - ESP_FAIL
 */
esp_err_t host_msc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
