/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <inttypes.h>
#include "platform/te/present_te_sync.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static uint64_t first, tx_start, count, sum, max_wait, tx_count, tx_sum, tx_max, polls, immediate;
static uint64_t buckets[7];
void meter_reset(void)
{
    portENTER_CRITICAL(&mux);
    first = tx_start = count = sum = max_wait = tx_count = tx_sum = tx_max = polls = immediate = 0;
    for (unsigned i = 0; i < 7; i++) {
        buckets[i] = 0;
    }
    portEXIT_CRITICAL(&mux);
}
esp_err_t __real_esp_display_present_te_sync_admit(esp_display_present_te_sync_context_t*, size_t, uint64_t, uint64_t*);
esp_err_t __wrap_esp_display_present_te_sync_admit(esp_display_present_te_sync_context_t*c, size_t bytes, uint64_t after, uint64_t*edge)
{
    uint64_t now = esp_timer_get_time();
    if (!first) {
        first = now;
    }
    polls++;
    esp_err_t e = __real_esp_display_present_te_sync_admit(c, bytes, after, edge);
    if (e == ESP_OK) {
        uint64_t us = esp_timer_get_time() - first;
        portENTER_CRITICAL(&mux);
        count++; sum += us; if (us > max_wait)max_wait = us;
        if (us < 100) {
            immediate++;
        }
        unsigned b = us < 1000 ? 0 : us < 4000 ? 1 : us < 8000 ? 2 : us < 12000 ? 3 : us < 17000 ? 4 : us < 34000 ? 5 : 6;
        buckets[b]++; first = 0;
        portEXIT_CRITICAL(&mux);
    }
    return e;
}
void __real_esp_display_present_te_sync_record_tx_start(esp_display_present_te_sync_context_t*);
void __wrap_esp_display_present_te_sync_record_tx_start(esp_display_present_te_sync_context_t*c)
{
    portENTER_CRITICAL(&mux); tx_start = esp_timer_get_time(); portEXIT_CRITICAL(&mux);
    __real_esp_display_present_te_sync_record_tx_start(c);
}
void __real_esp_display_present_te_sync_record_tx_done(esp_display_present_te_sync_context_t*);
void IRAM_ATTR __wrap_esp_display_present_te_sync_record_tx_done(esp_display_present_te_sync_context_t*c)
{
    uint64_t now = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&mux);
    if (tx_start) {
        uint64_t us = now - tx_start;
        tx_count++;
        tx_sum += us;
        if (us > tx_max) {
            tx_max = us;
        } tx_start = 0;
    }
    portEXIT_CRITICAL_ISR(&mux);
    __real_esp_display_present_te_sync_record_tx_done(c);
}
void meter_report(const char* label)
{
    uint64_t n, s, m, t, ts, tm, p, im, b[7];
    portENTER_CRITICAL(&mux);
    n = count; s = sum; m = max_wait; t = tx_count; ts = tx_sum; tm = tx_max; p = polls; im = immediate;
    for (unsigned i = 0; i < 7; i++) {
        b[i] = buckets[i];
    }
    portEXIT_CRITICAL(&mux);
    ESP_LOGI("TE_METER", "label=%s admitted=%"PRIu64" wait_avg_us=%"PRIu64" wait_max_us=%"PRIu64" immediate=%"PRIu64" polls=%"PRIu64" tx_n=%"PRIu64" tx_avg_us=%"PRIu64" tx_max_us=%"PRIu64" hist_lt_1_4_8_12_17_34_inf_ms=%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64,
             label, n, n ? s / n : 0, m, im, p, t, t ? ts / t : 0, tm, b[0], b[1], b[2], b[3], b[4], b[5], b[6]);
}
