/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "platform/te/present_te_timing.h"
#include <assert.h>
int main(void)
{
    esp_display_present_te_timing_t m = {.period_us = 100, .scan_us = 90, .guard_us = 2, .jitter_us = 1};
    for (unsigned w = 1; w < 200; w++)
        for (unsigned t = 0; t < 100; t++) {
            esp_display_present_te_window_t a = esp_display_present_te_window(&m, w, 1000, 1000 + t, true);
            if (a.admission == ESP_DISPLAY_PRESENT_TE_ALLOW) {
                /* Every scanline must be written after this refresh's read and
                 * before the next refresh's read, including both endpoints. */
                for (unsigned y = 0; y <= 100; y++) {
                    long write = (long)t * 100 + (long)y * w;
                    long scan = (long)y * m.scan_us;
                    assert(write >= scan + 200);
                    assert(write < scan + 10000 - 200);
                }
            }
        }
    assert(esp_display_present_te_window(&m, 40, 0, 60, false).admission == ESP_DISPLAY_PRESENT_TE_UNPROVEN);
    assert(esp_display_present_te_window(&m, 40, 0, 100, true).admission == ESP_DISPLAY_PRESENT_TE_WAIT);
    assert(esp_display_present_te_window(&m, 200, 0, 10, true).admission == ESP_DISPLAY_PRESENT_TE_UNSUPPORTED);
    assert(esp_display_present_te_window(&m, 40, 0, 55, true).admission == ESP_DISPLAY_PRESENT_TE_ALLOW);
    assert(esp_display_present_te_window(&m, 120, 0, 3, true).admission == ESP_DISPLAY_PRESENT_TE_ALLOW);
    m.edge_to_scan_us = 20;
    assert(esp_display_present_te_window(&m, 40, 0, 75, true).admission == ESP_DISPLAY_PRESENT_TE_ALLOW);

    /* Independently search every future instant in this actual edge period.
     * This exercises wrapping across scan start, inclusive opening, exclusive
     * close and the rule that predicted subsequent edges are never used. */
    for (uint64_t offset = 0; offset < 20; offset++) {
        esp_display_present_te_timing_t timing = {
            .period_us = 20, .scan_us = 13, .edge_to_scan_us = offset,
            .guard_us = 1, .jitter_us = 1,
        };
        for (uint64_t transfer = 1; transfer < 34; transfer++) {
            for (uint64_t elapsed = 0; elapsed < timing.period_us; elapsed++) {
                const uint64_t edge = 1000;
                esp_display_present_te_window_t current = esp_display_present_te_window(&timing, transfer, edge, edge + elapsed, true);
                uint64_t actual = UINT64_MAX;
                bool found = present_te_next_open(&timing, &current, edge, edge + elapsed, &actual);
                bool expected = false;
                uint64_t first = 0;
                if (current.admission == ESP_DISPLAY_PRESENT_TE_WAIT) {
                    for (uint64_t candidate = elapsed + 1; candidate < timing.period_us; candidate++) {
                        if (esp_display_present_te_window(&timing, transfer, edge, edge + candidate, true).admission == ESP_DISPLAY_PRESENT_TE_ALLOW) {
                            expected = true;
                            first = edge + candidate;
                            break;
                        }
                    }
                }
                assert(found == expected);
                if (found) {
                    assert(actual == first);
                } else {
                    assert(actual == UINT64_MAX);
                }
            }
        }
    }
    /* The valid scan-relative window straddles the selected edge: the
     * preceding window is open at edge+0..3, then closes and opens again at
     * edge+14. Returning 14 (not the next period) drives the phase timer. */
    esp_display_present_te_timing_t crossing = {
        .period_us = 20, .scan_us = 13, .edge_to_scan_us = 5, .guard_us = 1,
    };
    esp_display_present_te_window_t crossing_window = esp_display_present_te_window(&crossing, 5, 1000, 1000, true);
    assert(crossing_window.admission == ESP_DISPLAY_PRESENT_TE_ALLOW);
    crossing_window = esp_display_present_te_window(&crossing, 5, 1000, 1004, true);
    assert(crossing_window.admission == ESP_DISPLAY_PRESENT_TE_WAIT);
    uint64_t crossing_next = 0;
    assert(present_te_next_open(&crossing, &crossing_window, 1000, 1004, &crossing_next));
    assert(crossing_next == 1014);
    assert(esp_display_present_te_window(&crossing, 5, 1000, crossing_next, true).admission == ESP_DISPLAY_PRESENT_TE_ALLOW);
    crossing_window = esp_display_present_te_window(&crossing, 5, 1000, 1020, true);
    assert(crossing_window.admission == ESP_DISPLAY_PRESENT_TE_WAIT);
    assert(!present_te_next_open(&crossing, &crossing_window, 1000, 1020, &crossing_next));

    esp_display_present_te_timing_t near_wrap = {.period_us = 20, .scan_us = 13, .guard_us = 1};
    const uint64_t edge = UINT64_MAX - 5;
    esp_display_present_te_window_t window = esp_display_present_te_window(&near_wrap, 1, edge, edge, true);
    uint64_t next = 42;
    assert(window.admission == ESP_DISPLAY_PRESENT_TE_WAIT);
    assert(!present_te_next_open(&near_wrap, &window, edge, edge, &next));
    assert(next == 42);
    assert(!present_te_next_open(&near_wrap, &window, 10, 9, &next));
    assert(!present_te_next_open(&near_wrap, &window, 10, 30, &next));
    window.admission = ESP_DISPLAY_PRESENT_TE_UNPROVEN;
    assert(!present_te_next_open(&near_wrap, &window, 10, 10, &next));
    return 0;
}
