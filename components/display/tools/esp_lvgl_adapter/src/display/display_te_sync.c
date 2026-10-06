/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
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
    volatile int64_t last_te_isr_time_us; /*!< Timestamp captured at the TE interrupt */
    int64_t te_period_us;           /*!< Estimated TE period */
    int64_t tx_start_time_us;       /*!< Timestamp when the current transmission started */
    int64_t last_tx_duration_us;    /*!< Duration of the last transmission */
    int64_t estimated_tx_duration_us; /*!< Estimated duration of the current transmission */
    bool tx_in_progress;            /*!< Whether a transmission timestamp is active */
    bool tx_period_violation_logged;
    bool isr_registered;
    gpio_int_type_t intr_type;
    QueueHandle_t job_queue;
    SemaphoreHandle_t stage_free_sem;
    SemaphoreHandle_t tx_done_sem;
    SemaphoreHandle_t worker_stopped_sem;
    TaskHandle_t worker_task;
    volatile bool stop_requested;
};

static const char *TAG = "esp_lvgl:te";
static const uint8_t TE_WINDOW_MAX_DEFER = 1;
static const uint32_t TE_WAIT_TIMEOUT_MIN_MS = 20;
static const uint32_t TE_WAIT_TIMEOUT_MAX_MS = 1000;
static const uint32_t TE_WAIT_TIMEOUT_PERIODS = 6;
static const uint8_t TE_ASYNC_MAX_TIMEOUTS = 3;
static const uint32_t TE_WORKER_STACK_SIZE = 3072;
static const UBaseType_t TE_WORKER_PRIORITY = 7;

static uint32_t te_wait_timeout_ms(esp_lv_adapter_te_sync_context_t *ctx, int64_t te_period_us);

static void esp_lv_adapter_te_worker(void *arg)
{
    esp_lv_adapter_te_sync_context_t *ctx = (esp_lv_adapter_te_sync_context_t *)arg;
    esp_lv_adapter_te_sync_job_t job;

    while (xQueueReceive(ctx->job_queue, &job, portMAX_DELAY) == pdTRUE) {
        if (ctx->stop_requested) {
            break;
        }

        esp_err_t prepare_ret = job.prepare_tx ? job.prepare_tx(job.user_ctx, 0) : ESP_OK;
        if (prepare_ret != ESP_OK) {
            ESP_LOGE(TAG, "TE transfer 0 prepare failed: %s", esp_err_to_name(prepare_ret));
            job.done(job.user_ctx);
            xSemaphoreGive(ctx->stage_free_sem);
            continue;
        }

        esp_lv_adapter_te_sync_begin_frame(ctx, job.transfer_bytes);
        uint8_t timeout_count = 0;
        while (!ctx->stop_requested && esp_lv_adapter_te_sync_wait_for_vsync(ctx) != ESP_OK &&
                ++timeout_count < TE_ASYNC_MAX_TIMEOUTS) {
            esp_lv_adapter_te_sync_begin_frame(ctx, job.transfer_bytes);
        }

        if (!ctx->stop_requested && timeout_count < TE_ASYNC_MAX_TIMEOUTS) {
            esp_lv_adapter_te_sync_record_tx_start(ctx);
            for (size_t transfer_index = 0; transfer_index < job.transfer_count && !ctx->stop_requested;
                    transfer_index++) {
                (void)xSemaphoreTake(ctx->tx_done_sem, 0);
                esp_err_t ret = job.start_tx(job.user_ctx, transfer_index);
                if (ret == ESP_OK) {
                    esp_err_t next_prepare_ret = ESP_OK;
                    if (job.prepare_tx && transfer_index + 1 < job.transfer_count) {
                        next_prepare_ret = job.prepare_tx(job.user_ctx, transfer_index + 1);
                    }
                    int64_t te_period_us;
                    portENTER_CRITICAL(&ctx->lock);
                    te_period_us = ctx->te_period_us;
                    portEXIT_CRITICAL(&ctx->lock);
                    TickType_t tx_done_timeout_ticks = pdMS_TO_TICKS(te_wait_timeout_ms(ctx, te_period_us));
                    if (xSemaphoreTake(ctx->tx_done_sem, tx_done_timeout_ticks) != pdTRUE) {
                        ESP_LOGW(TAG, "TE transfer %zu completion callback timeout; still waiting", transfer_index);
                        /* Buffer ownership cannot be released until completion is confirmed. */
                        (void)xSemaphoreTake(ctx->tx_done_sem, portMAX_DELAY);
                    }
                    if (next_prepare_ret != ESP_OK) {
                        ESP_LOGE(TAG, "TE transfer %zu prepare failed: %s",
                                 transfer_index + 1, esp_err_to_name(next_prepare_ret));
                        break;
                    }
                } else {
                    ESP_LOGE(TAG, "TE transfer %zu start failed: %s", transfer_index, esp_err_to_name(ret));
                    break;
                }
            }
            esp_lv_adapter_te_sync_record_tx_done(ctx);
        } else if (!ctx->stop_requested) {
            ESP_LOGE(TAG, "Dropping async frame after %u TE timeouts", TE_ASYNC_MAX_TIMEOUTS);
        }

        if (!ctx->stop_requested) {
            job.done(job.user_ctx);
        }
        xSemaphoreGive(ctx->stage_free_sem);
    }

    xSemaphoreGive(ctx->worker_stopped_sem);
    vTaskSuspend(NULL);
}

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

static uint32_t te_wait_timeout_ms(esp_lv_adapter_te_sync_context_t *ctx, int64_t te_period_us)
{
    uint64_t period_us = 0;

    if (te_period_us > 0) {
        period_us = (uint64_t)te_period_us;
    } else {
        uint32_t tvdl_ms = ctx->cfg.time_tvdl_ms ? ctx->cfg.time_tvdl_ms : ESP_LV_ADAPTER_TE_TVDL_DEFAULT_MS;
        uint32_t tvdh_ms = ctx->cfg.time_tvdh_ms ? ctx->cfg.time_tvdh_ms : ESP_LV_ADAPTER_TE_TVDH_DEFAULT_MS;
        period_us = (uint64_t)(tvdl_ms + tvdh_ms) * 1000ULL;
    }

    uint64_t timeout_ms = (period_us * TE_WAIT_TIMEOUT_PERIODS + 999ULL) / 1000ULL;
    if (timeout_ms < TE_WAIT_TIMEOUT_MIN_MS) {
        timeout_ms = TE_WAIT_TIMEOUT_MIN_MS;
    }
    if (timeout_ms > TE_WAIT_TIMEOUT_MAX_MS) {
        timeout_ms = TE_WAIT_TIMEOUT_MAX_MS;
    }

    return (uint32_t)timeout_ms;
}

static TickType_t te_wait_timeout_ticks_from_us(int64_t wait_us)
{
    uint64_t wait_ms = (uint64_t)(wait_us + 999LL) / 1000ULL;
    TickType_t wait_ticks = pdMS_TO_TICKS(wait_ms);

    return (wait_ticks > 0) ? wait_ticks : 1;
}

static void te_reset_wait_state(esp_lv_adapter_te_sync_context_t *ctx)
{
    portENTER_CRITICAL(&ctx->lock);
    ctx->frame_request_ticks = 0;
    ctx->window_defer_count = 0;
    ctx->window_violation_logged = false;
    portEXIT_CRITICAL(&ctx->lock);
}

static int64_t te_estimate_transfer_time_us(const esp_lv_adapter_te_sync_context_t *ctx, size_t transfer_bytes)
{
    if (!ctx || transfer_bytes == 0 || ctx->cfg.bus_freq_hz == 0) {
        return 0;
    }

    uint32_t data_lines = ctx->cfg.data_lines ? ctx->cfg.data_lines : ESP_LV_ADAPTER_TE_DATA_LINES_DEFAULT;
    uint64_t bus_bits_per_sec = (uint64_t)ctx->cfg.bus_freq_hz * data_lines;
    if (bus_bits_per_sec == 0) {
        return 0;
    }

    uint64_t transfer_bits = (uint64_t)transfer_bytes * 8ULL;
    return (int64_t)((transfer_bits * 1000000ULL + bus_bits_per_sec - 1ULL) / bus_bits_per_sec);
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
    int64_t now_us = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&ctx->lock);
    int64_t previous_us = ctx->last_te_isr_time_us;
    ctx->last_te_ticks = xTaskGetTickCountFromISR();  /* IRAM safe */
    ctx->last_te_isr_time_us = now_us;
    if (previous_us > 0 && now_us > previous_us) {
        ctx->te_period_us = now_us - previous_us;
    }
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
    ctx->te_period_us = (int64_t)((ctx->cfg.time_tvdl_ms ? ctx->cfg.time_tvdl_ms : ESP_LV_ADAPTER_TE_TVDL_DEFAULT_MS) +
                                  (ctx->cfg.time_tvdh_ms ? ctx->cfg.time_tvdh_ms : ESP_LV_ADAPTER_TE_TVDH_DEFAULT_MS)) * 1000LL;
    ctx->tx_start_time_us = 0;
    ctx->last_tx_duration_us = 0;
    ctx->estimated_tx_duration_us = 0;
    ctx->tx_in_progress = false;

    const gpio_config_t te_cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << ctx->cfg.gpio_num,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_GOTO_ON_ERROR(gpio_config(&te_cfg), fail, TAG, "gpio config failed");

    /* Detect the idle level only after the GPIO input path is configured. */
    ctx->intr_type = detect_te_edge_type(cfg, intr_type, prefer_refresh_end);
    ESP_GOTO_ON_ERROR(gpio_set_intr_type(ctx->cfg.gpio_num, ctx->intr_type), fail, TAG,
                      "gpio interrupt type failed");

    ctx->te_vsync_sem = xSemaphoreCreateCounting(1, 0);
    ESP_GOTO_ON_FALSE(ctx->te_vsync_sem, ESP_ERR_NO_MEM, fail, TAG, "failed to create vsync semaphore");

    if (ctx->cfg.pipeline != ESP_LV_ADAPTER_TE_PIPELINE_SYNC_FULL) {
        ctx->job_queue = xQueueCreate(1, sizeof(esp_lv_adapter_te_sync_job_t));
        ctx->stage_free_sem = xSemaphoreCreateBinary();
        ctx->tx_done_sem = xSemaphoreCreateBinary();
        ctx->worker_stopped_sem = xSemaphoreCreateBinary();
        ESP_GOTO_ON_FALSE(ctx->job_queue && ctx->stage_free_sem && ctx->tx_done_sem && ctx->worker_stopped_sem,
                          ESP_ERR_NO_MEM, fail, TAG, "failed to create async TE resources");
        xSemaphoreGive(ctx->stage_free_sem);

        BaseType_t task_ret = xTaskCreate(esp_lv_adapter_te_worker, "lvgl_te",
                                          TE_WORKER_STACK_SIZE, ctx, TE_WORKER_PRIORITY,
                                          &ctx->worker_task);
        ESP_GOTO_ON_FALSE(task_ret == pdPASS, ESP_ERR_NO_MEM, fail, TAG,
                          "failed to create async TE worker");
    }

    ret = gpio_install_isr_service(ESP_INTR_FLAG_LOWMED);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_GOTO_ON_ERROR(ret, fail, TAG, "install isr service failed (%d)", ret);
    }

    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(ctx->cfg.gpio_num, esp_lv_adapter_te_gpio_isr, ctx), fail,
                      TAG, "add isr handler failed");
    ctx->isr_registered = true;

    *out_ctx = ctx;
    ESP_LOGI(TAG, "GPIO TE sync enabled on GPIO%d (edge: %s, mode: %s)",
             ctx->cfg.gpio_num,
             (ctx->intr_type == GPIO_INTR_NEGEDGE) ? "falling" : "rising",
             ctx->cfg.pipeline != ESP_LV_ADAPTER_TE_PIPELINE_SYNC_FULL ? "async-coherent" : "sync-full");
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

    if (ctx->worker_task) {
        esp_lv_adapter_te_sync_job_t stop_job = {0};
        ctx->stop_requested = true;
        (void)xQueueSend(ctx->job_queue, &stop_job, 0);
        if (ctx->te_vsync_sem) {
            xSemaphoreGive(ctx->te_vsync_sem);
        }

        bool tx_in_progress;
        portENTER_CRITICAL(&ctx->lock);
        tx_in_progress = ctx->tx_in_progress;
        portEXIT_CRITICAL(&ctx->lock);
        if (ctx->tx_done_sem && !tx_in_progress) {
            xSemaphoreGive(ctx->tx_done_sem);
        }
        (void)xSemaphoreTake(ctx->worker_stopped_sem, portMAX_DELAY);
        vTaskDelete(ctx->worker_task);
        ctx->worker_task = NULL;
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

    if (ctx->job_queue) {
        vQueueDelete(ctx->job_queue);
        ctx->job_queue = NULL;
    }
    if (ctx->stage_free_sem) {
        vSemaphoreDelete(ctx->stage_free_sem);
        ctx->stage_free_sem = NULL;
    }
    if (ctx->tx_done_sem) {
        vSemaphoreDelete(ctx->tx_done_sem);
        ctx->tx_done_sem = NULL;
    }
    if (ctx->worker_stopped_sem) {
        vSemaphoreDelete(ctx->worker_stopped_sem);
        ctx->worker_stopped_sem = NULL;
    }

    free(ctx);
    ESP_LOGI(TAG, "TE sync context destroyed");
}

void esp_lv_adapter_te_sync_begin_frame(esp_lv_adapter_te_sync_context_t *ctx, size_t transfer_bytes)
{
    if (!ctx) {
        return;
    }

    TickType_t request_ticks = xTaskGetTickCount();
    int64_t estimated_tx_duration_us = te_estimate_transfer_time_us(ctx, transfer_bytes);

    bool log_violation = false;
    portENTER_CRITICAL(&ctx->lock);
    int64_t duration = ctx->last_tx_duration_us;
    int64_t period = ctx->te_period_us;
    if (period > 0 && duration >= period && !ctx->tx_period_violation_logged) {
        ctx->tx_period_violation_logged = true;
        log_violation = true;
    }
    ctx->frame_request_ticks = request_ticks;
    ctx->estimated_tx_duration_us = estimated_tx_duration_us;
    ctx->window_defer_count = 0;
    ctx->window_violation_logged = false;
    portEXIT_CRITICAL(&ctx->lock);

    /* Report completion timing from task context. */
    if (log_violation) {
        ESP_LOGW(TAG, "Full-frame transfer exceeds one TE period (tx=%lldus, period=%lldus)",
                 (long long)duration, (long long)period);
    }

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

    uint32_t timeout_ms = te_wait_timeout_ms(ctx, ctx->te_period_us);
    int64_t deadline_us = esp_timer_get_time() + (int64_t)timeout_ms * 1000LL;

    while (true) {
        int64_t remaining_us = deadline_us - esp_timer_get_time();
        TickType_t wait_ticks = (remaining_us > 0) ? te_wait_timeout_ticks_from_us(remaining_us) : 0;

        if (xSemaphoreTake(ctx->te_vsync_sem, wait_ticks) != pdTRUE) {
            ESP_LOGW(TAG, "TE wait timed out (%" PRIu32 " ms)", timeout_ms);
            te_reset_wait_state(ctx);
            return ESP_ERR_TIMEOUT;
        }

        TickType_t current_ticks = xTaskGetTickCount();
        uint32_t te_ticks;
        TickType_t request_ticks;
        int64_t te_period_us;
        int64_t tx_duration_us;
        int64_t estimated_tx_duration_us;
        uint8_t window_percent;
        uint8_t window_defer_count;

        portENTER_CRITICAL(&ctx->lock);
        te_ticks = ctx->last_te_ticks;
        request_ticks = ctx->frame_request_ticks;
        te_period_us = ctx->te_period_us;
        tx_duration_us = ctx->last_tx_duration_us;
        estimated_tx_duration_us = ctx->estimated_tx_duration_us;
        window_percent = ctx->window_percent;
        window_defer_count = ctx->window_defer_count;
        portEXIT_CRITICAL(&ctx->lock);

        if (estimated_tx_duration_us > tx_duration_us) {
            tx_duration_us = estimated_tx_duration_us;
        }

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

        bool transfer_spans_period = te_period_us > 0 && tx_duration_us >= te_period_us;
        if (!te_window_has_room(window_percent, te_period_us, tx_duration_us) && !transfer_spans_period) {
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

        portENTER_CRITICAL(&ctx->lock);
        ctx->frame_request_ticks = 0;
        ctx->window_defer_count = 0;
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

bool esp_lv_adapter_te_sync_is_async(const esp_lv_adapter_te_sync_context_t *ctx)
{
    return ctx && ctx->cfg.pipeline != ESP_LV_ADAPTER_TE_PIPELINE_SYNC_FULL;
}

esp_err_t esp_lv_adapter_te_sync_acquire_stage(esp_lv_adapter_te_sync_context_t *ctx)
{
    ESP_RETURN_ON_FALSE(esp_lv_adapter_te_sync_is_async(ctx) && ctx->stage_free_sem,
                        ESP_ERR_INVALID_STATE, TAG, "asynchronous TE pipeline is disabled");
    ESP_RETURN_ON_FALSE(!ctx->stop_requested, ESP_ERR_INVALID_STATE, TAG,
                        "async TE worker is stopping");

    return xSemaphoreTake(ctx->stage_free_sem, portMAX_DELAY) == pdTRUE ? ESP_OK : ESP_FAIL;
}

void esp_lv_adapter_te_sync_release_stage(esp_lv_adapter_te_sync_context_t *ctx)
{
    if (ctx && ctx->stage_free_sem) {
        xSemaphoreGive(ctx->stage_free_sem);
    }
}

esp_err_t esp_lv_adapter_te_sync_wait_idle(esp_lv_adapter_te_sync_context_t *ctx, int32_t timeout_ms)
{
    if (!esp_lv_adapter_te_sync_is_async(ctx)) {
        return ESP_OK;
    }

    TickType_t wait_ticks = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xSemaphoreTake(ctx->stage_free_sem, wait_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    xSemaphoreGive(ctx->stage_free_sem);
    return ESP_OK;
}

esp_err_t esp_lv_adapter_te_sync_submit(esp_lv_adapter_te_sync_context_t *ctx,
                                        const esp_lv_adapter_te_sync_job_t *job)
{
    ESP_RETURN_ON_FALSE(ctx && job && job->transfer_bytes && job->transfer_count && job->start_tx && job->done,
                        ESP_ERR_INVALID_ARG, TAG, "invalid async TE job");
    ESP_RETURN_ON_FALSE(esp_lv_adapter_te_sync_is_async(ctx) && ctx->job_queue,
                        ESP_ERR_INVALID_STATE, TAG, "async TE mode is disabled");
    ESP_RETURN_ON_FALSE(!ctx->stop_requested, ESP_ERR_INVALID_STATE, TAG,
                        "async TE worker is stopping");

    if (xQueueSend(ctx->job_queue, job, 0) != pdTRUE) {
        ESP_LOGE(TAG, "async TE job queue is full");
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

bool IRAM_ATTR esp_lv_adapter_te_sync_notify_tx_done_from_isr(esp_lv_adapter_te_sync_context_t *ctx)
{
    if (!esp_lv_adapter_te_sync_is_async(ctx) || !ctx->tx_done_sem) {
        return false;
    }

    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(ctx->tx_done_sem, &need_yield);
    return need_yield == pdTRUE;
}
