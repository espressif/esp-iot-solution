/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/* GPIO TE measurement and admission coordinator; composes timing and timer.
 * This service does not submit LCD pixels. */

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
/* Framework-owned context and timer wrapper, excluding SDK timer allocation. */
size_t esp_display_present_te_sync_memory(void);

/** Create TE sync. Blanking edges are auto-detected from the idle GPIO level.
 *  @p prefer_refresh_end selects the initial sync edge preference before the
 *  measured schedule takes over. */
esp_err_t esp_display_present_te_sync_create(const esp_display_present_te_sync_config_t *cfg,
                                             bool prefer_refresh_end,
                                             esp_display_present_te_sync_context_t **out_ctx);

void esp_display_present_te_sync_record_tx_start(esp_display_present_te_sync_context_t *ctx);

void esp_display_present_te_sync_record_tx_done(esp_display_present_te_sync_context_t *ctx);

void esp_display_present_te_sync_get_timing(
    const esp_display_present_te_sync_context_t *ctx,
    int64_t *out_period_us,
    int64_t *out_last_tx_us,
    int64_t *out_blanking_us);

/* Optional embedding hooks; not part of the public presenter ABI. */
typedef bool (*esp_display_present_te_wakeup_fn)(void *, bool from_isr);
void esp_display_present_te_sync_set_wakeup(esp_display_present_te_sync_context_t *, esp_display_present_te_wakeup_fn, void *);
/* timeout_ms is shared by GPIO retirement and timer callback drain. */
esp_err_t esp_display_present_te_sync_destroy_budget(esp_display_present_te_sync_context_t *, uint32_t timeout_ms);
esp_err_t esp_display_present_te_sync_destroy_checked(esp_display_present_te_sync_context_t *);
/* Nonblocking: accepts only the current measured window and an edge > after.
 * TIMEOUT leaves the job pending; GPIO and phase timer wake the backend worker again. */
esp_err_t esp_display_present_te_sync_admit(esp_display_present_te_sync_context_t *, size_t, uint64_t, uint64_t *);
void esp_display_present_te_sync_abort_tx(esp_display_present_te_sync_context_t *);

#ifdef __cplusplus
}
#endif
