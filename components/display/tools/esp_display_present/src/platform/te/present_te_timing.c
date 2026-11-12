/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "platform/te/present_te_timing.h"
#include <stddef.h>
esp_display_present_te_window_t esp_display_present_te_window(const esp_display_present_te_timing_t *timing, uint64_t w, uint64_t edge, uint64_t now, bool measured)
{
    esp_display_present_te_window_t out = {.admission = ESP_DISPLAY_PRESENT_TE_UNSUPPORTED};
    if (!timing || !timing->period_us || !timing->scan_us || timing->scan_us > timing->period_us || !w ||
            timing->edge_to_scan_us >= timing->period_us || timing->guard_us >= timing->period_us ||
            timing->jitter_us > timing->guard_us) {
        return out;
    }
    uint64_t delta = w > timing->scan_us ? w - timing->scan_us : timing->scan_us - w;
    if (delta >= timing->period_us || timing->guard_us > (timing->period_us - delta) / 2) {
        return out;
    }
    uint64_t lo = (w < timing->scan_us ? delta : 0) + timing->guard_us;
    uint64_t hi = timing->period_us - (w > timing->scan_us ? delta : 0) - timing->guard_us;
    if (lo >= hi) {
        return out;
    }
    out.earliest_phase_us = lo;
    out.latest_phase_us = hi;
    if (!measured) {
        out.admission = ESP_DISPLAY_PRESENT_TE_UNPROVEN;
        return out;
    }
    if (now < edge || now - edge >= timing->period_us) {
        out.admission = ESP_DISPLAY_PRESENT_TE_WAIT;
        return out;
    }
    uint64_t phase = now - edge;
    phase = phase >= timing->edge_to_scan_us ? phase - timing->edge_to_scan_us : timing->period_us - (timing->edge_to_scan_us - phase);
    out.admission = phase >= lo && phase < hi ? ESP_DISPLAY_PRESENT_TE_ALLOW : ESP_DISPLAY_PRESENT_TE_WAIT;
    return out;
}

bool present_te_next_open(const esp_display_present_te_timing_t *timing,
                          const esp_display_present_te_window_t *window,
                          uint64_t edge_us, uint64_t now_us, uint64_t *next_us)
{
    if (!timing || !window || !next_us || !timing->period_us ||
            timing->edge_to_scan_us >= timing->period_us ||
            window->admission != ESP_DISPLAY_PRESENT_TE_WAIT ||
            window->earliest_phase_us >= window->latest_phase_us ||
            window->latest_phase_us > timing->period_us || now_us < edge_us ||
            now_us - edge_us >= timing->period_us) {
        return false;
    }

    /* Map the scan-relative opening into this actual edge's period. Avoid
     * adding two uint64_t phases before reducing them modulo the period. */
    const uint64_t remaining = timing->period_us - timing->edge_to_scan_us;
    const uint64_t opening = window->earliest_phase_us >= remaining
                             ? window->earliest_phase_us - remaining
                             : window->earliest_phase_us + timing->edge_to_scan_us;
    if (opening <= now_us - edge_us || opening > UINT64_MAX - edge_us) {
        return false;
    }
    *next_us = edge_us + opening;
    return true;
}
