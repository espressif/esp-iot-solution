/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "platform/te/present_te_sync.h"
#include "esp_intr_alloc.h"
#include "platform/present_os.h"
#include "platform/te/present_te_timing.h"
#include "platform/te/present_te_timer.h"
#include "backend/present_records.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* esp_timer_stop_blocking() preserves callback argument lifetime at teardown. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 2, 0)
#define PRESENT_TE_PHASE_TIMER 1
#else
#define PRESENT_TE_PHASE_TIMER 0
#endif

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
#if PRESENT_TE_PHASE_TIMER
    present_te_timer_t *phase_timer;
#endif
    portMUX_TYPE lock;
    int64_t te_period_us;
    int64_t tx_start_time_us;
    int64_t last_tx_duration_us;
    int64_t estimated_tx_duration_us;
    int64_t blanking_duration_us;
    bool tx_in_progress;
    bool isr_registered, isr_removed;
    esp_display_present_te_wakeup_fn wakeup;
    void *wakeup_user;
    gpio_int_type_t blanking_start_edge;
    gpio_int_type_t blanking_end_edge;
    gpio_int_type_t sync_edge;
    te_schedule_window_t schedule;
    int64_t last_rise_time_us;
    int64_t last_fall_time_us;
    int64_t rise_period_us;
    int64_t fall_period_us;
};

static const char *TAG = "present_te";
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
    uint64_t period = (uint64_t)period_us;
    uint64_t duration = (uint64_t)duration_us;
    uint64_t whole = duration / period;
    if (whole > UINT16_MAX / 100) {
        return UINT16_MAX;
    }
    /* Compute ceil(remainder * 100 / period) without overflowing. */
    uint64_t remainder = duration % period;
    uint64_t accumulated = 0;
    uint64_t fraction = 0;
    for (unsigned i = 0; i < 100; ++i) {
        if (accumulated >= period - remainder) {
            accumulated -= period - remainder;
            ++fraction;
        } else {
            accumulated += remainder;
        }
    }
    uint64_t required = whole * 100 + fraction + (accumulated != 0);
    if (required > UINT16_MAX) {
        required = UINT16_MAX;
    }
    return (uint16_t)required;
}

/* Edge selection is transport policy; all window bounds come from the shared
 * linear-scan model. The measured blanking pulse is an assumption supplied by
 * the panel integration, not independent proof of optical blanking. */
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
    out.active_scan_us = frame_period_us - blanking_us;
    esp_display_present_te_timing_t timing = {
        .period_us = (uint64_t)frame_period_us,
        .scan_us = (uint64_t)out.active_scan_us,
    };
    esp_display_present_te_window_t window = esp_display_present_te_window(
                                                 &timing, (uint64_t)transfer_us, 1, 1, true);
    if (window.admission == ESP_DISPLAY_PRESENT_TE_UNSUPPORTED) {
        out.strategy = TE_STRATEGY_OVERRUN;
        out.sync_edge = blanking_end_edge;
        return out;
    }
    if (transfer_us <= out.active_scan_us) {
        out.strategy = TE_STRATEGY_FAST_START;
    } else if (transfer_us <= frame_period_us) {
        out.strategy = TE_STRATEGY_LEAD_START;
    } else {
        out.strategy = TE_STRATEGY_SLOW_END;
        out.sync_edge = blanking_end_edge;
    }
    out.phase_begin_us = (int64_t)window.earliest_phase_us;
    out.phase_end_us = (int64_t)window.latest_phase_us;
    out.blanking_reuse_us = out.phase_end_us > out.active_scan_us
                            ? out.phase_end_us - out.active_scan_us : 0;
    out.has_phase_window = true;
    return out;
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

    /* Saturate rather than wrap a huge or invalid request into a short,
     * apparently safe transfer. Normal display frame sizes never approach
     * this conservative arithmetic limit. */
    const uint64_t scale = 8000000ULL;
    if (transfer_bytes > UINT64_MAX / scale) {
        return INT64_MAX;
    }
    uint64_t scaled = (uint64_t)transfer_bytes * scale;
    uint64_t duration = scaled / bus_bits_per_sec + (scaled % bus_bits_per_sec != 0);
    return duration > INT64_MAX ? INT64_MAX : (int64_t)duration;
}

static void IRAM_ATTR te_gpio_isr(void *arg)
{
    esp_display_present_te_sync_context_t *ctx = (esp_display_present_te_sync_context_t *)arg;
    if (!ctx) {
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
    }
    portEXIT_CRITICAL_ISR(&ctx->lock);

    /* Both edges may open a new phase or establish its timer deadline. */
    portENTER_CRITICAL_ISR(&ctx->lock);
    esp_display_present_te_wakeup_fn wakeup = ctx->wakeup;
    void *user = ctx->wakeup_user;
    portEXIT_CRITICAL_ISR(&ctx->lock);
    if (wakeup && wakeup(user, true)) {
        need_yield = pdTRUE;
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

#if PRESENT_TE_PHASE_TIMER
/* Wake only; a phase timer cannot supply a TE timestamp. */
static void te_phase_wakeup(void *arg)
{
    esp_display_present_te_sync_context_t *ctx = arg;
    portENTER_CRITICAL(&ctx->lock);
    esp_display_present_te_wakeup_fn wakeup = ctx->wakeup;
    void *user = ctx->wakeup_user;
    portEXIT_CRITICAL(&ctx->lock);
    if (wakeup) {
        (void)wakeup(user, false);
    }
}

static esp_err_t te_phase_timer_destroy(esp_display_present_te_sync_context_t *ctx, uint32_t timeout)
{
    esp_err_t error = present_te_timer_destroy(ctx->phase_timer, timeout);
    if (error == ESP_OK) {
        ctx->phase_timer = NULL;
    }
    return error;
}

#endif

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
    /* Validate before reading the pin or shifting its number into a mask. */
    ESP_RETURN_ON_FALSE(cfg->gpio_num < 64 && GPIO_IS_VALID_GPIO(cfg->gpio_num), ESP_ERR_INVALID_ARG, TAG, "invalid TE GPIO");

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

#if PRESENT_TE_PHASE_TIMER
    /* One shared private timer implements drainable phase wakeups. */
    esp_err_t phase_ret = present_te_timer_create(te_phase_wakeup, ctx, &ctx->phase_timer);
    if (phase_ret != ESP_OK) {
        ret = phase_ret;
        goto fail;
    }
#endif

    const gpio_config_t gpio_cfg = {
        /* Both edges update measured timing and wake the nonblocking gate. */
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
        ctx->isr_registered = false;
    }
#if PRESENT_TE_PHASE_TIMER
    if (te_phase_timer_destroy(ctx, UINT32_MAX) != ESP_OK) {
        *out_ctx = ctx; /* Checked embedders can retain and retry failed cleanup. */
        return ret;
    }
#endif
    free(ctx);
    ESP_LOGE(TAG, "TE sync create failed: %s", esp_err_to_name(ret));
    return ret;
}

esp_err_t esp_display_present_te_sync_destroy_budget(esp_display_present_te_sync_context_t *ctx, uint32_t timeout)
{
    if (!ctx) {
        return ESP_OK;
    }
    uint64_t start = present_now_ms();
    if (ctx->isr_registered) {
        if (!ctx->isr_removed) {
            esp_err_t error = gpio_isr_handler_remove(ctx->cfg.gpio_num);
            if (error != ESP_OK) {
                return error;
            }
            ctx->isr_removed = true;
        }
        esp_err_t error = present_isr_barrier(present_remaining_ms(start, timeout));
        if (error != ESP_OK) {
            return error; /* Keep the context until callbacks have exited. */
        }
        ctx->isr_registered = false;
    }
#if PRESENT_TE_PHASE_TIMER
    esp_err_t error = te_phase_timer_destroy(ctx, present_remaining_ms(start, timeout));
    if (error != ESP_OK) {
        return error;
    }
#endif
    free(ctx);
    return ESP_OK;
}

esp_err_t esp_display_present_te_sync_destroy_checked(esp_display_present_te_sync_context_t *ctx)
{
    return esp_display_present_te_sync_destroy_budget(ctx, UINT32_MAX);
}

/* Caller holds ctx->lock. Recompute from the same measurement generation
 * that will be used to choose its edge and evaluate the current window. */
static te_schedule_window_t te_update_schedule_locked(
    esp_display_present_te_sync_context_t *ctx, size_t transfer_bytes)
{
    ctx->estimated_tx_duration_us = te_estimate_transfer_time_us(ctx, transfer_bytes);
    int64_t duration = ctx->estimated_tx_duration_us;
    if (ctx->last_tx_duration_us > duration) {
        duration = ctx->last_tx_duration_us;
    }
    te_schedule_window_t schedule = te_derive_schedule(
                                        ctx->te_period_us, duration, ctx->blanking_duration_us,
                                        ctx->blanking_start_edge, ctx->blanking_end_edge);
    if (schedule.strategy != TE_STRATEGY_UNKNOWN) {
        ctx->schedule = schedule;
        ctx->sync_edge = schedule.sync_edge;
    }
    return schedule;
}

static void update_schedule(esp_display_present_te_sync_context_t *ctx, size_t transfer_bytes)
{
    portENTER_CRITICAL(&ctx->lock);
    te_strategy_t previous_strategy = ctx->schedule.strategy;
    gpio_int_type_t previous_edge = ctx->schedule.sync_edge;
    te_schedule_window_t schedule = te_update_schedule_locked(ctx, transfer_bytes);
    bool strategy_changed = schedule.strategy != TE_STRATEGY_UNKNOWN &&
                            (schedule.strategy != previous_strategy || schedule.sync_edge != previous_edge);
    portEXIT_CRITICAL(&ctx->lock);
    int64_t duration_us = schedule.transfer_us;
    int64_t period_us = schedule.frame_period_us;

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

void esp_display_present_te_sync_set_wakeup(esp_display_present_te_sync_context_t *ctx,
                                            esp_display_present_te_wakeup_fn callback, void *user)
{
    portENTER_CRITICAL(&ctx->lock);
    ctx->wakeup = callback;
    ctx->wakeup_user = user;
    portEXIT_CRITICAL(&ctx->lock);
}

esp_err_t esp_display_present_te_sync_admit(esp_display_present_te_sync_context_t *ctx,
                                            size_t bytes, uint64_t after, uint64_t *edge)
{
    if (!ctx || !edge) {
        return ESP_ERR_INVALID_ARG;
    }
    /* A poll is not a new frame request. Admission depends on the current
     * measured window, never on an old edge's unconditional freshness shortcut. */
    update_schedule(ctx, bytes);
    portENTER_CRITICAL(&ctx->lock);
    /* Measurements can change while the log above runs. Refresh the model
     * under the same lock as both physical edge timestamps and the clock. */
    (void)te_update_schedule_locked(ctx, bytes);
    int64_t now = esp_timer_get_time();
    bool measured = ctx->schedule.frame_period_us > 0 && ctx->schedule.blanking_us > 0;
    /* A schedule update may change the selected polarity between interrupts.
     * Read its own edge timestamp, never the previous selection's timestamp. */
    int64_t selected_time = ctx->sync_edge == GPIO_INTR_POSEDGE
                            ? ctx->last_rise_time_us : ctx->last_fall_time_us;
    uint64_t selected = selected_time > 0 ? (uint64_t)selected_time : 0;
    int64_t scan = ctx->blanking_end_edge == GPIO_INTR_POSEDGE ? ctx->last_rise_time_us : ctx->last_fall_time_us;
    esp_display_present_te_timing_t timing = {
        .period_us = ctx->schedule.frame_period_us > 0 ? (uint64_t)ctx->schedule.frame_period_us : 0,
                                  .scan_us = ctx->schedule.active_scan_us > 0 ? (uint64_t)ctx->schedule.active_scan_us : 0,
    };
    uint64_t transfer = ctx->schedule.transfer_us > 0 ? (uint64_t)ctx->schedule.transfer_us : 0;
    esp_display_present_te_window_t window = esp_display_present_te_window(
                                                 &timing, transfer, scan > 0 ? (uint64_t)scan : 0,
                                                 now > 0 ? (uint64_t)now : 0, measured && scan > 0);
    bool accept = selected > after && window.admission == ESP_DISPLAY_PRESENT_TE_ALLOW;
    uint64_t next = 0;
    bool arm = selected > after && scan > 0 && now > 0 && present_te_next_open(
                   &timing, &window, (uint64_t)scan, (uint64_t)now, &next);
    portEXIT_CRITICAL(&ctx->lock);
    if (accept) {
        *edge = selected;
        return ESP_OK;
    }
#if PRESENT_TE_PHASE_TIMER
    if (arm && ctx->phase_timer) {
        /* The shared timer uses the same monotonic clock as the measured edge. */
        esp_err_t error = present_te_timer_arm(ctx->phase_timer, next);
        if (error != ESP_OK) {
            return error;
        }
    }
#else
    if (arm) {
        return ESP_ERR_NOT_SUPPORTED; /* Never park without a future wakeup. */
    }
#endif
    return ESP_ERR_TIMEOUT;
}

void IRAM_ATTR esp_display_present_te_sync_abort_tx(esp_display_present_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }
    portENTER_CRITICAL_SAFE(&ctx->lock);
    ctx->tx_in_progress = false;
    portEXIT_CRITICAL_SAFE(&ctx->lock);
}

size_t esp_display_present_te_sync_memory(void)
{
    size_t bytes = sizeof(esp_display_present_te_sync_context_t);
#if PRESENT_TE_PHASE_TIMER
    bytes += present_te_timer_memory();
#endif
    return bytes;
}
