/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/* Pure window calculation: no GPIO, OS timer, allocation or worker lifecycle. */
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Explicit linear full-frame model: panel scan and writer traverse rows in the
 * same physical direction. transfer_us is the exact modeled duration for the
 * full transfer, not a bound sufficient to prove safety for all other speeds:
 * a faster writer can catch up with the scan. Callers must establish the scan
 * timing, phase and duration envelope and account for their uncertainty when
 * deciding whether this model applies. A single observation, estimate, or
 * transfer-duration upper bound alone does not prove a tear-free window.
 * Scan starts edge_to_scan_us after the selected TE edge. No implicit polarity.
 * A segmented/burst writer must NOT use this model without a proven envelope. */
typedef struct {
    uint64_t period_us, scan_us, edge_to_scan_us, guard_us, jitter_us;
} esp_display_present_te_timing_t;
typedef enum {
    ESP_DISPLAY_PRESENT_TE_ALLOW,
    ESP_DISPLAY_PRESENT_TE_WAIT,
    ESP_DISPLAY_PRESENT_TE_UNPROVEN,
    ESP_DISPLAY_PRESENT_TE_UNSUPPORTED,
} esp_display_present_te_admission_t;
typedef struct {
    esp_display_present_te_admission_t admission;
    uint64_t earliest_phase_us, latest_phase_us;
} esp_display_present_te_window_t;
esp_display_present_te_window_t esp_display_present_te_window(const esp_display_present_te_timing_t *, uint64_t transfer_us,
                                                              uint64_t edge_us, uint64_t now_us, bool measured);

/* Return the next opening of this measured edge's window, if it lies later
 * in the same edge-to-edge period. If false, wait for a real subsequent edge:
 * a predicted future period must not be treated as a measured edge. The
 * absolute timestamp is written only on success. `window` must be the WAIT
 * result returned for these same timing, edge and now values. */
bool present_te_next_open(const esp_display_present_te_timing_t *timing,
                          const esp_display_present_te_window_t *window,
                          uint64_t edge_us, uint64_t now_us, uint64_t *next_us);
#ifdef __cplusplus
}
#endif
