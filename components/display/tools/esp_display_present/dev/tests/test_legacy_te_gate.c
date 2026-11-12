/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
/* Compile the complete compatibility TE implementation, not its mock admit.
 * Virtual clock/timer makes the boundary checks deterministic and sleep-free. */
#include "driver/gpio.h"
static void (*log_hook)(void);
static void test_log(const char *tag, const char *fmt, ...)
{
    (void)tag;
    (void)fmt;
    if (log_hook) {
        void (*hook)(void) = log_hook;
        log_hook = NULL;
        hook();
    }
}
#undef ESP_LOGI
#undef ESP_LOGW
#define ESP_LOGI test_log
#define ESP_LOGW test_log
#include "support/te_platform/platform.h"
#include "../../src/platform/te/present_te_timer.h"
/* Virtual clock fixture for the same private timer interface used on target. */
typedef struct {
    present_te_timer_callback_t callback;
    void *user;
    uint64_t due;
} virtual_phase_timer_t;
static virtual_phase_timer_t *virtual_phase_timer;
static bool phase_arm_failure;
static esp_err_t fake_phase_create(present_te_timer_callback_t callback, void *user, present_te_timer_t **out)
{
    virtual_phase_timer = calloc(1, sizeof(*virtual_phase_timer));
    if (!virtual_phase_timer) {
        return ESP_ERR_NO_MEM;
    }
    virtual_phase_timer->callback = callback;
    virtual_phase_timer->user = user;
    *out = (present_te_timer_t *)virtual_phase_timer;
    return ESP_OK;
}
static esp_err_t fake_phase_arm(present_te_timer_t *timer, uint64_t deadline)
{
    assert((void *)timer == virtual_phase_timer);
    if (phase_arm_failure) {
        return ESP_FAIL;
    }
    virtual_phase_timer->due = deadline;
    return ESP_OK;
}
static esp_err_t fake_phase_destroy(present_te_timer_t *timer, uint32_t timeout)
{
    if (!timer) {
        return ESP_OK;
    }
    assert((void *)timer == virtual_phase_timer);
    stop_budget = timeout;
    if (timer_busy) {
        return ESP_ERR_TIMEOUT;
    }
    free(virtual_phase_timer);
    virtual_phase_timer = NULL;
    return ESP_OK;
}
#define present_te_timer_create fake_phase_create
#define present_te_timer_arm fake_phase_arm
#define present_te_timer_destroy fake_phase_destroy
#include "../../src/platform/te/present_te_sync.c"
#undef present_te_timer_create
#undef present_te_timer_arm
#undef present_te_timer_destroy
static esp_display_present_te_sync_context_t *hook_context;
static void change_period_during_log(void)
{
    /* Simulate a measurement update after schedule logging, before admission. */
    hook_context->te_period_us = 20000;
    hook_context->blanking_duration_us = 1000;
}
static unsigned wakeups;
static bool wake(void *user, bool isr)
{
    (void)user;
    (void)isr;
    ++wakeups;
    return false;
}
int main(void)
{
    esp_display_present_te_sync_config_t cfg = {.gpio_num = 1, .bus_freq_hz = 8000000, .data_lines = 1};
    esp_display_present_te_sync_context_t *ctx;
    const int invalid_pins[] = {-1, 22, 49, 64};
    for (size_t i = 0; i < sizeof(invalid_pins) / sizeof(invalid_pins[0]); ++i) {
        cfg.gpio_num = invalid_pins[i];
        ctx = (void *)1;
        assert(esp_display_present_te_sync_create(&cfg, false, &ctx) == ESP_ERR_INVALID_ARG && !ctx);
    }
    cfg.gpio_num = 1;
    assert(esp_display_present_te_sync_create(&cfg, false, &ctx) == ESP_OK);
    esp_display_present_te_sync_set_wakeup(ctx, wake, NULL);
    uint64_t edge = 0;
    fake_time = 100;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_ERR_TIMEOUT);
    ctx->te_period_us = 10000;
    ctx->blanking_duration_us = 1000;
    ctx->blanking_start_edge = GPIO_INTR_POSEDGE;
    ctx->blanking_end_edge = GPIO_INTR_NEGEDGE;
    ctx->last_rise_time_us = 1000;
    ctx->last_fall_time_us = 2000;
    fake_time = 2000;
    /* Ttx=1000, Ta=9000 => late phase begins at 10000 absolute. */
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_ERR_TIMEOUT);
    assert(virtual_phase_timer->due == 10000);
    fake_time = 9999;
    phase_arm_failure = true;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_FAIL);
    phase_arm_failure = false;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_ERR_TIMEOUT);
    assert(virtual_phase_timer->due == 10000);
    fake_time = 10000;
    virtual_phase_timer->callback(virtual_phase_timer->user);
    assert(wakeups == 1);
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_OK && edge == 1000);
    assert(esp_display_present_te_sync_admit(ctx, 1000, edge, &edge) == ESP_ERR_TIMEOUT);
    fake_time = 12000; /* Late service cannot reuse an old selected edge. */
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_ERR_TIMEOUT);
    assert(esp_display_present_te_sync_admit(ctx, 30000, 0, &edge) == ESP_ERR_TIMEOUT);

    /* Fast transfer: the same window spans late scan and blanking. A fresh
     * blanking-start edge enables reuse; the next scan start is exclusive. */
    ctx->last_rise_time_us = 11000;
    fake_time = 11000;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 2000, &edge) == ESP_OK && edge == 11000);
    fake_time = 11999;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 2000, &edge) == ESP_OK);
    fake_time = 12000;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 2000, &edge) == ESP_ERR_TIMEOUT);

    /* Lead transfer: the window crosses into blanking and closes after the
     * calculated reuse interval. It must never use a stale selected edge. */
    fake_time = 11499;
    assert(esp_display_present_te_sync_admit(ctx, 9500, 2000, &edge) == ESP_OK);
    assert(esp_display_present_te_sync_admit(ctx, 9500, 11000, &edge) == ESP_ERR_TIMEOUT);
    fake_time = 11500;
    assert(esp_display_present_te_sync_admit(ctx, 9500, 2000, &edge) == ESP_ERR_TIMEOUT);

    /* Switching to the other polarity must use its own most recent edge.
     * An unrelated later edge must never satisfy the request watermark. */
    ctx->last_rise_time_us = 1000;
    ctx->last_fall_time_us = 2000;
    fake_time = 3000;
    assert(esp_display_present_te_sync_admit(ctx, 12000, 1000, &edge) == ESP_OK && edge == 2000);
    virtual_phase_timer->due = 0;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 1000, &edge) == ESP_ERR_TIMEOUT);
    assert(virtual_phase_timer->due == 0); /* The selected edge is not fresh. */
    fake_time = 10000;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 1500, &edge) == ESP_ERR_TIMEOUT);

    /* Slow transfer selects scan_start and closes in active scan; no timer
     * may predict the next period without another actual edge. */
    fake_time = 8999;
    assert(esp_display_present_te_sync_admit(ctx, 12000, 0, &edge) == ESP_OK);
    assert(ctx->sync_edge == GPIO_INTR_NEGEDGE);
    fake_time = 9000;
    assert(esp_display_present_te_sync_admit(ctx, 12000, 0, &edge) == ESP_ERR_TIMEOUT);
    assert(te_estimate_transfer_time_us(ctx, SIZE_MAX) == INT64_MAX);
    assert(te_window_required_percent(INT64_MAX, 10000) == UINT16_MAX);
    assert(esp_display_present_te_sync_admit(ctx, SIZE_MAX, 0, &edge) == ESP_ERR_TIMEOUT);
    /* The model, edge and clock must all belong to the same locked snapshot.
     * Logging provides a deterministic ISR interleaving point: stale 10 ms
     * timing would admit at 10 ms, the new 20 ms timing must wait. */
    ctx->schedule.strategy = TE_STRATEGY_UNKNOWN;
    ctx->te_period_us = 10000;
    ctx->blanking_duration_us = 1000;
    ctx->last_rise_time_us = 1000;
    ctx->last_fall_time_us = 2000;
    fake_time = 10000;
    virtual_phase_timer->due = 0;
    hook_context = ctx;
    log_hook = change_period_during_log;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_ERR_TIMEOUT);
    assert(log_hook == NULL && virtual_phase_timer->due == 20000);
    timer_busy = true;
    assert(esp_display_present_te_sync_destroy_budget(ctx, 0) == ESP_ERR_TIMEOUT);
    assert(removes == 1 && stop_budget == 0);
    timer_busy = false;
    assert(esp_display_present_te_sync_destroy_budget(ctx, 100) == ESP_OK);
    assert(removes == 1 && stop_budget <= 100);
    return 0;
}
