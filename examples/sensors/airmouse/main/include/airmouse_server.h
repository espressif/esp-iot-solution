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
esp_err_t airmouse_http_server_wait_config_submitted(TickType_t ticks_to_wait);
esp_err_t airmouse_http_server_wait_config_done(TickType_t ticks_to_wait);
esp_err_t airmouse_http_server_wait_runtime_calibration_start(TickType_t ticks_to_wait);
void airmouse_http_runtime_notice_set(const char *mode,
                                      const char *level,
                                      const char *message,
                                      bool active);
void airmouse_http_refresh_calibration_notice_state(void);
void airmouse_http_set_calibration_notice_enabled(bool enabled);
void airmouse_http_set_initial_calibration_flow(bool enabled);
void airmouse_http_prepare_runtime_calibration(const char *kind, uint32_t countdown_seconds);
void airmouse_http_finish_runtime_calibration(void);
void airmouse_http_signal_config_done(void);

#ifdef __cplusplus
}
#endif
