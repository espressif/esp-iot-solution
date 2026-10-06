/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_display_present_te.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

typedef enum {
    TE_STRATEGY_UNKNOWN = 0,
    TE_STRATEGY_FAST_START,
    TE_STRATEGY_LEAD_START,
    TE_STRATEGY_SLOW_END,
    TE_STRATEGY_OVERRUN,
} te_strategy_t;

typedef struct {
    te_strategy_t strategy;
    gpio_int_type_t sync_edge;
    int64_t frame_period_us;
    int64_t blanking_us;
    int64_t active_scan_us;
    int64_t transfer_us;
    int64_t phase_begin_us;
    int64_t phase_end_us;
    int64_t blanking_reuse_us;
    bool has_phase_window;
} te_schedule_window_t;

struct esp_display_present_te_sync_context {
    esp_display_present_te_sync_config_t cfg;
    SemaphoreHandle_t te_vsync_sem;
    portMUX_TYPE lock;
    int64_t te_period_us;
    int64_t tx_start_time_us;
    int64_t last_tx_duration_us;
    int64_t estimated_tx_duration_us;
    int64_t blanking_duration_us;
    bool tx_in_progress;
    bool isr_registered;
    gpio_int_type_t blanking_start_edge;
    gpio_int_type_t blanking_end_edge;
    gpio_int_type_t sync_edge;
    te_schedule_window_t schedule;
    int64_t frame_request_time_us;
    int64_t last_te_isr_time_us;
    int64_t last_rise_time_us;
    int64_t last_fall_time_us;
    int64_t rise_period_us;
    int64_t fall_period_us;
};

static const char *TAG = "present_te";
static const uint32_t TE_WAIT_TIMEOUT_MIN_MS = 20;
static const uint32_t TE_WAIT_TIMEOUT_MAX_MS = 1000;
static const uint32_t TE_WAIT_TIMEOUT_PERIODS = 6;
/* Used only before the first same-edge period is measured (~60 Hz). */
static const uint64_t TE_WAIT_TIMEOUT_DEFAULT_PERIOD_US = 16667ULL;

static const char *te_edge_name(gpio_int_type_t edge)
{
    return edge == GPIO_INTR_POSEDGE ? "rise" : "fall";
}

static const char *te_strategy_name(te_strategy_t strategy)
{
    switch (strategy) {
    case TE_STRATEGY_FAST_START:
        return "fast/start";
    case TE_STRATEGY_LEAD_START:
        return "lead/start";
    case TE_STRATEGY_SLOW_END:
        return "slow/end";
    case TE_STRATEGY_OVERRUN:
        return "overrun/end";
    default:
        return "unknown";
    }
}

static uint16_t te_window_required_percent(int64_t duration_us,
                                           int64_t period_us)
{
    if (duration_us <= 0 || period_us <= 0) {
        return 0;
    }
    uint64_t required =
        (uint64_t)(duration_us * 100ULL + period_us - 1ULL) / period_us;
    if (required > UINT16_MAX) {
        required = UINT16_MAX;
    }
    return (uint16_t)required;
}

static uint32_t te_wait_timeout_ms(int64_t te_period_us)
{
    uint64_t period_us = te_period_us > 0
                         ? (uint64_t)te_period_us
                         : TE_WAIT_TIMEOUT_DEFAULT_PERIOD_US;

    uint64_t timeout_ms = (period_us * TE_WAIT_TIMEOUT_PERIODS + 999ULL) / 1000ULL;
    if (timeout_ms < TE_WAIT_TIMEOUT_MIN_MS) {
        timeout_ms = TE_WAIT_TIMEOUT_MIN_MS;
    }
    if (timeout_ms > TE_WAIT_TIMEOUT_MAX_MS) {
        timeout_ms = TE_WAIT_TIMEOUT_MAX_MS;
    }

    return (uint32_t)timeout_ms;
}

/* Derive the legal start window from measured timing.
 *
 * Both the panel scan head (S) and GRAM write head (W) move top-to-bottom.
 * Let Tf be the same-edge frame period measured from TE, Tb the blanking time,
 * Ta = Tf - Tb the active scan time, Ttx the full-frame transfer time, and p
 * the elapsed active-scan phase from blanking_end/scan_start.
 *
 * A. Send starts first, scan starts later. The completion deadline is the end
 *    of that scan. At blanking_start the available time is exactly one frame:
 *    Ttx <= Tb + Ta = Tf. If starting e into blanking: e + Ttx <= Tf.
 *
 *       W:   |------------ Ttx ------------>|
 *       S:         |---------------- Ta ---------------->|
 *            blank ^ scan_start              ^ scan_end/deadline
 *
 * B. Scan starts first and W is faster (Ttx <= Ta). W must not catch S in
 *    this scan. Therefore completion must be at/after this scan_end:
 *    p + Ttx >= Ta, giving the safe late window p >= Ta - Ttx.
 *
 *       scan: |-------------- Ta --------------|
 *       start:|------ unsafe ------|<- safe ----|
 *                                  p >= Ta-Ttx
 *
 * C. Scan starts first and W is slower (Ttx > Ta). S stays ahead, but transfer
 *    must complete by the following scan_end: p + Ttx <= Tf + Ta, giving the
 *    safe early window p <= Tf + Ta - Ttx.
 *
 *       scan: |-------------- Ta --------------|
 *       start:|<---- safe ---->|------ unsafe --|
 *              p <= Tf+Ta-Ttx
 *
 * Ttx >= Tf+Ta has no provably safe full-frame phase. The schedule still uses
 * scan_start as best effort and reports overrun. When Ta < Ttx <= Tf both A
 * and C are legal: blanking_start is preferred, with C retained as a fallback. */
static te_schedule_window_t te_derive_schedule(
    int64_t frame_period_us, int64_t transfer_us, int64_t blanking_us,
    gpio_int_type_t blanking_start_edge, gpio_int_type_t blanking_end_edge)
{
    te_schedule_window_t out = {
        .strategy = TE_STRATEGY_UNKNOWN,
        .sync_edge = blanking_start_edge,
        .frame_period_us = frame_period_us,
        .blanking_us = blanking_us,
        .transfer_us = transfer_us,
    };
    if (frame_period_us <= 0 || transfer_us <= 0) {
        return out;
    }

    if (blanking_us < 0 || blanking_us >= frame_period_us) {
        blanking_us = 0;
        out.blanking_us = 0;
    }
    const int64_t active_scan_us = frame_period_us - blanking_us;
    out.active_scan_us = active_scan_us;

    if (transfer_us <= active_scan_us) {
        out.strategy = TE_STRATEGY_FAST_START;
        out.sync_edge = blanking_start_edge;
        out.phase_begin_us = active_scan_us - transfer_us;
        out.phase_end_us = active_scan_us;
        out.blanking_reuse_us = blanking_us;
        out.has_phase_window = transfer_us > 0;
    } else if (transfer_us <= frame_period_us) {
        out.strategy = TE_STRATEGY_LEAD_START;
        out.sync_edge = blanking_start_edge;
        int64_t width_us = frame_period_us + active_scan_us - transfer_us;
        out.phase_begin_us = 0;
        out.phase_end_us = width_us;
        out.blanking_reuse_us = frame_period_us - transfer_us;
        out.has_phase_window = width_us > 0;
    } else if (transfer_us < frame_period_us + active_scan_us) {
        out.strategy = TE_STRATEGY_SLOW_END;
        out.sync_edge = blanking_end_edge;
        int64_t width_us = frame_period_us + active_scan_us - transfer_us;
        out.phase_begin_us = 0;
        out.phase_end_us = width_us;
        out.has_phase_window = width_us > 0;
    } else {
        out.strategy = TE_STRATEGY_OVERRUN;
        out.sync_edge = blanking_end_edge;
    }
    return out;
}

/* Decide whether a frame may start now using the already-derived schedule.
 *
 * A newly observed preferred edge is always accepted. Otherwise a request may
 * reuse the remainder of blanking or the active-scan phase interval above. */
static bool te_current_phase_is_usable_locked(
    esp_display_present_te_sync_context_t *ctx, int64_t now_us)
{
    const int64_t sync_us = ctx->sync_edge == GPIO_INTR_POSEDGE
                            ? ctx->last_rise_time_us
                            : ctx->last_fall_time_us;
    if (sync_us > 0 && sync_us >= ctx->frame_request_time_us) {
        return true;
    }
    const te_schedule_window_t *schedule = &ctx->schedule;
    if (schedule->strategy == TE_STRATEGY_UNKNOWN) {
        return false;
    }

    const int64_t scan_start_us = ctx->blanking_end_edge == GPIO_INTR_POSEDGE
                                  ? ctx->last_rise_time_us
                                  : ctx->last_fall_time_us;
    const int64_t blanking_start_us =
        ctx->blanking_start_edge == GPIO_INTR_POSEDGE
        ? ctx->last_rise_time_us : ctx->last_fall_time_us;
    if (scan_start_us <= 0 || now_us < scan_start_us) {
        return false;
    }

    /* The most recent blanking_start is newer than scan_start only while the
     * panel is currently in blanking. Keep its completion before deadline A. */
    if (schedule->blanking_reuse_us > 0 &&
            blanking_start_us > scan_start_us && now_us >= blanking_start_us &&
            now_us - blanking_start_us <= schedule->blanking_reuse_us) {
        return true;
    }

    const int64_t phase_us = now_us - scan_start_us;
    if (phase_us < 0 || phase_us >= schedule->active_scan_us) {
        return false;
    }
    if (schedule->has_phase_window &&
            phase_us >= schedule->phase_begin_us &&
            phase_us <= schedule->phase_end_us) {
        return true;
    }
    return false;
}

static int64_t te_estimate_transfer_time_us(const esp_display_present_te_sync_context_t *ctx, size_t transfer_bytes)
{
    if (!ctx || transfer_bytes == 0 || ctx->cfg.bus_freq_hz == 0) {
        return 0;
    }

    uint32_t data_lines = ctx->cfg.data_lines ? ctx->cfg.data_lines : ESP_DISPLAY_PRESENT_TE_DATA_LINES_DEFAULT;
    uint64_t bus_bits_per_sec = (uint64_t)ctx->cfg.bus_freq_hz * data_lines;
    if (bus_bits_per_sec == 0) {
        return 0;
    }

    uint64_t transfer_bits = (uint64_t)transfer_bytes * 8ULL;
    return (int64_t)((transfer_bits * 1000000ULL + bus_bits_per_sec - 1ULL) / bus_bits_per_sec);
}

static void IRAM_ATTR te_gpio_isr(void *arg)
{
    esp_display_present_te_sync_context_t *ctx = (esp_display_present_te_sync_context_t *)arg;
    if (!ctx || !ctx->te_vsync_sem) {
        return;
    }

    const int level = gpio_get_level(ctx->cfg.gpio_num);
    const int64_t now_us = esp_timer_get_time();
    BaseType_t need_yield = pdFALSE;
    portENTER_CRITICAL_ISR(&ctx->lock);
    if (level != 0) {
        if (ctx->last_rise_time_us > 0) {
            ctx->rise_period_us = now_us - ctx->last_rise_time_us;
        }
        ctx->last_rise_time_us = now_us;
    } else {
        if (ctx->last_fall_time_us > 0) {
            ctx->fall_period_us = now_us - ctx->last_fall_time_us;
        }
        ctx->last_fall_time_us = now_us;
    }
    const bool blanking_end =
        (ctx->blanking_end_edge == GPIO_INTR_POSEDGE && level != 0) ||
        (ctx->blanking_end_edge == GPIO_INTR_NEGEDGE && level == 0);
    if (blanking_end) {
        const int64_t blanking_start_us =
            ctx->blanking_start_edge == GPIO_INTR_POSEDGE
            ? ctx->last_rise_time_us : ctx->last_fall_time_us;
        if (blanking_start_us > 0 && now_us > blanking_start_us) {
            ctx->blanking_duration_us = now_us - blanking_start_us;
        }
    }
    const bool selected_edge =
        (ctx->sync_edge == GPIO_INTR_POSEDGE && level != 0) ||
        (ctx->sync_edge == GPIO_INTR_NEGEDGE && level == 0);
    if (selected_edge) {
        const int64_t edge_period = level != 0
                                    ? ctx->rise_period_us
                                    : ctx->fall_period_us;
        if (edge_period > 0) {
            ctx->te_period_us = edge_period;
        }
        ctx->last_te_isr_time_us = now_us;
    }
    portEXIT_CRITICAL_ISR(&ctx->lock);

    if (selected_edge) {
        xSemaphoreGiveFromISR(ctx->te_vsync_sem, &need_yield);
    }
    if (need_yield) {
        portYIELD_FROM_ISR();
    }
}

static gpio_int_type_t detect_blanking_start_edge(
    const esp_display_present_te_sync_config_t *cfg)
{
    int idle_level = gpio_get_level(cfg->gpio_num);
    bool idle_high = (idle_level == 1);
    gpio_int_type_t start_edge = idle_high ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;

    ESP_LOGI(TAG, "TE blanking start auto-detected: gpio=%d idle=%d edge=%s",
             cfg->gpio_num, idle_level,
             (start_edge == GPIO_INTR_NEGEDGE) ? "falling" : "rising");

    return start_edge;
}

bool esp_display_present_te_sync_is_enabled(const esp_display_present_te_sync_config_t *cfg)
{
    return cfg && cfg->gpio_num >= 0;
}

esp_err_t esp_display_present_te_sync_create(const esp_display_present_te_sync_config_t *cfg,
                                             bool prefer_refresh_end,
                                             esp_display_present_te_sync_context_t **out_ctx)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(out_ctx, ESP_ERR_INVALID_ARG, TAG, "output context is NULL");
    *out_ctx = NULL;
    ESP_RETURN_ON_FALSE(esp_display_present_te_sync_is_enabled(cfg), ESP_ERR_INVALID_ARG, TAG, "invalid TE config");

    esp_display_present_te_sync_context_t *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "no memory for TE context");

    ctx->cfg = *cfg;
    ctx->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    ctx->blanking_start_edge = detect_blanking_start_edge(cfg);
    ctx->blanking_end_edge =
        ctx->blanking_start_edge == GPIO_INTR_POSEDGE
        ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
    ctx->sync_edge = prefer_refresh_end
                     ? ctx->blanking_end_edge : ctx->blanking_start_edge;

    ctx->te_vsync_sem = xSemaphoreCreateCounting(1, 0);
    ESP_GOTO_ON_FALSE(ctx->te_vsync_sem, ESP_ERR_NO_MEM, fail, TAG, "failed to create TE semaphore");

    const gpio_config_t gpio_cfg = {
        /* Capture both levels for timing diagnostics.  The ISR only releases
         * the VSYNC waiter for the configured synchronization edge. */
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << ctx->cfg.gpio_num,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&gpio_cfg), fail, TAG, "gpio config failed");

    ret = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_GOTO_ON_ERROR(ret, fail, TAG, "install ISR service failed");
    }

    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(ctx->cfg.gpio_num, te_gpio_isr, ctx), fail, TAG, "add TE ISR failed");
    ctx->isr_registered = true;

    *out_ctx = ctx;
    ESP_LOGI(TAG,
             "TE sync ready: gpio=%d blanking=%s..%s sync=%s irq=both",
             ctx->cfg.gpio_num,
             te_edge_name(ctx->blanking_start_edge),
             te_edge_name(ctx->blanking_end_edge),
             te_edge_name(ctx->sync_edge));
    return ESP_OK;

fail:
    if (ctx->isr_registered) {
        gpio_isr_handler_remove(ctx->cfg.gpio_num);
    }
    if (ctx->te_vsync_sem) {
        vSemaphoreDelete(ctx->te_vsync_sem);
    }
    free(ctx);
    ESP_LOGE(TAG, "TE sync create failed: %s", esp_err_to_name(ret));
    return ret;
}

void esp_display_present_te_sync_destroy(esp_display_present_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->isr_registered) {
        gpio_isr_handler_remove(ctx->cfg.gpio_num);
    }
    if (ctx->te_vsync_sem) {
        vSemaphoreDelete(ctx->te_vsync_sem);
    }
    ESP_LOGI(TAG, "TE sync destroyed: gpio=%d", ctx->cfg.gpio_num);
    free(ctx);
}

void esp_display_present_te_sync_begin_frame(esp_display_present_te_sync_context_t *ctx, size_t transfer_bytes)
{
    if (!ctx) {
        return;
    }

    bool strategy_changed = false;
    te_schedule_window_t schedule = { 0 };
    int64_t duration_us = 0;
    int64_t period_us = 0;

    portENTER_CRITICAL(&ctx->lock);
    ctx->frame_request_time_us = esp_timer_get_time();
    ctx->estimated_tx_duration_us = te_estimate_transfer_time_us(ctx, transfer_bytes);
    duration_us = ctx->estimated_tx_duration_us;
    if (ctx->last_tx_duration_us > duration_us) {
        duration_us = ctx->last_tx_duration_us;
    }
    period_us = ctx->te_period_us;
    schedule = te_derive_schedule(
                   period_us, duration_us, ctx->blanking_duration_us,
                   ctx->blanking_start_edge, ctx->blanking_end_edge);
    if (schedule.strategy != TE_STRATEGY_UNKNOWN) {
        /* Exact measured bounds naturally jitter by a few microseconds. Keep
         * the event log for actual mode/edge changes; sampled frame logs below
         * carry the continuously updated numeric window. */
        strategy_changed = schedule.strategy != ctx->schedule.strategy ||
                           schedule.sync_edge != ctx->schedule.sync_edge;
        ctx->schedule = schedule;
        ctx->sync_edge = schedule.sync_edge;
    }
    portEXIT_CRITICAL(&ctx->lock);

    if (strategy_changed) {
        const uint16_t required =
            te_window_required_percent(duration_us, period_us);
        if (schedule.strategy == TE_STRATEGY_OVERRUN) {
            ESP_LOGW(TAG,
                     "TE schedule=%s edge=%s Tf/Tb/Ta/Ttx=%lld/%lld/%lld/%lld required=%u%%; no safe full-frame window",
                     te_strategy_name(schedule.strategy),
                     te_edge_name(schedule.sync_edge),
                     (long long)schedule.frame_period_us,
                     (long long)schedule.blanking_us,
                     (long long)schedule.active_scan_us,
                     (long long)schedule.transfer_us, required);
        } else {
            ESP_LOGI(TAG,
                     "TE schedule=%s edge=%s Tf/Tb/Ta/Ttx=%lld/%lld/%lld/%lld phase=[%lld,%lld] blank_reuse=%lld required=%u%%",
                     te_strategy_name(schedule.strategy),
                     te_edge_name(schedule.sync_edge),
                     (long long)schedule.frame_period_us,
                     (long long)schedule.blanking_us,
                     (long long)schedule.active_scan_us,
                     (long long)schedule.transfer_us,
                     (long long)schedule.phase_begin_us,
                     (long long)schedule.phase_end_us,
                     (long long)schedule.blanking_reuse_us,
                     required);
        }
    }

    /* Drop a queued wakeup. The edge timestamp remains available so the
     * current-period phase window can still be reused deliberately. */
    (void)xSemaphoreTake(ctx->te_vsync_sem, 0);
}

esp_err_t esp_display_present_te_sync_wait_for_vsync(esp_display_present_te_sync_context_t *ctx)
{
    if (!ctx || !ctx->te_vsync_sem) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t period_us = 0;
    portENTER_CRITICAL(&ctx->lock);
    period_us = ctx->te_period_us;
    portEXIT_CRITICAL(&ctx->lock);
    const TickType_t timeout_ticks = pdMS_TO_TICKS(te_wait_timeout_ms(period_us));

    while (true) {
        bool accept = false;
        int64_t next_phase_us = 0;
        portENTER_CRITICAL(&ctx->lock);
        const int64_t checked_us = esp_timer_get_time();
        accept = te_current_phase_is_usable_locked(ctx, checked_us);
        const te_schedule_window_t *schedule = &ctx->schedule;
        const int64_t scan_start_us = ctx->blanking_end_edge == GPIO_INTR_POSEDGE
                                      ? ctx->last_rise_time_us : ctx->last_fall_time_us;
        if (!accept && schedule->has_phase_window && scan_start_us > 0) {
            const int64_t phase_begin_us = scan_start_us + schedule->phase_begin_us;
            if (phase_begin_us > checked_us) {
                next_phase_us = phase_begin_us;
            }
        }
        portEXIT_CRITICAL(&ctx->lock);
        if (accept) {
            return ESP_OK;
        }

        const int64_t now_us = esp_timer_get_time();
        if (next_phase_us > 0 && next_phase_us <= now_us) {
            continue;
        }
        TickType_t wait_ticks = timeout_ticks;
        bool phase_wakeup = false;
        if (next_phase_us > now_us) {
            const int64_t tick_us = (int64_t)portTICK_PERIOD_MS * 1000;
            const TickType_t phase_ticks =
                (TickType_t)((next_phase_us - now_us + tick_us - 1) / tick_us);
            if (phase_ticks > 0 && phase_ticks < wait_ticks) {
                wait_ticks = phase_ticks;
                phase_wakeup = true;
            }
        }
        if (xSemaphoreTake(ctx->te_vsync_sem, wait_ticks) != pdTRUE) {
            /* An optional current-period phase wakeup is not a missing TE
             * edge. Re-check the original admission predicate; an early
             * tick or an expired window never authorizes a transfer. */
            if (phase_wakeup) {
                continue;
            }
            ESP_LOGW(TAG,
                     "TE wait timeout: gpio=%d timeout_ticks=%u period=%lldus last_tx=%lldus",
                     ctx->cfg.gpio_num, (unsigned)timeout_ticks,
                     (long long)ctx->te_period_us,
                     (long long)ctx->last_tx_duration_us);
            return ESP_ERR_TIMEOUT;
        }

        accept = true;
        portENTER_CRITICAL(&ctx->lock);
        period_us = ctx->te_period_us;

        /* A queued edge may belong to the previous strategy. Validate its
         * timestamp against the currently selected edge and phase window. */
        if (period_us > 0) {
            accept = te_current_phase_is_usable_locked(ctx, esp_timer_get_time());
        } else if (ctx->last_te_isr_time_us < ctx->frame_request_time_us) {
            accept = false;
        }
        portEXIT_CRITICAL(&ctx->lock);

        if (accept) {
            return ESP_OK;
        }
    }
}

void esp_display_present_te_sync_record_tx_start(esp_display_present_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    portENTER_CRITICAL(&ctx->lock);
    const int64_t now_us = esp_timer_get_time();
    ctx->tx_start_time_us = now_us;
    ctx->tx_in_progress = true;
    portEXIT_CRITICAL(&ctx->lock);
}

void IRAM_ATTR esp_display_present_te_sync_record_tx_done(esp_display_present_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&ctx->lock);
    if (ctx->tx_in_progress && ctx->tx_start_time_us > 0) {
        ctx->last_tx_duration_us = now_us - ctx->tx_start_time_us;
    }
    ctx->tx_in_progress = false;
    portEXIT_CRITICAL_ISR(&ctx->lock);
}

void esp_display_present_te_sync_get_timing(
    const esp_display_present_te_sync_context_t *ctx,
    int64_t *out_period_us,
    int64_t *out_last_tx_us,
    int64_t *out_blanking_us)
{
    if (out_period_us != NULL) {
        *out_period_us = 0;
    }
    if (out_last_tx_us != NULL) {
        *out_last_tx_us = 0;
    }
    if (out_blanking_us != NULL) {
        *out_blanking_us = 0;
    }
    if (ctx == NULL) {
        return;
    }
    portENTER_CRITICAL((portMUX_TYPE *)&ctx->lock);
    if (out_period_us != NULL) {
        *out_period_us = ctx->te_period_us;
    }
    if (out_last_tx_us != NULL) {
        *out_last_tx_us = ctx->last_tx_duration_us;
    }
    if (out_blanking_us != NULL) {
        *out_blanking_us = ctx->blanking_duration_us;
    }
    portEXIT_CRITICAL((portMUX_TYPE *)&ctx->lock);
}
