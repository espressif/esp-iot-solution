/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0 */
/* Compile the actual adapter with an IDF 6.1 capability shim. */
#include "../../src/platform/te/present_te_sync.c"
int main(void)
{
    esp_display_present_te_sync_config_t cfg = {.gpio_num = 1, .bus_freq_hz = 8000000, .data_lines = 1};
    esp_display_present_te_sync_context_t *ctx = NULL;
    assert(esp_display_present_te_sync_create(&cfg, false, &ctx) == ESP_OK && ctx);
    ctx->te_period_us = 10000;
    ctx->blanking_duration_us = 1000;
    ctx->blanking_start_edge = GPIO_INTR_POSEDGE;
    ctx->blanking_end_edge = GPIO_INTR_NEGEDGE;
    ctx->last_rise_time_us = 1000;
    ctx->last_fall_time_us = 2000;
    uint64_t edge = 0;
    fake_time = 2000;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_ERR_NOT_SUPPORTED);
    fake_time = 10000;
    assert(esp_display_present_te_sync_admit(ctx, 1000, 0, &edge) == ESP_OK && edge == 1000);
    assert(esp_display_present_te_sync_destroy_budget(ctx, 100) == ESP_OK);
    assert(removes == 1);
    return 0;
}
