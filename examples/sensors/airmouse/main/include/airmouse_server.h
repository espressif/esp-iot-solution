/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "airmouse_config.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t airmouse_wifi_init_sta(void);
esp_err_t airmouse_wifi_deinit_sta(void);
esp_err_t airmouse_http_server_start(airmouse_config_t *airmouse_config);
esp_err_t airmouse_http_server_stop(void);
esp_err_t airmouse_http_server_wait_config_done(TickType_t ticks_to_wait);
void airmouse_http_runtime_notice_set(const char *mode,
                                      const char *level,
                                      const char *message,
                                      bool active);

#ifdef __cplusplus
}
#endif
