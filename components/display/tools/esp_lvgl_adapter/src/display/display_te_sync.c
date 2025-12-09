/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "display_te_sync.h"

struct esp_lv_adapter_te_sync_context {
    esp_lv_adapter_te_sync_config_t cfg;
    SemaphoreHandle_t te_vsync_sem;
    portMUX_TYPE lock;
    uint32_t last_te_ticks;         /*!< Last TE interrupt tick count (IRAM safe) */
    TickType_t tvdh_ticks;          /*!< Tvdh in ticks for tick-based comparison */
    TickType_t frame_request_ticks; /*!< Tick count when the current frame request started */
    uint8_t window_percent;         /*!< Allowed TE window percentage */
    uint8_t window_defer_count;     /*!< Number of deferred TE cycles */
    bool window_violation_logged;   /*!< Whether last violation already logged */
    int64_t last_te_time_us;        /*!< Timestamp of the last TE signal (esp_timer units) */
    int64_t te_period_us;           /*!< Estimated TE period */
    int64_t tx_start_time_us;       /*!< Timestamp when the current transmission started */
    int64_t last_tx_duration_us;    /*!< Duration of the last transmission */
    bool tx_in_progress;            /*!< Whether a transmission timestamp is active */
    bool isr_registered;
    gpio_int_type_t intr_type;
};

static const char *TAG = "esp_lvgl:te";
static const uint8_t TE_WINDOW_MAX_DEFER = 1;

static inline bool te_tick_after(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) >= 0;
}

static inline bool te_window_has_room(uint8_t percent, int64_t period_us, int64_t duration_us)
{
    if (percent == 0 || period_us <= 0 || duration_us <= 0) {
        return true;
    }
    int64_t window_us = (period_us * percent) / 100;
    return window_us <= 0 || duration_us <= window_us;
}

static uint8_t te_window_required_percent(int64_t duration_us, int64_t period_us)
{
    if (duration_us <= 0 || period_us <= 0) {
        return ESP_LV_ADAPTER_TE_WINDOW_PERCENT_DEFAULT;
    }

    uint64_t required = (uint64_t)(duration_us * 100ULL + period_us - 1ULL) / period_us;
    if (required > 100ULL) {
        required = 100ULL;
    }
    return (uint8_t)required;
}

static uint8_t te_window_expand_percent(uint8_t current_percent, int64_t duration_us, int64_t period_us)
{
    uint8_t required = te_window_required_percent(duration_us, period_us);

    if (required <= current_percent) {
        return current_percent;
    }

    uint32_t expanded = required + ESP_LV_ADAPTER_TE_WINDOW_MARGIN_PERCENT;
    if (expanded > 100U) {
        expanded = 100U;
    }
    return (uint8_t)expanded;
}

/**
 * @brief TE GPIO ISR handler
 *
 * This ISR is triggered by the TE (Tearing Effect) signal from the LCD panel.
 * It records the tick count and signals the vsync semaphore.
 * Uses IRAM-safe FreeRTOS tick API instead of esp_log_timestamp().
 */
static void IRAM_ATTR esp_lv_adapter_te_gpio_isr(void *arg)
{
    esp_lv_adapter_te_sync_context_t *ctx = (esp_lv_adapter_te_sync_context_t *)arg;
    if (!ctx || !ctx->te_vsync_sem) {
        return;
    }

    BaseType_t need_yield = pdFALSE;
    portENTER_CRITICAL_ISR(&ctx->lock);
    ctx->last_te_ticks = xTaskGetTickCountFromISR();  /* IRAM safe */
    portEXIT_CRITICAL_ISR(&ctx->lock);

    xSemaphoreGiveFromISR(ctx->te_vsync_sem, &need_yield);
    if (need_yield) {
        portYIELD_FROM_ISR();
    }
}

bool esp_lv_adapter_te_sync_is_enabled(const esp_lv_adapter_te_sync_config_t *cfg)
{
    return cfg && cfg->gpio_num >= 0;
}

/**
 * @brief Auto-detect TE signal edge type
 *
 * If intr_type is GPIO_INTR_DISABLE, automatically detect the edge.
 * Otherwise, use the provided edge type.
 *
 * @param cfg TE configuration
 * @param intr_type User-specified interrupt type (or GPIO_INTR_DISABLE for auto)
 * @return Detected or specified interrupt type
 */
static gpio_int_type_t detect_te_edge_type(const esp_lv_adapter_te_sync_config_t *cfg,
                                           gpio_int_type_t intr_type,
                                           bool prefer_refresh_end)
{
    /* Use user-specified edge type if valid */
    if (intr_type == GPIO_INTR_NEGEDGE || intr_type == GPIO_INTR_POSEDGE) {
        return intr_type;
    }

    /* Auto-detection: sample GPIO level to identify active state */
    int idle_level = gpio_get_level(cfg->gpio_num);
    bool idle_high = (idle_level == 1);

    gpio_int_type_t start_edge = idle_high ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
    gpio_int_type_t end_edge = idle_high ? GPIO_INTR_POSEDGE : GPIO_INTR_NEGEDGE;
    gpio_int_type_t selected = prefer_refresh_end ? end_edge : start_edge;

    ESP_LOGI(TAG, "Auto TE edge: GPIO%d idle=%d, prefer %s -> %s edge",
             cfg->gpio_num, idle_level,
             prefer_refresh_end ? "end" : "start",
             (selected == GPIO_INTR_NEGEDGE) ? "falling" : "rising");

    return selected;
}

esp_err_t esp_lv_adapter_te_sync_create(const esp_lv_adapter_te_sync_config_t *cfg,
                                        gpio_int_type_t intr_type,
                                        bool prefer_refresh_end,
                                        esp_lv_adapter_te_sync_context_t **out_ctx)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(out_ctx, ESP_ERR_INVALID_ARG, TAG, "output ctx is NULL");
    *out_ctx = NULL;
    ESP_RETURN_ON_FALSE(esp_lv_adapter_te_sync_is_enabled(cfg), ESP_ERR_INVALID_ARG, TAG, "invalid TE config");

    esp_lv_adapter_te_sync_context_t *ctx = calloc(1, sizeof(*ctx));
    ESP_RETURN_ON_FALSE(ctx, ESP_ERR_NO_MEM, TAG, "no memory for TE context");

    ctx->cfg = *cfg;
    ctx->lock.owner = portMUX_FREE_VAL;
    ctx->lock.count = 0;
    ctx->tvdh_ticks = pdMS_TO_TICKS(ctx->cfg.time_tvdh_ms ? ctx->cfg.time_tvdh_ms : ESP_LV_ADAPTER_TE_TVDH_DEFAULT_MS);
    ctx->frame_request_ticks = 0;
    if (ctx->cfg.refresh_window_percent == 0 || ctx->cfg.refresh_window_percent > 100) {
        ctx->window_percent = ESP_LV_ADAPTER_TE_WINDOW_PERCENT_DEFAULT;
    } else {
        ctx->window_percent = ctx->cfg.refresh_window_percent;
    }
    ctx->window_defer_count = 0;
    ctx->window_violation_logged = false;
    ctx->last_te_time_us = 0;
    ctx->te_period_us = 0;
    ctx->tx_start_time_us = 0;
    ctx->last_tx_duration_us = 0;
    ctx->tx_in_progress = false;

    /* Auto-detect or use specified TE edge type */
    ctx->intr_type = detect_te_edge_type(cfg, intr_type, prefer_refresh_end);

    ctx->te_vsync_sem = xSemaphoreCreateCounting(1, 0);
    ESP_GOTO_ON_FALSE(ctx->te_vsync_sem, ESP_ERR_NO_MEM, fail, TAG, "failed to create vsync semaphore");

    const gpio_config_t te_cfg = {
        .intr_type = ctx->intr_type,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << ctx->cfg.gpio_num,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&te_cfg), fail, TAG, "gpio config failed");

    ret = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_GOTO_ON_ERROR(ret, fail, TAG, "install isr service failed (%d)", ret);
    }

    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(ctx->cfg.gpio_num, esp_lv_adapter_te_gpio_isr, ctx), fail,
                      TAG, "add isr handler failed");
    ctx->isr_registered = true;

    *out_ctx = ctx;
    ESP_LOGI(TAG, "GPIO TE sync enabled on GPIO%d (edge: %s)",
             ctx->cfg.gpio_num,
             (ctx->intr_type == GPIO_INTR_NEGEDGE) ? "falling" : "rising");
    return ESP_OK;

fail:
    esp_lv_adapter_te_sync_destroy(ctx);
    return ESP_FAIL;
}

/**
 * @brief Destroy TE synchronization context with graceful shutdown
 *
 * Requests task exit, waits for completion, then cleans up resources.
 */
void esp_lv_adapter_te_sync_destroy(esp_lv_adapter_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    /* Remove ISR handler */
    if (ctx->isr_registered) {
        gpio_isr_handler_remove(ctx->cfg.gpio_num);
        ctx->isr_registered = false;
    }

    if (ctx->te_vsync_sem) {
        vSemaphoreDelete(ctx->te_vsync_sem);
        ctx->te_vsync_sem = NULL;
    }

    free(ctx);
    ESP_LOGI(TAG, "TE sync context destroyed");
}

void esp_lv_adapter_te_sync_begin_frame(esp_lv_adapter_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    TickType_t request_ticks = xTaskGetTickCount();

    portENTER_CRITICAL(&ctx->lock);
    ctx->frame_request_ticks = request_ticks;
    ctx->window_defer_count = 0;
    ctx->window_violation_logged = false;
    portEXIT_CRITICAL(&ctx->lock);

    /* Drop any stale TE signal so we only consume ones after this request */
    (void)xSemaphoreTake(ctx->te_vsync_sem, 0);
}

/**
 * @brief Wait for TE vsync signal with Tvdh validation
 *
 * Waits for the TE signal. If Tvdh timing is configured, checks if the signal
 * is stale (occurred too long ago). If stale, waits for the next TE signal.
 *
 * Tvdh (vertical display hold time): Period when the panel is NOT updating
 * from frame memory. It's safe to transmit during this window.
 *
 * @param ctx TE sync context
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if wait times out
 */
esp_err_t esp_lv_adapter_te_sync_wait_for_vsync(esp_lv_adapter_te_sync_context_t *ctx)
{
    if (!ctx || !ctx->te_vsync_sem) {
        return ESP_OK;
    }

    while (true) {
        if (xSemaphoreTake(ctx->te_vsync_sem, portMAX_DELAY) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }

        TickType_t current_ticks = xTaskGetTickCount();
        uint32_t te_ticks;
        TickType_t request_ticks;
        int64_t te_period_us;
        int64_t tx_duration_us;
        uint8_t window_percent;
        uint8_t window_defer_count;

        portENTER_CRITICAL(&ctx->lock);
        te_ticks = ctx->last_te_ticks;
        request_ticks = ctx->frame_request_ticks;
        te_period_us = ctx->te_period_us;
        tx_duration_us = ctx->last_tx_duration_us;
        window_percent = ctx->window_percent;
        window_defer_count = ctx->window_defer_count;
        portEXIT_CRITICAL(&ctx->lock);

        if (request_ticks != 0 && !te_tick_after(te_ticks, request_ticks)) {
            ESP_LOGD(TAG, "TE signal before frame request, ignoring");
            continue;
        }

        if (ctx->tvdh_ticks > 0) {
            TickType_t delta_ticks = current_ticks - te_ticks;
            if (delta_ticks > ctx->tvdh_ticks) {
                ESP_LOGD(TAG, "TE signal stale (delta=%u > tvdh=%u), waiting for next",
                         (unsigned)delta_ticks, (unsigned)ctx->tvdh_ticks);
                continue;
            }
        }

        if (!te_window_has_room(window_percent, te_period_us, tx_duration_us)) {
            bool request_next_window = false;

            uint8_t expanded_percent = te_window_expand_percent(window_percent, tx_duration_us, te_period_us);
            if (expanded_percent > window_percent) {
                portENTER_CRITICAL(&ctx->lock);
                ctx->window_percent = expanded_percent;
                ctx->window_defer_count = 0;
                ctx->window_violation_logged = false;
                window_percent = expanded_percent;
                window_defer_count = 0;
                portEXIT_CRITICAL(&ctx->lock);
                ESP_LOGW(TAG, "TE window expanded to %u%% (tx=%lldus, period=%lldus)",
                         expanded_percent,
                         (long long)tx_duration_us,
                         (long long)te_period_us);
                request_next_window = true;
            } else if (window_defer_count < TE_WINDOW_MAX_DEFER) {
                portENTER_CRITICAL(&ctx->lock);
                ctx->window_defer_count++;
                window_defer_count = ctx->window_defer_count;
                portEXIT_CRITICAL(&ctx->lock);
                ESP_LOGD(TAG, "Deferring flush to next TE window (tx=%lldus)",
                         (long long)tx_duration_us);
                request_next_window = true;
            } else {
                portENTER_CRITICAL(&ctx->lock);
                if (!ctx->window_violation_logged) {
                    ctx->window_violation_logged = true;
                    portEXIT_CRITICAL(&ctx->lock);
                    int64_t window_us = (te_period_us * window_percent) / 100;
                    ESP_LOGW(TAG, "TE window too small (tx=%lldus, window=%lldus)",
                             (long long)tx_duration_us,
                             (long long)window_us);
                } else {
                    portEXIT_CRITICAL(&ctx->lock);
                }
            }

            if (request_next_window) {
                continue;
            }
        }

        int64_t te_time_us = esp_timer_get_time();
        portENTER_CRITICAL(&ctx->lock);
        int64_t prev_te_time_us = ctx->last_te_time_us;
        ctx->last_te_time_us = te_time_us;
        ctx->frame_request_ticks = 0;
        ctx->window_defer_count = 0;
        if (prev_te_time_us > 0) {
            int64_t period = te_time_us - prev_te_time_us;
            if (period > 0) {
                ctx->te_period_us = period;
            }
        }
        portEXIT_CRITICAL(&ctx->lock);

        return ESP_OK;
    }
}

void esp_lv_adapter_te_sync_record_tx_start(esp_lv_adapter_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&ctx->lock);
    ctx->tx_start_time_us = now;
    ctx->tx_in_progress = true;
    portEXIT_CRITICAL(&ctx->lock);
}

void esp_lv_adapter_te_sync_record_tx_done(esp_lv_adapter_te_sync_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&ctx->lock);
    if (ctx->tx_in_progress && ctx->tx_start_time_us > 0) {
        int64_t duration = now - ctx->tx_start_time_us;
        if (duration > 0) {
            ctx->last_tx_duration_us = duration;
        }
    }
    ctx->tx_in_progress = false;
    portEXIT_CRITICAL(&ctx->lock);
}
