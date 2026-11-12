/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "backend/present_backend.h"
#include "core/present_completion_tracker.h"

void present_completion_record_error(present_completion_tracker_t *tracker, esp_err_t error)
{
    if (tracker->first_error == ESP_OK) {
        tracker->first_error = error;
    }
}

esp_err_t present_completion_reap(present_completion_tracker_t *tracker, const present_backend_ops_t *ops, present_backend_t *backend)
{
    while (tracker->count) {
        esp_display_present_frame_status_t status;
        esp_err_t ret = ops->query(backend, tracker->receipts[tracker->head], &status);
        if (ret != ESP_OK) {
            return ret;
        }
        if (!status.terminal) {
            break;
        }
        ret = ops->release_receipt(backend, tracker->receipts[tracker->head]);
        if (ret != ESP_OK) {
            return ret;
        }
        for (size_t i = 0; i < PRESENT_MAX_FRAMES; ++i) {
            present_completion_observer_t *observer = &tracker->observers[i];
            if (observer->token && observer->backend.value == tracker->receipts[tracker->head].value) {
                observer->status = status;
            }
        }
        present_completion_record_error(tracker, status.result);
        tracker->head = (tracker->head + 1) % tracker->limit;
        --tracker->count;
        ++tracker->completed;
    }
    return ESP_OK;
}

present_completion_observer_t *present_completion_find(present_completion_tracker_t *tracker, esp_display_present_engine_receipt_t receipt)
{
    for (size_t i = 0; receipt.value && i < PRESENT_MAX_FRAMES; ++i)
        if (tracker->observers[i].token == receipt.value) {
            return &tracker->observers[i];
        }
    return NULL;
}

esp_err_t present_completion_query(present_completion_observer_t *observer, const present_backend_ops_t *ops, present_backend_t *backend, esp_display_present_frame_status_t *out)
{
    if (!observer) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!observer->status.terminal) {
        esp_err_t error = ops->query(backend, observer->backend, &observer->status);
        if (error != ESP_OK) {
            return error;
        }
    }
    *out = observer->status;
    return ESP_OK;
}
