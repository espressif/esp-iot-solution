/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_lv_adapter_display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_lv_adapter_te_sync_context esp_lv_adapter_te_sync_context_t;

bool esp_lv_adapter_te_sync_is_enabled(const esp_lv_adapter_te_sync_config_t *cfg);
esp_err_t esp_lv_adapter_te_sync_create(const esp_lv_adapter_te_sync_config_t *cfg,
                                        gpio_int_type_t intr_type,
                                        bool prefer_refresh_end,
                                        esp_lv_adapter_te_sync_context_t **out_ctx);
void esp_lv_adapter_te_sync_destroy(esp_lv_adapter_te_sync_context_t *ctx);
void esp_lv_adapter_te_sync_begin_frame(esp_lv_adapter_te_sync_context_t *ctx);
esp_err_t esp_lv_adapter_te_sync_wait_for_vsync(esp_lv_adapter_te_sync_context_t *ctx);
void esp_lv_adapter_te_sync_record_tx_start(esp_lv_adapter_te_sync_context_t *ctx);
void esp_lv_adapter_te_sync_record_tx_done(esp_lv_adapter_te_sync_context_t *ctx);

#ifdef __cplusplus
}
#endif
