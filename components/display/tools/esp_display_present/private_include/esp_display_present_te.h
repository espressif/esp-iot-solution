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
#include "esp_display_present_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_DISPLAY_PRESENT_TE_DATA_LINES_DEFAULT         1

typedef struct esp_display_present_te_sync_context esp_display_present_te_sync_context_t;

bool esp_display_present_te_sync_is_enabled(const esp_display_present_te_sync_config_t *cfg);

/** Create TE sync. Blanking edges are auto-detected from the idle GPIO level.
 *  @p prefer_refresh_end selects the initial sync edge preference before the
 *  measured schedule takes over. */
esp_err_t esp_display_present_te_sync_create(const esp_display_present_te_sync_config_t *cfg,
                                             bool prefer_refresh_end,
                                             esp_display_present_te_sync_context_t **out_ctx);

void esp_display_present_te_sync_destroy(esp_display_present_te_sync_context_t *ctx);

void esp_display_present_te_sync_begin_frame(esp_display_present_te_sync_context_t *ctx, size_t transfer_bytes);

esp_err_t esp_display_present_te_sync_wait_for_vsync(esp_display_present_te_sync_context_t *ctx);

void esp_display_present_te_sync_record_tx_start(esp_display_present_te_sync_context_t *ctx);

void esp_display_present_te_sync_record_tx_done(esp_display_present_te_sync_context_t *ctx);

void esp_display_present_te_sync_get_timing(
    const esp_display_present_te_sync_context_t *ctx,
    int64_t *out_period_us,
    int64_t *out_last_tx_us,
    int64_t *out_blanking_us);

#ifdef __cplusplus
}
#endif
