/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

void app_wifi_main(void);

esp_err_t app_wifi_set_wifi(char *mode, char *ap_ssid, char *ap_passwd, char *sta_ssid, char *sta_passwd);

#ifdef __cplusplus
}
#endif
