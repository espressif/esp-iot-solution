/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_lv_adapter_display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_lv_adapter_te_sync_context esp_lv_adapter_te_sync_context_t;

typedef esp_err_t (*esp_lv_adapter_te_sync_prepare_tx_cb_t)(void *user_ctx, size_t transfer_index);
/* On error, all submitted transfers must be drained before returning. */
typedef esp_err_t (*esp_lv_adapter_te_sync_start_tx_cb_t)(void *user_ctx, size_t transfer_index);
typedef void (*esp_lv_adapter_te_sync_job_done_cb_t)(void *user_ctx);

typedef struct {
    size_t transfer_bytes;
    size_t transfer_count;
    esp_lv_adapter_te_sync_prepare_tx_cb_t prepare_tx;
    esp_lv_adapter_te_sync_start_tx_cb_t start_tx;
    esp_lv_adapter_te_sync_job_done_cb_t done;
    void *user_ctx;
} esp_lv_adapter_te_sync_job_t;

bool esp_lv_adapter_te_sync_is_enabled(const esp_lv_adapter_te_sync_config_t *cfg);
esp_err_t esp_lv_adapter_te_sync_create(const esp_lv_adapter_te_sync_config_t *cfg,
                                        gpio_int_type_t intr_type,
                                        bool prefer_refresh_end,
                                        esp_lv_adapter_te_sync_context_t **out_ctx);
void esp_lv_adapter_te_sync_destroy(esp_lv_adapter_te_sync_context_t *ctx);
void esp_lv_adapter_te_sync_begin_frame(esp_lv_adapter_te_sync_context_t *ctx, size_t transfer_bytes);
esp_err_t esp_lv_adapter_te_sync_wait_for_vsync(esp_lv_adapter_te_sync_context_t *ctx);
void esp_lv_adapter_te_sync_record_tx_start(esp_lv_adapter_te_sync_context_t *ctx);
void esp_lv_adapter_te_sync_record_tx_done(esp_lv_adapter_te_sync_context_t *ctx);
bool esp_lv_adapter_te_sync_is_async(const esp_lv_adapter_te_sync_context_t *ctx);
esp_err_t esp_lv_adapter_te_sync_acquire_stage(esp_lv_adapter_te_sync_context_t *ctx);
void esp_lv_adapter_te_sync_release_stage(esp_lv_adapter_te_sync_context_t *ctx);
esp_err_t esp_lv_adapter_te_sync_wait_idle(esp_lv_adapter_te_sync_context_t *ctx, int32_t timeout_ms);
esp_err_t esp_lv_adapter_te_sync_submit(esp_lv_adapter_te_sync_context_t *ctx,
                                        const esp_lv_adapter_te_sync_job_t *job);
bool esp_lv_adapter_te_sync_notify_tx_done_from_isr(esp_lv_adapter_te_sync_context_t *ctx);

#ifdef __cplusplus
}
#endif
