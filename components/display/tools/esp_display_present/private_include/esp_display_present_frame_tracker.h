/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_display_present_profile.h"
#include "esp_display_present_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES 3U

typedef struct {
    uint64_t ids[ESP_DISPLAY_PRESENT_MAX_INFLIGHT_FRAMES];
    uint8_t head;
    uint8_t count;
} esp_display_present_frame_queue_t;

typedef struct esp_display_present_tracker_buf {
    void *buffer;
    void *new_display_buffer;
    uint64_t ticket;
    bool release_on_retire;
    STAILQ_ENTRY(esp_display_present_tracker_buf) entry;
} esp_display_present_tracker_buf_t;

typedef struct {
    bool use_pipeline;
    void *disp_fb;
    void *draw_fb;
    uint8_t required_count;
    uint8_t free_start_index;
} esp_display_present_tracker_frame_buffer_plan_t;

/**
 * @brief In-flight bookkeeping for one frame producer.
 *
 * One tracker embeds two orthogonal domains: buffer ownership and a private
 * completion-ticket fence. Buffer retirement is driven only by the switch
 * and pool domains; a missing or duplicate ticket must never manufacture a
 * free buffer. All domains share a single ISR-safe lock:
 * every access path (producer task and ISR) takes and releases the lock
 * inside one leaf tracker function, no call path ever nests two domain
 * locks, and all semaphore/notification waits happen outside the critical
 * sections, so the merged lock cannot deadlock or reorder against itself.
 * Fields are public only to allow static embedding; consumers must use the
 * tracker APIs to access or mutate them.
 */
typedef struct {
    portMUX_TYPE lock;

    /* Private fence domain: producer build lifecycle, transfer/present tickets,
     * backpressure tickets, completion counters and the ISR guard. */
    struct {
        TaskHandle_t producer_task;
        esp_display_present_frame_queue_t transfer_queue;
        esp_display_present_frame_queue_t present_queue;
        volatile uint64_t completed_transfer_frame;
        volatile uint64_t completed_present_frame;
        volatile uint32_t active_isr_count;
        uint64_t last_submitted_frame;
        uint64_t building_frame;
        volatile uint16_t pending_transfer_tickets;
        SemaphoreHandle_t completion_event;
        StaticSemaphore_t completion_event_storage;
        bool track_transfer_completion;
        bool accepting_callbacks;
        bool faulted;
        /** First fault wins: later faults must not mask the original cause. */
        esp_display_present_fault_reason_t fault_reason;
        bool initialized;
    } frame;

    /* Switch domain: pending framebuffer publication, frame-done submit
     * gate and the closing latch for shutdown waits. */
    struct {
        void *pending_frame_buffer;
        SemaphoreHandle_t pending_sem;
        uint8_t armed_submit_count;
        bool closing;
    } sw;

    /* Pool domain: framebuffer pool metadata, ordered inflight queue and
     * free-buffer ownership gate. */
    struct {
        STAILQ_HEAD(esp_display_present_tracker_inflight_list,
                    esp_display_present_tracker_buf) inflight_queue;
        STAILQ_HEAD(esp_display_present_tracker_free_list,
                    esp_display_present_tracker_buf) free_queue;
        SemaphoreHandle_t avail_sem;
        /** Given on every retire; lets a producer wait for the queue to drain. */
        SemaphoreHandle_t retire_sem;
        esp_display_present_tracker_buf_t *elems;
        uint8_t elem_count;
        /** Free-buffer acquire budget; resolved from config at pool init. */
        uint32_t acquire_timeout_ms;
    } pool;
} esp_display_present_frame_tracker_t;

/* ---- Frame domain ---- */

void esp_display_present_tracker_init_frame(
    esp_display_present_frame_tracker_t *tracker,
    TaskHandle_t producer_task,
    bool track_transfer_completion);

/** Wake hint for ticket predicates. Buffer ownership never depends on it. */
void IRAM_ATTR esp_display_present_tracker_signal_completion_isr(
    esp_display_present_frame_tracker_t *tracker,
    BaseType_t *need_yield);

/** Wake task-side fence waiters after a worker advances completion state. */
void esp_display_present_tracker_signal_completion(
    esp_display_present_frame_tracker_t *tracker);

/** Wait for a completion edge; callers must re-check their ticket predicate. */
bool esp_display_present_tracker_wait_completion(
    esp_display_present_frame_tracker_t *tracker,
    TickType_t ticks_to_wait);

/** Validate a frame before doing expensive producer-side preparation. */
esp_err_t esp_display_present_tracker_validate_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    TaskHandle_t current_task);

/** Open one producer-side frame build. A frame is published separately. */
esp_err_t esp_display_present_tracker_begin_build(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    TaskHandle_t current_task,
    uint64_t *out_previous_frame);

/** Continue assembling the active frame, whether or not it is published. */
esp_err_t esp_display_present_tracker_continue_build(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    TaskHandle_t current_task);

/** Mark the producer-side assembly of a frame complete. */
esp_err_t esp_display_present_tracker_end_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id);
/**
 * Publish one opened frame to transfer/present fence queues.
 * May be called once per build, before the transport accepts work.
 */
esp_err_t esp_display_present_tracker_publish_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    uint64_t *out_previous_frame);

uint64_t esp_display_present_tracker_get_building_frame(
    const esp_display_present_frame_tracker_t *tracker);

/**
 * Roll back a frame that was not handed to hardware. Once submission is
 * active, the tracker is faulted because callback ownership is ambiguous.
 */
void esp_display_present_tracker_rollback_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    uint64_t previous_frame,
    bool submission_active);

/** Complete fences synchronously for modes without asynchronous callbacks. */
esp_err_t esp_display_present_tracker_complete_frame(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id,
    bool transfer_complete,
    bool present_complete);

bool IRAM_ATTR esp_display_present_tracker_isr_enter(
    esp_display_present_frame_tracker_t *tracker);

void IRAM_ATTR esp_display_present_tracker_isr_leave(
    esp_display_present_frame_tracker_t *tracker);

bool IRAM_ATTR esp_display_present_tracker_complete_transfer_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t *out_frame_id);

/** Record one transport completion without consuming a frame fence. */
bool IRAM_ATTR esp_display_present_tracker_complete_transfer_ticket_isr(
    esp_display_present_frame_tracker_t *tracker);

/** Record one transport submission used for backpressure accounting. */
esp_err_t esp_display_present_tracker_submit_transfer_ticket(
    esp_display_present_frame_tracker_t *tracker);

/** Roll back a ticket when the panel submission was rejected synchronously. */
void esp_display_present_tracker_cancel_transfer_ticket(
    esp_display_present_frame_tracker_t *tracker);

/** Drop transport tickets after a transfer timeout/abandon. */
void esp_display_present_tracker_drop_transfer_tickets(
    esp_display_present_frame_tracker_t *tracker);

uint16_t esp_display_present_tracker_get_pending_transfer_tickets(
    const esp_display_present_frame_tracker_t *tracker);

/** Complete the frame-level transfer fence from ISR after its last ticket. */
bool IRAM_ATTR esp_display_present_tracker_complete_transfer_frame_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id);

bool IRAM_ATTR esp_display_present_tracker_complete_present_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t *out_frame_id);

bool IRAM_ATTR esp_display_present_tracker_complete_present_frame_isr(
    esp_display_present_frame_tracker_t *tracker,
    uint64_t frame_id);

bool IRAM_ATTR esp_display_present_tracker_has_pending_present(
    const esp_display_present_frame_tracker_t *tracker);

/** Fault from task context, recording why the session became terminal. */
void esp_display_present_tracker_mark_faulted(
    esp_display_present_frame_tracker_t *tracker,
    esp_display_present_fault_reason_t reason);

void IRAM_ATTR esp_display_present_tracker_mark_faulted_isr(
    esp_display_present_frame_tracker_t *tracker,
    esp_display_present_fault_reason_t reason);

/** Stop accepting callbacks after all queued fences have completed. */
esp_err_t esp_display_present_tracker_stop(
    esp_display_present_frame_tracker_t *tracker);

/** Drop queued fences and stop. Used when transport abandons in-flight work. */
esp_err_t esp_display_present_tracker_force_stop(
    esp_display_present_frame_tracker_t *tracker);

/**
 * Transfer producer ownership between serial render tasks. No frame build may
 * be active; completed fences remain valid and continue to retire in ISR.
 */
esp_err_t esp_display_present_tracker_rebind_producer_task(
    esp_display_present_frame_tracker_t *tracker, TaskHandle_t producer_task);

TaskHandle_t IRAM_ATTR esp_display_present_tracker_get_producer_task(
    const esp_display_present_frame_tracker_t *tracker);

bool IRAM_ATTR esp_display_present_tracker_tracks_transfer(
    const esp_display_present_frame_tracker_t *tracker);

uint64_t esp_display_present_tracker_get_completed_transfer_frame(
    const esp_display_present_frame_tracker_t *tracker);

uint64_t esp_display_present_tracker_get_completed_present_frame(
    const esp_display_present_frame_tracker_t *tracker);

uint64_t esp_display_present_tracker_get_last_submitted_frame(
    const esp_display_present_frame_tracker_t *tracker);

bool esp_display_present_tracker_is_faulted(
    const esp_display_present_frame_tracker_t *tracker);

esp_display_present_fault_reason_t esp_display_present_tracker_get_fault_reason(
    const esp_display_present_frame_tracker_t *tracker);

/* ---- Switch domain ---- */

esp_err_t esp_display_present_tracker_init_switch(
    esp_display_present_frame_tracker_t *tracker);

void esp_display_present_tracker_deinit_switch(
    esp_display_present_frame_tracker_t *tracker);

void esp_display_present_tracker_mark_submit(
    esp_display_present_frame_tracker_t *tracker);

bool IRAM_ATTR esp_display_present_tracker_consume_submit_isr(
    esp_display_present_frame_tracker_t *tracker,
    bool submit_gate_enabled);

void esp_display_present_tracker_set_pending(
    esp_display_present_frame_tracker_t *tracker,
    void *frame_buffer);

/** ISR-safe restore when publish was taken but the frame must stay pending. */
void IRAM_ATTR esp_display_present_tracker_set_pending_isr(
    esp_display_present_frame_tracker_t *tracker,
    void *frame_buffer);

bool esp_display_present_tracker_has_pending(
    esp_display_present_frame_tracker_t *tracker);

/** Wake pending waiters and reject further waits during endpoint shutdown. */
void esp_display_present_tracker_request_switch_stop(
    esp_display_present_frame_tracker_t *tracker);

esp_err_t esp_display_present_tracker_wait_pending(
    esp_display_present_frame_tracker_t *tracker);

bool IRAM_ATTR esp_display_present_tracker_publish_pending_isr(
    esp_display_present_frame_tracker_t *tracker,
    void **out_frame_buffer);

/* ---- Pool domain ---- */

esp_err_t esp_display_present_tracker_build_frame_buffer_plan(
    const esp_display_present_profile_t *profile,
    void **frame_buffers,
    uint8_t frame_buffer_count,
    esp_display_present_tracker_frame_buffer_plan_t *out_plan);

esp_err_t esp_display_present_tracker_init_pool(
    esp_display_present_frame_tracker_t *tracker,
    void **frame_buffers,
    uint8_t frame_buffer_count,
    const esp_display_present_tracker_frame_buffer_plan_t *plan,
    bool create_avail_sem,
    uint32_t acquire_timeout_ms);

void esp_display_present_tracker_deinit_pool(
    esp_display_present_frame_tracker_t *tracker);

void esp_display_present_tracker_pool_commit(
    esp_display_present_frame_tracker_t *tracker,
    void *retired_buffer,
    void *new_display_buffer,
    uint64_t ticket);

esp_display_present_tracker_buf_t *esp_display_present_tracker_pool_take_free(
    esp_display_present_frame_tracker_t *tracker);

esp_display_present_tracker_buf_t *esp_display_present_tracker_pool_acquire(
    esp_display_present_frame_tracker_t *tracker,
    TickType_t ticks_to_wait);

esp_err_t esp_display_present_tracker_pool_acquire_next(
    esp_display_present_frame_tracker_t *tracker,
    esp_display_present_tracker_buf_t **out_next);

bool esp_display_present_tracker_pool_cancel_commit(
    esp_display_present_frame_tracker_t *tracker,
    void *buffer);

bool esp_display_present_tracker_pool_retire_isr(
    esp_display_present_frame_tracker_t *tracker,
    void **out_new_display_buffer,
    uint64_t *out_ticket);

bool IRAM_ATTR esp_display_present_tracker_pool_has_inflight(
    const esp_display_present_frame_tracker_t *tracker);

/** Block until every committed buffer has been retired by the ISR. */
esp_err_t esp_display_present_tracker_pool_wait_retired(
    esp_display_present_frame_tracker_t *tracker);

#ifdef __cplusplus
}
#endif
