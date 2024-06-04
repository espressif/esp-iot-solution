/*
 * SPDX-FileCopyrightText: 2017-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_hidh.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_BT_HID_HOST_ENABLED

esp_err_t esp_bt_hidh_init(const esp_hidh_config_t *config);
esp_err_t esp_bt_hidh_deinit(void);

esp_hidh_dev_t *esp_bt_hidh_dev_open(esp_bd_addr_t bda);

#endif /* CONFIG_BT_HID_HOST_ENABLED */

#ifdef __cplusplus
}
#endif
