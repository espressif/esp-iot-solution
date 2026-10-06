/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_fb_internal.h"
#include "present_mode_internal.h"

#include "esp_attr.h"

typedef struct {
    bool switched;
    bool pipeline_released;
    uint64_t ticket;
} fb_frame_release_t;

bool present_fb_transfer_done_isr(
    esp_display_present_fb_endpoint_t *surface)
{
    if (surface == NULL ||
            !esp_display_present_tracker_isr_enter(&surface->tracker)) {
        return false;
    }
    if (!esp_display_present_tracker_tracks_transfer(&surface->tracker)) {
        esp_display_present_tracker_isr_leave(&surface->tracker);
        return false;
    }
    uint64_t frame_id;
    (void)esp_display_present_tracker_complete_transfer_isr(
        &surface->tracker, &frame_id);
    BaseType_t need_yield = pdFALSE;
    esp_display_present_tracker_signal_completion_isr(
        &surface->tracker, &need_yield);
    esp_display_present_tracker_isr_leave(&surface->tracker);
    return need_yield == pdTRUE;
}

static bool IRAM_ATTR release_frame_done_isr(
    esp_display_present_fb_endpoint_t *surface,
    fb_frame_release_t *out_result)
{
    fb_frame_release_t result = {0};
    if (surface == NULL || !present_fb_uses_pipeline(surface)) {
        if (out_result != NULL) {
            *out_result = result;
        }
        return false;
    }

    const bool switch_pipeline = !present_fb_requires_repair(surface);
    bool switched = true;
    void *published = surface->fb.disp_fb;
    if (switch_pipeline) {
        switched = esp_display_present_tracker_publish_pending_isr(
                       &surface->tracker, &published);
    }
    result.switched = switched;
    if (!switched) {
        if (out_result != NULL) {
            *out_result = result;
        }
        return false;
    }

    if (!esp_display_present_tracker_consume_submit_isr(
                &surface->tracker, surface->fb.submit_gate_enabled)) {
        if (switch_pipeline) {
            esp_display_present_tracker_set_pending_isr(
                &surface->tracker, published);
        }
        if (out_result != NULL) {
            *out_result = result;
        }
        return false;
    }
    void *submitted_display = NULL;
    uint64_t ticket = 0;
    if (!esp_display_present_tracker_pool_retire_isr(
                &surface->tracker, &submitted_display, &ticket)) {
        if (out_result != NULL) {
            *out_result = result;
        }
        return false;
    }
    if (switch_pipeline && submitted_display != published) {
        esp_display_present_tracker_mark_faulted_isr(
            &surface->tracker, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL);
        if (out_result != NULL) {
            *out_result = result;
        }
        return false;
    }
    surface->fb.disp_fb = submitted_display;
    result.pipeline_released = true;
    result.ticket = ticket;
    if (out_result != NULL) {
        *out_result = result;
    }
    return true;
}

bool present_fb_frame_done_isr(
    esp_display_present_fb_endpoint_t *surface,
    BaseType_t *need_yield)
{
    if (need_yield == NULL || surface == NULL ||
            !esp_display_present_tracker_isr_enter(&surface->tracker)) {
        return false;
    }
    if (present_fb_requires_repair(surface) &&
            !esp_display_present_tracker_pool_has_inflight(&surface->tracker)) {
        esp_display_present_tracker_isr_leave(&surface->tracker);
        return false;
    }

    fb_frame_release_t release = {0};
    if (!release_frame_done_isr(surface, &release)) {
        esp_display_present_tracker_isr_leave(&surface->tracker);
        return false;
    }
    /* Ownership was retired above from the atomic submission record. Ticket
     * completion is a parallel fence update and cannot decide whether the
     * previous display buffer becomes writable. */
    bool completed = esp_display_present_tracker_complete_present_frame_isr(
                         &surface->tracker, release.ticket);
    if (completed) {
        esp_display_present_tracker_signal_completion_isr(
            &surface->tracker, need_yield);
    }
    esp_display_present_tracker_isr_leave(&surface->tracker);
    return *need_yield == pdTRUE;
}

uint64_t present_fb_completed_transfer(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL
           ? esp_display_present_tracker_get_completed_transfer_frame(
               &surface->tracker) : 0;
}

uint64_t present_fb_completed_present(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL
           ? esp_display_present_tracker_get_completed_present_frame(
               &surface->tracker) : 0;
}

bool IRAM_ATTR esp_display_present_fb_notify_transfer_done_from_isr(
    esp_display_present_fb_endpoint_t *surface)
{
    return present_fb_transfer_done_isr(surface);
}

bool IRAM_ATTR esp_display_present_fb_notify_frame_done_from_isr(
    esp_display_present_fb_endpoint_t *surface,
    BaseType_t *need_yield)
{
    return present_fb_frame_done_isr(surface, need_yield);
}

uint64_t esp_display_present_fb_get_completed_transfer(
    const esp_display_present_fb_endpoint_t *surface)
{
    return present_fb_completed_transfer(surface);
}

uint64_t esp_display_present_fb_get_last_submitted(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL
           ? esp_display_present_tracker_get_last_submitted_frame(
               &surface->tracker) : 0;
}

uint64_t esp_display_present_fb_get_completed_present(
    const esp_display_present_fb_endpoint_t *surface)
{
    return present_fb_completed_present(surface);
}

bool esp_display_present_fb_is_faulted(
    const esp_display_present_fb_endpoint_t *surface)
{
    return surface != NULL &&
           esp_display_present_tracker_is_faulted(&surface->tracker);
}
