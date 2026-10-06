/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_transfer_wait.h"

esp_err_t present_transfer_wait_idle(
    esp_display_present_frame_tracker_t *tracker,
    uint32_t timeout_ms)
{
    if (tracker == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    while (esp_display_present_tracker_get_pending_transfer_tickets(
                tracker) != 0) {
        if (!esp_display_present_tracker_wait_completion(
                    tracker, pdMS_TO_TICKS(timeout_ms))) {
            esp_display_present_tracker_mark_faulted(
                tracker, ESP_DISPLAY_PRESENT_FAULT_TRANSFER_TIMEOUT);
            return ESP_ERR_TIMEOUT;
        }
    }
    return ESP_OK;
}
