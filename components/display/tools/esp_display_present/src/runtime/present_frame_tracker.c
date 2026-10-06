/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_display_present_frame_tracker.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_display_present.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "present_tracker";
static const uint32_t PIPELINE_ACQUIRE_POLL_MS = 10;

/* ---- Frame domain ---- */

static void IRAM_ATTR fault_locked(esp_display_present_frame_tracker_t *tracker,
                                   esp_display_present_fault_reason_t reason)
{
    tracker->frame.faulted = true;
    if (tracker->frame.fault_reason == ESP_DISPLAY_PRESENT_FAULT_NONE) {
        tracker->frame.fault_reason = reason;
    }
}

static bool queue_push(esp_display_present_frame_queue_t *queue,
                       uint64_t frame_id)
{
    if (queue->count == ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES) {
        return false;
    }
    uint8_t tail = (uint8_t)((queue->head + queue->count) %
                             ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES);
    queue->ids[tail] = frame_id;
    ++queue->count;
    return true;
}

static void queue_rollback(esp_display_present_frame_queue_t *queue,
                           uint64_t frame_id)
{
    if (queue->count == 0) {
        return;
    }
    uint8_t tail = (uint8_t)((queue->head + queue->count - 1U) %
                             ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES);
    if (queue->ids[tail] == frame_id) {
        --queue->count;
    }
}

static bool IRAM_ATTR queue_pop(esp_display_present_frame_queue_t *queue,
                                uint64_t *out_frame_id)
{
    if (queue->count == 0 || out_frame_id == NULL) {
        return false;
    }
    *out_frame_id = queue->ids[queue->head];
    queue->head = (uint8_t)((queue->head + 1U) %
                            ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES);
    --queue->count;
    return true;
}

static bool validate_locked(esp_display_present_frame_tracker_t *tracker,
                            uint64_t frame_id,
                            TaskHandle_t current_task)
{
    if (!tracker->frame.initialized || !tracker->frame.accepting_callbacks ||
            tracker->frame.faulted || frame_id == 0) {
        return false;
    }
    if (tracker->frame.producer_task == NULL) {
        tracker->frame.producer_task = current_task;
    }
    return tracker->frame.producer_task == current_task &&
           frame_id > tracker->frame.last_submitted_frame;
}

void esp_display_present_tracker_init_frame(
    esp_display_present_frame_tracker_t *tracker,
    TaskHandle_t producer_task,
    bool track_transfer_completion)
{
    if (tracker == NULL) {
        return;
    }
    tracker->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    memset(&tracker->frame, 0, sizeof(tracker->frame));
    tracker->frame.producer_task = producer_task;
    tracker->frame.track_transfer_completion = track_transfer_completion;
    tracker->frame.completion_event = xSemaphoreCreateBinaryStatic(
                                          &tracker->frame.completion_event_storage);
    tracker->frame.accepting_callbacks = true;
    tracker->frame.initialized = true;
    tracker->frame.building_frame = 0;
    tracker->frame.pending_transfer_tickets = 0;
}

void esp_display_present_tracker_signal_completion_isr(
    esp_display_present_frame_tracker_t *tracker,
    BaseType_t *need_yield)
{
    if (tracker != NULL && tracker->frame.completion_event != NULL) {
        xSemaphoreGiveFromISR(tracker->frame.completion_event, need_yield);
    }
}

void esp_display_present_tracker_signal_completion(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker != NULL && tracker->frame.completion_event != NULL) {
        (void)xSemaphoreGive(tracker->frame.completion_event);
    }
}

bool esp_display_present_tracker_wait_completion(
    esp_display_present_frame_tracker_t *tracker,
    TickType_t ticks_to_wait)
{
    return tracker != NULL && tracker->frame.completion_event != NULL &&
           xSemaphoreTake(tracker->frame.completion_event, ticks_to_wait) ==
           pdTRUE;
}

esp_err_t esp_display_present_tracker_validate_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    TaskHandle_t current_task)
{
    if (tracker == NULL || current_task == NULL || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&tracker->lock);
    bool valid = validate_locked(tracker, frame_id, current_task);
    portEXIT_CRITICAL(&tracker->lock);
    return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t esp_display_present_tracker_begin_build(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    TaskHandle_t current_task,
    uint64_t *out_previous_frame)
{
    if (tracker == NULL || current_task == NULL || frame_id == 0 ||
            out_previous_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&tracker->lock);
    if (!validate_locked(tracker, frame_id, current_task) ||
            (tracker->frame.building_frame != 0 &&
             tracker->frame.last_submitted_frame !=
             tracker->frame.building_frame)) {
        portEXIT_CRITICAL(&tracker->lock);
        return ESP_ERR_INVALID_STATE;
    }

    *out_previous_frame = tracker->frame.last_submitted_frame;
    tracker->frame.building_frame = frame_id;
    portEXIT_CRITICAL(&tracker->lock);
    return ESP_OK;
}

esp_err_t esp_display_present_tracker_publish_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    uint64_t *out_previous_frame)
{
    if (tracker == NULL || out_previous_frame == NULL || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&tracker->lock);
    bool valid = tracker->frame.initialized &&
                 tracker->frame.building_frame == frame_id &&
                 tracker->frame.last_submitted_frame != frame_id;
    bool present_queued = valid &&
                          queue_push(&tracker->frame.present_queue, frame_id);
    bool transfer_queued = present_queued &&
                           (!tracker->frame.track_transfer_completion ||
                            queue_push(&tracker->frame.transfer_queue, frame_id));
    if (!transfer_queued) {
        if (present_queued) {
            queue_rollback(&tracker->frame.present_queue, frame_id);
        }
        if (tracker->frame.track_transfer_completion) {
            queue_rollback(&tracker->frame.transfer_queue, frame_id);
        }
        portEXIT_CRITICAL(&tracker->lock);
        return ESP_ERR_INVALID_STATE;
    }

    *out_previous_frame = tracker->frame.last_submitted_frame;
    tracker->frame.last_submitted_frame = frame_id;
    portEXIT_CRITICAL(&tracker->lock);
    return ESP_OK;
}

esp_err_t esp_display_present_tracker_continue_build(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    TaskHandle_t current_task)
{
    if (tracker == NULL || current_task == NULL || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&tracker->lock);
    bool valid = tracker->frame.initialized &&
                 tracker->frame.accepting_callbacks &&
                 !tracker->frame.faulted &&
                 tracker->frame.producer_task == current_task &&
                 tracker->frame.building_frame == frame_id;
    portEXIT_CRITICAL(&tracker->lock);
    return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t esp_display_present_tracker_end_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id)
{
    if (tracker == NULL || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&tracker->lock);
    bool valid = tracker->frame.initialized &&
                 tracker->frame.building_frame == frame_id;
    if (valid) {
        tracker->frame.building_frame = 0;
    } else {
        fault_locked(tracker, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL);
    }
    portEXIT_CRITICAL(&tracker->lock);
    return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}

void esp_display_present_tracker_rollback_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    uint64_t previous_frame,
    bool submission_active)
{
    if (tracker == NULL) {
        return;
    }
    portENTER_CRITICAL(&tracker->lock);
    if (submission_active) {
        fault_locked(tracker, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL);
    } else {
        queue_rollback(&tracker->frame.present_queue, frame_id);
        if (tracker->frame.track_transfer_completion) {
            queue_rollback(&tracker->frame.transfer_queue, frame_id);
        }
        if (tracker->frame.last_submitted_frame == frame_id) {
            tracker->frame.last_submitted_frame = previous_frame;
        }
        if (tracker->frame.building_frame == frame_id) {
            tracker->frame.building_frame = 0;
        }
    }
    portEXIT_CRITICAL(&tracker->lock);
}

uint64_t esp_display_present_tracker_get_building_frame(
    const esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return 0;
    }
    portENTER_CRITICAL((portMUX_TYPE *)&tracker->lock);
    uint64_t frame_id = tracker->frame.building_frame;
    portEXIT_CRITICAL((portMUX_TYPE *)&tracker->lock);
    return frame_id;
}

esp_err_t esp_display_present_tracker_complete_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    bool transfer_complete,
    bool present_complete)
{
    if (tracker == NULL || frame_id == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint64_t completed_frame = 0;
    bool valid = true;
    portENTER_CRITICAL(&tracker->lock);
    if (!tracker->frame.initialized) {
        valid = false;
    }
    if (valid && transfer_complete && tracker->frame.track_transfer_completion) {
        valid = queue_pop(&tracker->frame.transfer_queue, &completed_frame) &&
                completed_frame == frame_id;
        if (valid) {
            tracker->frame.completed_transfer_frame = frame_id;
        }
    }
    if (valid && present_complete) {
        valid = queue_pop(&tracker->frame.present_queue, &completed_frame) &&
                completed_frame == frame_id;
        if (valid) {
            /* Closing the build is end_frame's job alone. Clearing it here
             * too would hide a producer that never closed its frame instead
             * of letting end_frame fault on it. */
            tracker->frame.completed_present_frame = frame_id;
        }
    }
    if (!valid) {
        fault_locked(tracker, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL);
    }
    portEXIT_CRITICAL(&tracker->lock);
    return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}

bool esp_display_present_tracker_isr_enter(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return false;
    }
    portENTER_CRITICAL_ISR(&tracker->lock);
    bool accepted = tracker->frame.initialized &&
                    tracker->frame.accepting_callbacks;
    if (accepted) {
        ++tracker->frame.active_isr_count;
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);
    return accepted;
}

void esp_display_present_tracker_isr_leave(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return;
    }
    portENTER_CRITICAL_ISR(&tracker->lock);
    if (tracker->frame.active_isr_count != 0) {
        --tracker->frame.active_isr_count;
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);
}

bool esp_display_present_tracker_complete_transfer_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t *out_frame_id)
{
    if (tracker == NULL || out_frame_id == NULL) {
        return false;
    }
    portENTER_CRITICAL_ISR(&tracker->lock);
    bool completed = tracker->frame.track_transfer_completion &&
                     queue_pop(&tracker->frame.transfer_queue, out_frame_id);
    if (completed) {
        tracker->frame.completed_transfer_frame = *out_frame_id;
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);
    return completed;
}

bool esp_display_present_tracker_complete_transfer_ticket_isr(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return false;
    }
    portENTER_CRITICAL_ISR(&tracker->lock);
    bool completed = tracker->frame.pending_transfer_tickets != 0;
    if (completed) {
        --tracker->frame.pending_transfer_tickets;
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);
    return completed;
}

bool esp_display_present_tracker_complete_transfer_frame_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id)
{
    if (tracker == NULL || frame_id == 0) {
        return false;
    }
    uint64_t completed_frame = 0;
    portENTER_CRITICAL_ISR(&tracker->lock);
    bool completed = tracker->frame.track_transfer_completion &&
                     queue_pop(&tracker->frame.transfer_queue, &completed_frame) &&
                     completed_frame == frame_id;
    if (completed) {
        tracker->frame.completed_transfer_frame = frame_id;
    } else {
        fault_locked(tracker, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL);
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);
    return completed;
}

esp_err_t esp_display_present_tracker_submit_transfer_ticket(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&tracker->lock);
    if (!tracker->frame.initialized || tracker->frame.faulted ||
            tracker->frame.pending_transfer_tickets == UINT16_MAX) {
        portEXIT_CRITICAL(&tracker->lock);
        return ESP_ERR_INVALID_STATE;
    }
    ++tracker->frame.pending_transfer_tickets;
    portEXIT_CRITICAL(&tracker->lock);
    return ESP_OK;
}

void esp_display_present_tracker_cancel_transfer_ticket(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return;
    }
    portENTER_CRITICAL(&tracker->lock);
    if (tracker->frame.pending_transfer_tickets != 0) {
        --tracker->frame.pending_transfer_tickets;
    }
    portEXIT_CRITICAL(&tracker->lock);
}

void esp_display_present_tracker_drop_transfer_tickets(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return;
    }
    portENTER_CRITICAL(&tracker->lock);
    tracker->frame.pending_transfer_tickets = 0;
    portEXIT_CRITICAL(&tracker->lock);
}

uint16_t esp_display_present_tracker_get_pending_transfer_tickets(
    const esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return 0;
    }
    portENTER_CRITICAL_ISR((portMUX_TYPE *)&tracker->lock);
    uint16_t tickets = tracker->frame.pending_transfer_tickets;
    portEXIT_CRITICAL_ISR((portMUX_TYPE *)&tracker->lock);
    return tickets;
}

bool esp_display_present_tracker_complete_present_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t *out_frame_id)
{
    if (tracker == NULL || out_frame_id == NULL) {
        return false;
    }
    portENTER_CRITICAL_ISR(&tracker->lock);
    bool completed = queue_pop(&tracker->frame.present_queue, out_frame_id);
    if (completed) {
        /* The panel finishing a frame says nothing about whether the
         * producer has closed its build: the frame is queued for present
         * before the transfer starts, so this interrupt routinely lands
         * while the producer is still between publish and end_frame.
         * Closing the build here would strand that end_frame with a
         * mismatched id, which latches the tracker faulted for good. */
        tracker->frame.completed_present_frame = *out_frame_id;
    } else {
        fault_locked(tracker, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL);
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);
    return completed;
}

bool esp_display_present_tracker_complete_present_frame_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id)
{
    if (tracker == NULL || frame_id == 0) {
        return false;
    }
    uint64_t completed_frame = 0;
    portENTER_CRITICAL_ISR(&tracker->lock);
    bool completed = queue_pop(&tracker->frame.present_queue,
                               &completed_frame) &&
                     completed_frame == frame_id;
    if (completed) {
        tracker->frame.completed_present_frame = frame_id;
    } else {
        fault_locked(tracker, ESP_DISPLAY_PRESENT_FAULT_PRODUCER_PROTOCOL);
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);
    return completed;
}

bool esp_display_present_tracker_has_pending_present(
    const esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return false;
    }
    portENTER_CRITICAL_ISR((portMUX_TYPE *)&tracker->lock);
    bool pending = tracker->frame.present_queue.count != 0;
    portEXIT_CRITICAL_ISR((portMUX_TYPE *)&tracker->lock);
    return pending;
}

void esp_display_present_tracker_mark_faulted(
    esp_display_present_frame_tracker_t *tracker,
    esp_display_present_fault_reason_t reason)
{
    if (tracker == NULL) {
        return;
    }
    portENTER_CRITICAL(&tracker->lock);
    fault_locked(tracker, reason);
    portEXIT_CRITICAL(&tracker->lock);
}

void esp_display_present_tracker_mark_faulted_isr(
    esp_display_present_frame_tracker_t *tracker,
    esp_display_present_fault_reason_t reason)
{
    if (tracker == NULL) {
        return;
    }
    portENTER_CRITICAL_ISR(&tracker->lock);
    fault_locked(tracker, reason);
    portEXIT_CRITICAL_ISR(&tracker->lock);
}

esp_err_t esp_display_present_tracker_stop(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&tracker->lock);
    if (!tracker->frame.initialized) {
        portEXIT_CRITICAL(&tracker->lock);
        return ESP_OK;
    }
    if (tracker->frame.present_queue.count != 0 ||
            tracker->frame.transfer_queue.count != 0 ||
            tracker->frame.pending_transfer_tickets != 0 ||
            tracker->frame.building_frame != 0) {
        portEXIT_CRITICAL(&tracker->lock);
        return ESP_ERR_INVALID_STATE;
    }
    tracker->frame.accepting_callbacks = false;
    portEXIT_CRITICAL(&tracker->lock);

    while (tracker->frame.active_isr_count != 0) {
        taskYIELD();
    }
    portENTER_CRITICAL(&tracker->lock);
    tracker->frame.initialized = false;
    portEXIT_CRITICAL(&tracker->lock);
    return ESP_OK;
}

esp_err_t esp_display_present_tracker_force_stop(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&tracker->lock);
    if (!tracker->frame.initialized) {
        portEXIT_CRITICAL(&tracker->lock);
        return ESP_OK;
    }
    tracker->frame.present_queue.count = 0;
    tracker->frame.present_queue.head = 0;
    tracker->frame.transfer_queue.count = 0;
    tracker->frame.transfer_queue.head = 0;
    tracker->frame.building_frame = 0;
    tracker->frame.pending_transfer_tickets = 0;
    tracker->frame.accepting_callbacks = false;
    tracker->frame.producer_task = NULL;
    portEXIT_CRITICAL(&tracker->lock);

    while (tracker->frame.active_isr_count != 0) {
        taskYIELD();
    }
    portENTER_CRITICAL(&tracker->lock);
    tracker->frame.initialized = false;
    portEXIT_CRITICAL(&tracker->lock);
    return ESP_OK;
}

esp_err_t esp_display_present_tracker_rebind_producer_task(
    esp_display_present_frame_tracker_t *tracker, TaskHandle_t producer_task)
{
    if (tracker == NULL || producer_task == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&tracker->lock);
    bool valid = tracker->frame.initialized &&
                 tracker->frame.accepting_callbacks &&
                 !tracker->frame.faulted &&
                 tracker->frame.building_frame == 0;
    if (valid) {
        tracker->frame.producer_task = producer_task;
    }
    portEXIT_CRITICAL(&tracker->lock);
    return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}

TaskHandle_t esp_display_present_tracker_get_producer_task(
    const esp_display_present_frame_tracker_t *tracker)
{
    return tracker != NULL ? tracker->frame.producer_task : NULL;
}

bool esp_display_present_tracker_tracks_transfer(
    const esp_display_present_frame_tracker_t *tracker)
{
    return tracker != NULL && tracker->frame.track_transfer_completion;
}

uint64_t esp_display_present_tracker_get_completed_transfer_frame(
    const esp_display_present_frame_tracker_t *tracker)
{
    return tracker != NULL ? tracker->frame.completed_transfer_frame : 0;
}

uint64_t esp_display_present_tracker_get_completed_present_frame(
    const esp_display_present_frame_tracker_t *tracker)
{
    return tracker != NULL ? tracker->frame.completed_present_frame : 0;
}

uint64_t esp_display_present_tracker_get_last_submitted_frame(
    const esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return 0;
    }
    portENTER_CRITICAL((portMUX_TYPE *)&tracker->lock);
    uint64_t frame_id = tracker->frame.last_submitted_frame;
    portEXIT_CRITICAL((portMUX_TYPE *)&tracker->lock);
    return frame_id;
}

bool esp_display_present_tracker_is_faulted(
    const esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return false;
    }
    portENTER_CRITICAL((portMUX_TYPE *)&tracker->lock);
    bool faulted = tracker->frame.faulted;
    portEXIT_CRITICAL((portMUX_TYPE *)&tracker->lock);
    return faulted;
}

esp_display_present_fault_reason_t esp_display_present_tracker_get_fault_reason(
    const esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return ESP_DISPLAY_PRESENT_FAULT_NONE;
    }
    portENTER_CRITICAL((portMUX_TYPE *)&tracker->lock);
    esp_display_present_fault_reason_t reason = tracker->frame.fault_reason;
    portEXIT_CRITICAL((portMUX_TYPE *)&tracker->lock);
    return reason;
}

/* ---- Switch domain ---- */

esp_err_t esp_display_present_tracker_init_switch(
    esp_display_present_frame_tracker_t *tracker)
{
    if (!tracker) {
        return ESP_ERR_INVALID_ARG;
    }

    tracker->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    tracker->sw.pending_frame_buffer = NULL;
    tracker->sw.armed_submit_count = 0;
    tracker->sw.closing = false;
    tracker->sw.pending_sem = xSemaphoreCreateBinaryWithCaps(
                                  MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return tracker->sw.pending_sem != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void esp_display_present_tracker_deinit_switch(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL) {
        return;
    }
    if (tracker->sw.pending_sem != NULL) {
        vSemaphoreDeleteWithCaps(tracker->sw.pending_sem);
        tracker->sw.pending_sem = NULL;
    }
}

void esp_display_present_tracker_mark_submit(
    esp_display_present_frame_tracker_t *tracker)
{
    if (!tracker) {
        return;
    }

    portENTER_CRITICAL(&tracker->lock);
    ++tracker->sw.armed_submit_count;
    portEXIT_CRITICAL(&tracker->lock);
}

bool esp_display_present_tracker_consume_submit_isr(
    esp_display_present_frame_tracker_t *tracker,
    bool submit_gate_enabled)
{
    if (!submit_gate_enabled) {
        return true;
    }

    if (!tracker) {
        return false;
    }

    bool submit_pending = false;
    portENTER_CRITICAL_ISR(&tracker->lock);
    submit_pending = tracker->sw.armed_submit_count != 0;
    if (submit_pending) {
        --tracker->sw.armed_submit_count;
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);

    return submit_pending;
}

void esp_display_present_tracker_set_pending(
    esp_display_present_frame_tracker_t *tracker,
    void *frame_buffer)
{
    if (!tracker) {
        return;
    }

    portENTER_CRITICAL(&tracker->lock);
    tracker->sw.pending_frame_buffer = frame_buffer;
    portEXIT_CRITICAL(&tracker->lock);
}

void esp_display_present_tracker_set_pending_isr(
    esp_display_present_frame_tracker_t *tracker,
    void *frame_buffer)
{
    if (!tracker) {
        return;
    }

    portENTER_CRITICAL_ISR(&tracker->lock);
    tracker->sw.pending_frame_buffer = frame_buffer;
    portEXIT_CRITICAL_ISR(&tracker->lock);
}

bool esp_display_present_tracker_has_pending(
    esp_display_present_frame_tracker_t *tracker)
{
    if (!tracker) {
        return false;
    }

    bool pending = false;
    portENTER_CRITICAL(&tracker->lock);
    pending = (tracker->sw.pending_frame_buffer != NULL);
    portEXIT_CRITICAL(&tracker->lock);

    return pending;
}

static bool esp_display_present_tracker_switch_closing(
    esp_display_present_frame_tracker_t *tracker)
{
    if (!tracker) {
        return true;
    }

    bool closing = false;
    portENTER_CRITICAL(&tracker->lock);
    closing = tracker->sw.closing;
    portEXIT_CRITICAL(&tracker->lock);

    return closing;
}

void esp_display_present_tracker_request_switch_stop(
    esp_display_present_frame_tracker_t *tracker)
{
    if (!tracker || tracker->sw.pending_sem == NULL) {
        return;
    }

    portENTER_CRITICAL(&tracker->lock);
    tracker->sw.closing = true;
    portEXIT_CRITICAL(&tracker->lock);
    (void)xSemaphoreGive(tracker->sw.pending_sem);
}

esp_err_t esp_display_present_tracker_wait_pending(
    esp_display_present_frame_tracker_t *tracker)
{
    if (!tracker || tracker->sw.pending_sem == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    while (esp_display_present_tracker_has_pending(tracker)) {
        portENTER_CRITICAL(&tracker->lock);
        bool closing = tracker->sw.closing;
        portEXIT_CRITICAL(&tracker->lock);
        if (closing) {
            return ESP_ERR_INVALID_STATE;
        }
        (void)xSemaphoreTake(tracker->sw.pending_sem, portMAX_DELAY);
    }
    return ESP_OK;
}

bool esp_display_present_tracker_publish_pending_isr(
    esp_display_present_frame_tracker_t *tracker,
    void **out_frame_buffer)
{
    if (!tracker || !out_frame_buffer) {
        return false;
    }

    bool published = false;
    SemaphoreHandle_t sem = NULL;
    portENTER_CRITICAL_ISR(&tracker->lock);
    if (tracker->sw.pending_frame_buffer) {
        *out_frame_buffer = tracker->sw.pending_frame_buffer;
        tracker->sw.pending_frame_buffer = NULL;
        sem = tracker->sw.pending_sem;
        published = true;
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);

    if (published && sem != NULL) {
        BaseType_t ignored = pdFALSE;
        (void)xSemaphoreGiveFromISR(sem, &ignored);
        (void)ignored;
    }

    return published;
}

/* ---- Pool domain ---- */

static bool pool_buffer_is_managed(esp_display_present_frame_tracker_t *tracker,
                                   void *buffer)
{
    esp_display_present_tracker_buf_t *elem;

    if (!tracker || !buffer) {
        return false;
    }

    STAILQ_FOREACH(elem, &tracker->pool.inflight_queue, entry) {
        if (elem->buffer == buffer) {
            return true;
        }
    }
    STAILQ_FOREACH(elem, &tracker->pool.free_queue, entry) {
        if (elem->buffer == buffer) {
            return true;
        }
    }

    return false;
}

esp_err_t esp_display_present_tracker_build_frame_buffer_plan(
    const esp_display_present_profile_t *profile,
    void **frame_buffers,
    uint8_t frame_buffer_count,
    esp_display_present_tracker_frame_buffer_plan_t *out_plan)
{
    ESP_RETURN_ON_FALSE(profile && frame_buffers && out_plan,
                        ESP_ERR_INVALID_ARG, TAG, "invalid pipeline plan args");

    memset(out_plan, 0, sizeof(*out_plan));

    if (profile->storage != ESP_DISPLAY_PRESENT_STORAGE_PIPELINE_FB ||
            frame_buffer_count == 0) {
        return ESP_OK;
    }

    out_plan->use_pipeline = true;
    out_plan->free_start_index = frame_buffer_count;

    out_plan->required_count = profile->frame_buffer_count;
    if (frame_buffer_count < out_plan->required_count) {
        return ESP_ERR_INVALID_ARG;
    }
    out_plan->disp_fb = frame_buffers[1];
    out_plan->draw_fb = frame_buffers[0];
    out_plan->free_start_index = 2;

    return ESP_OK;
}

esp_err_t esp_display_present_tracker_init_pool(
    esp_display_present_frame_tracker_t *tracker,
    void **frame_buffers,
    uint8_t frame_buffer_count,
    const esp_display_present_tracker_frame_buffer_plan_t *plan,
    bool create_avail_sem,
    uint32_t acquire_timeout_ms)
{
    ESP_RETURN_ON_FALSE(tracker && frame_buffers && plan, ESP_ERR_INVALID_ARG, TAG, "invalid pipeline plan init args");
    ESP_RETURN_ON_FALSE(plan->use_pipeline, ESP_ERR_INVALID_ARG, TAG, "pipeline plan is inactive");
    ESP_RETURN_ON_FALSE(frame_buffer_count >= plan->required_count, ESP_ERR_INVALID_ARG,
                        TAG, "pipeline requires %d buffers, got %d", plan->required_count, frame_buffer_count);

    tracker->lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    memset(&tracker->pool, 0, sizeof(tracker->pool));
    tracker->pool.acquire_timeout_ms =
        acquire_timeout_ms != 0
        ? acquire_timeout_ms
        : ESP_DISPLAY_PRESENT_DEFAULT_PIPELINE_ACQUIRE_TIMEOUT_MS;
    STAILQ_INIT(&tracker->pool.inflight_queue);
    STAILQ_INIT(&tracker->pool.free_queue);

    if (create_avail_sem) {
        tracker->pool.avail_sem = xSemaphoreCreateCounting(frame_buffer_count, 0);
        ESP_RETURN_ON_FALSE(tracker->pool.avail_sem, ESP_ERR_NO_MEM, TAG, "failed to create pipeline semaphore");
        tracker->pool.retire_sem = xSemaphoreCreateBinary();
        if (!tracker->pool.retire_sem) {
            vSemaphoreDelete(tracker->pool.avail_sem);
            tracker->pool.avail_sem = NULL;
            return ESP_ERR_NO_MEM;
        }
    }

    tracker->pool.elem_count = frame_buffer_count;
    tracker->pool.elems = calloc(frame_buffer_count, sizeof(*tracker->pool.elems));
    if (!tracker->pool.elems) {
        if (tracker->pool.avail_sem) {
            vSemaphoreDelete(tracker->pool.avail_sem);
            tracker->pool.avail_sem = NULL;
        }
        if (tracker->pool.retire_sem) {
            vSemaphoreDelete(tracker->pool.retire_sem);
            tracker->pool.retire_sem = NULL;
        }
        return ESP_ERR_NO_MEM;
    }

    for (uint8_t i = 0; i < frame_buffer_count; i++) {
        tracker->pool.elems[i].buffer = frame_buffers[i];
    }

    for (uint8_t i = plan->free_start_index; i < frame_buffer_count; i++) {
        STAILQ_INSERT_TAIL(&tracker->pool.free_queue, &tracker->pool.elems[i], entry);
        if (tracker->pool.avail_sem) {
            (void)xSemaphoreGive(tracker->pool.avail_sem);
        }
    }

    return ESP_OK;
}

void esp_display_present_tracker_deinit_pool(
    esp_display_present_frame_tracker_t *tracker)
{
    if (!tracker) {
        return;
    }

    free(tracker->pool.elems);
    tracker->pool.elems = NULL;
    tracker->pool.elem_count = 0;

    if (tracker->pool.avail_sem) {
        vSemaphoreDelete(tracker->pool.avail_sem);
        tracker->pool.avail_sem = NULL;
    }
    if (tracker->pool.retire_sem) {
        vSemaphoreDelete(tracker->pool.retire_sem);
        tracker->pool.retire_sem = NULL;
    }

    STAILQ_INIT(&tracker->pool.inflight_queue);
    STAILQ_INIT(&tracker->pool.free_queue);
}

void esp_display_present_tracker_pool_commit(
    esp_display_present_frame_tracker_t *tracker,
    void *retired_buffer,
    void *new_display_buffer,
    uint64_t ticket)
{
    if (!tracker || !retired_buffer || !new_display_buffer || ticket == 0 ||
            !tracker->pool.elems) {
        return;
    }

    portENTER_CRITICAL(&tracker->lock);
    if (pool_buffer_is_managed(tracker, retired_buffer)) {
        portEXIT_CRITICAL(&tracker->lock);
        return;
    }

    for (uint8_t i = 0; i < tracker->pool.elem_count; i++) {
        if (tracker->pool.elems[i].buffer == retired_buffer) {
            tracker->pool.elems[i].new_display_buffer = new_display_buffer;
            tracker->pool.elems[i].ticket = ticket;
            tracker->pool.elems[i].release_on_retire = true;
            STAILQ_INSERT_TAIL(&tracker->pool.inflight_queue,
                               &tracker->pool.elems[i], entry);
            break;
        }
    }
    portEXIT_CRITICAL(&tracker->lock);
}

esp_display_present_tracker_buf_t *esp_display_present_tracker_pool_take_free(
    esp_display_present_frame_tracker_t *tracker)
{
    esp_display_present_tracker_buf_t *next = NULL;

    if (!tracker || !tracker->pool.elems) {
        return NULL;
    }

    portENTER_CRITICAL(&tracker->lock);
    next = STAILQ_FIRST(&tracker->pool.free_queue);
    if (next) {
        STAILQ_REMOVE_HEAD(&tracker->pool.free_queue, entry);
    }
    portEXIT_CRITICAL(&tracker->lock);

    return next;
}

esp_display_present_tracker_buf_t *esp_display_present_tracker_pool_acquire(
    esp_display_present_frame_tracker_t *tracker,
    TickType_t ticks_to_wait)
{
    esp_display_present_tracker_buf_t *next = NULL;

    if (!tracker || !tracker->pool.avail_sem) {
        return NULL;
    }

    if (xSemaphoreTake(tracker->pool.avail_sem, ticks_to_wait) != pdTRUE) {
        return NULL;
    }

    next = esp_display_present_tracker_pool_take_free(tracker);

    if (!next) {
        ESP_LOGW(TAG, "pipeline semaphore signaled without free buffer");
    }

    return next;
}

esp_err_t esp_display_present_tracker_pool_acquire_next(
    esp_display_present_frame_tracker_t *tracker,
    esp_display_present_tracker_buf_t **out_next)
{
    if (!tracker || !out_next) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_next = NULL;

    if (tracker->pool.avail_sem != NULL) {
        esp_display_present_tracker_buf_t *next = NULL;
        TickType_t waited = 0;
        const TickType_t poll_ticks = pdMS_TO_TICKS(PIPELINE_ACQUIRE_POLL_MS);
        const TickType_t timeout_ticks =
            pdMS_TO_TICKS(tracker->pool.acquire_timeout_ms);
        while (next == NULL) {
            if (esp_display_present_tracker_switch_closing(tracker)) {
                return ESP_ERR_INVALID_STATE;
            }
            if (waited >= timeout_ticks) {
                return ESP_ERR_TIMEOUT;
            }
            TickType_t wait_ticks = poll_ticks;
            if (timeout_ticks - waited < wait_ticks) {
                wait_ticks = timeout_ticks - waited;
            }
            next = esp_display_present_tracker_pool_acquire(
                       tracker, wait_ticks);
            waited += wait_ticks;
        }
        *out_next = next;
        return ESP_OK;
    }

    return ESP_ERR_INVALID_STATE;
}

bool esp_display_present_tracker_pool_cancel_commit(
    esp_display_present_frame_tracker_t *tracker,
    void *buffer)
{
    if (!tracker || !buffer || !tracker->pool.elems) {
        return false;
    }
    bool removed = false;
    esp_display_present_tracker_buf_t *elem;
    portENTER_CRITICAL(&tracker->lock);
    STAILQ_FOREACH(elem, &tracker->pool.inflight_queue, entry) {
        if (elem->buffer == buffer) {
            STAILQ_REMOVE(&tracker->pool.inflight_queue, elem,
                          esp_display_present_tracker_buf, entry);
            elem->new_display_buffer = NULL;
            elem->ticket = 0;
            elem->release_on_retire = false;
            removed = true;
            break;
        }
    }
    portEXIT_CRITICAL(&tracker->lock);
    return removed;
}

bool IRAM_ATTR esp_display_present_tracker_pool_retire_isr(
    esp_display_present_frame_tracker_t *tracker,
    void **out_new_display_buffer,
    uint64_t *out_ticket)
{
    esp_display_present_tracker_buf_t *elem;
    SemaphoreHandle_t sem = NULL;
    BaseType_t wake = pdFALSE;

    if (!tracker || !tracker->pool.elems) {
        return false;
    }

    portENTER_CRITICAL_ISR(&tracker->lock);
    elem = STAILQ_FIRST(&tracker->pool.inflight_queue);
    if (elem) {
        STAILQ_REMOVE_HEAD(&tracker->pool.inflight_queue, entry);
        if (out_new_display_buffer != NULL) {
            *out_new_display_buffer = elem->new_display_buffer;
        }
        if (out_ticket != NULL) {
            *out_ticket = elem->ticket;
        }
        bool release_buffer = elem->release_on_retire;
        elem->new_display_buffer = NULL;
        elem->ticket = 0;
        elem->release_on_retire = false;
        if (release_buffer) {
            STAILQ_INSERT_TAIL(&tracker->pool.free_queue, elem, entry);
            sem = tracker->pool.avail_sem;
        }
    }
    portEXIT_CRITICAL_ISR(&tracker->lock);

    if (sem) {
        xSemaphoreGiveFromISR(sem, &wake);
    }
    if (elem != NULL && tracker->pool.retire_sem != NULL) {
        xSemaphoreGiveFromISR(tracker->pool.retire_sem, &wake);
    }
    if (wake == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    return elem != NULL;
}

bool esp_display_present_tracker_pool_has_inflight(
    const esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL || tracker->pool.elems == NULL) {
        return false;
    }
    portENTER_CRITICAL_ISR((portMUX_TYPE *)&tracker->lock);
    bool pending = STAILQ_FIRST(&tracker->pool.inflight_queue) != NULL;
    portEXIT_CRITICAL_ISR((portMUX_TYPE *)&tracker->lock);
    return pending;
}

esp_err_t esp_display_present_tracker_pool_wait_retired(
    esp_display_present_frame_tracker_t *tracker)
{
    if (tracker == NULL || tracker->pool.retire_sem == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    const TickType_t timeout_ticks =
        pdMS_TO_TICKS(tracker->pool.acquire_timeout_ms);
    const TickType_t start = xTaskGetTickCount();
    while (esp_display_present_tracker_pool_has_inflight(tracker)) {
        if (esp_display_present_tracker_switch_closing(tracker)) {
            return ESP_ERR_INVALID_STATE;
        }
        TickType_t waited = xTaskGetTickCount() - start;
        if (waited >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }
        (void)xSemaphoreTake(tracker->pool.retire_sem,
                             timeout_ticks - waited);
    }
    return ESP_OK;
}
