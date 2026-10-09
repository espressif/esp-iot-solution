/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "present_fb_internal.h"
#include "present_mode_internal.h"

#include "esp_attr.h"

bool present_fb_transfer_done_isr(
    esp_display_present_fb_endpoint_t *surface)
{
    if (surface == NULL ||
            !esp_display_present_tracker_isr_enter(&surface->tracker)) {
        return false;
    }
    if (surface->fb.single_pending_switch) {
        esp_display_present_tracker_color_done(&surface->tracker);
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
    esp_display_present_fb_endpoint_t *surface, uint64_t *ticket)
{
    return present_fb_uses_pipeline(surface) &&
           esp_display_present_tracker_retire_submit_isr(
               &surface->tracker, !present_fb_requires_repair(surface),
               surface->fb.submit_gate_enabled, &surface->fb.disp_fb, ticket);
}

bool present_fb_frame_done_isr(
    esp_display_present_fb_endpoint_t *surface,
    BaseType_t *need_yield)
{
    if (need_yield == NULL || surface == NULL ||
            !esp_display_present_tracker_isr_enter(&surface->tracker)) {
        return false;
    }
    uint64_t ticket = 0;
    if (!release_frame_done_isr(surface, &ticket)) {
        esp_display_present_tracker_isr_leave(&surface->tracker);
        return false;
    }
    /* Ownership was retired above from the atomic submission record. Ticket
     * completion is a parallel fence update and cannot decide whether the
     * previous display buffer becomes writable. */
    bool completed = esp_display_present_tracker_complete_present_frame_isr(
                         &surface->tracker, ticket);
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
