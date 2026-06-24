/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_web_start(void);
bool app_web_has_client(void);
esp_err_t app_web_broadcast_state(void);
esp_err_t app_web_send_error(const char *code, const char *message);
esp_err_t app_web_send_mic_pcm(const uint8_t *data, size_t len);
esp_err_t app_web_send_spk_refill_budget_bytes(uint32_t bytes);

#ifdef __cplusplus
}
#endif
