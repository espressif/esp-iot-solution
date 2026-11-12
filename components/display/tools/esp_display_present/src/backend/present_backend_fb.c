/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_engine.h"
#include "pixel/present_pixel_ops.h"
#include "backend/present_backend_config.h"
#include "backend/present_backend_validate.h"
#include "backend/present_backend.h"
#include "resources/present_memory.h"
#include "platform/present_os.h"
#include "platform/present_limits.h"
#include "backend/present_frame_core.h"
#include "backend/present_transaction.h"
#include "backend/present_image_repair.h"
#include <string.h>

enum {
    FB_LIMIT = 3,
    RECORD_LIMIT = PRESENT_MAX_FRAMES,
};

/*
 * Explicit single-backend FB pipeline stages (advanced by present_fb_service):
 *
 * 1. FB_SLOT_PREFILL:
 *    - Entry: begin() selects a free slot when DIRECT inherits a different baseline.
 *    - Holds: writes slot.surface, reads the immutable physical baseline.
 *    - Advance: prefill copy ticket reaches terminal state -> FB_SLOT_DRAWING.
 *    - Cancel/Error: drain prefill ticket, then return slot to FREE/SCANNING.
 *
 * 2. FB_SLOT_DRAWING:
 *    - Entry: begin() (no prefill) or FB_SLOT_PREFILL completes.
 *    - Holds: writes the physical image for PARTIAL, otherwise the logical surface.
 *    - Advance: caller invokes end()/end_ex() -> FB_SLOT_DRAIN_TILE.
 *    - Cancel/Error: FB_SLOT_CANCELING drains in-flight tile copy -> FREE/SCANNING.
 *
 * 3. FB_SLOT_DRAIN_TILE:
 *    - Entry: end()/end_ex() seals producer writes.
 *    - Holds: retains each asynchronous tile source until its copy terminates.
 *    - Advance: last tile copy finishes and coverage is verified -> FB_SLOT_REPAIR.
 *    - Error: fails frame record, records sticky fault, returns slot to FREE/SCANNING.
 *
 * 4. FB_SLOT_REPAIR:
 *    - Entry: FB_SLOT_DRAIN_TILE verified coverage.
 *    - Holds: reads the physical baseline, writes history damage minus actual writes.
 *    - Advance: all required spans repaired -> FB_SLOT_CONVERT.
 *    - Error: fails frame record, records sticky fault, returns slot to FREE/SCANNING.
 *
 * 5. FB_SLOT_CONVERT:
 *    - Entry: FB_SLOT_REPAIR completes.
 *    - Holds: FULL/DIRECT read the shared logical canvas and write physical slot.scan.
 *      PARTIAL tiles already have physical orientation; no full-frame rotation.
 *    - Advance: rotation copy finishes (or 0-deg) -> publishes baseline & FB_SLOT_READY.
 *    - Error: fails frame record, records sticky fault, returns slot to FREE/SCANNING.
 *
 * 6. FB_SLOT_READY:
 *    - Entry: candidate is fully prepared and immutable (baseline published).
 *    - Holds: slot.surface (as baseline) and physical scanout target.
 *    - Advance: pending switch slot is idle -> panel.submit -> FB_SLOT_PENDING.
 *    - Error: prior channel fault aborts queued READY frames -> FREE.
 *
 * 7. FB_SLOT_SUBMITTING / FB_SLOT_PENDING:
 *    - Entry: panel.submit called / accepted.
 *    - Holds: hardware may read both old scanning slot and new pending slot.
 *    - Advance: matching ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED -> retires old scanning slot
 *      to FB_SLOT_FREE and promotes pending slot to FB_SLOT_SCANNING.
 *
 * 8. FB_SLOT_SCANNING:
 *    - Entry: scan_buf or confirmed switch.
 *    - Holds: continuous scanout borrow until replaced by a confirmed newer frame
 *      or stopped via panel.quiesce.
 */
typedef enum {
    FB_SLOT_FREE,
    FB_SLOT_PREFILL,
    FB_SLOT_DRAWING,
    FB_SLOT_CANCELING,
    FB_SLOT_DRAIN_TILE,
    FB_SLOT_REPAIR,
    FB_SLOT_CONVERT,
    FB_SLOT_READY,
    FB_SLOT_SUBMITTING,
    FB_SLOT_PENDING,
    FB_SLOT_SCANNING,
} fb_state_t;

typedef struct {
    esp_display_present_surface_t surface;
    esp_display_present_surface_t scan;
    bool draw_owned;
    bool scan_owned;
    fb_state_t state;
    size_t record;
    uint64_t sequence;
    uint64_t cookie;
    bool confirmed;
    bool require_full_redraw;
    present_transaction_t transaction;
} fb_slot_t;

typedef present_core_frame_t fb_record_t;

typedef struct {
    void *pixels;
    esp_display_present_lease_t lease;
    esp_display_present_copy_ticket_t copy;
    esp_display_present_rect_t area;
} fb_tile_t;

struct present_backend {
    present_lock_t lock;
    esp_display_present_backend_config_t config;
    esp_display_present_pixel_engine_t *pixel;
    fb_slot_t slots[FB_LIMIT];
    present_frame_core_t core;
    size_t active;
    size_t pending;
    size_t scanning;
    size_t baseline;
    size_t bytes;
    size_t tile_bytes;
    present_image_builder_t image;
    fb_tile_t *tiles;
    size_t tile_count;
    size_t next_tile;
    esp_display_present_lease_t lease;
    esp_display_present_copy_ticket_t copy;
    esp_display_present_rect_t copy_area;
    bool copy_is_tile;
    size_t repair_pos;
    bool scan_storage;
    bool converted;
    bool damage_declared;
    bool resolved_direct;
    bool image_physical;
    uint64_t sequence;

    bool submitted;

};

static bool valid_area(present_backend_t *backend, const esp_display_present_rect_t *area)
{
    return present_valid_rect(backend->config.width, backend->config.height, area);
}

static void PRESENT_ISR_ATTR fault(present_backend_t *backend, size_t index, esp_err_t error)
{
    if (error == ESP_OK) {
        return;
    }
    uint64_t token = 0, cookie = 0;
    if (index != SIZE_MAX) {
        fb_slot_t *slot = &backend->slots[index];
        fb_record_t *record = &backend->core.frames[slot->record];
        token = record->common.token;
        cookie = slot->cookie;
        present_record_fail(&record->common, error);
    }
    present_fault_first(&backend->core.fault, token, cookie, error);
}

static bool PRESENT_ISR_ATTR dispatch(void *context, const esp_display_present_backend_event_t *event, bool isr)
{
    present_backend_t *backend = context;
    if (!backend || !event) {
        return false;
    }
    present_lock_enter(&backend->lock, isr);
    if ((backend->core.life != PRESENT_CORE_QUIESCENT)) {
        if (event->kind == ESP_DISPLAY_PRESENT_EVENT_FAULT && !event->transaction_cookie) {
            fault(backend, backend->pending, event->result == ESP_OK ? ESP_FAIL : event->result);
        } else if (backend->pending != SIZE_MAX) {
            fb_slot_t *slot = &backend->slots[backend->pending];
            if (slot->cookie && slot->cookie == event->transaction_cookie) {
                if (event->kind == ESP_DISPLAY_PRESENT_EVENT_SCANOUT_CONFIRMED) {
                    if (!present_transaction_done(&slot->transaction, event->result)) {
                        fault(backend, backend->pending, ESP_ERR_INVALID_STATE);
                    }
                    if (event->result == ESP_OK) {
                        slot->confirmed = true;
                    } else {
                        fault(backend, backend->pending, event->result);
                    }
                } else if (event->kind == ESP_DISPLAY_PRESENT_EVENT_FAULT) {
                    fault(backend, backend->pending, event->result == ESP_OK ? ESP_FAIL : event->result);
                }
            }
        }
    }
    present_lock_exit(&backend->lock, isr);
    return false;
}

static bool PRESENT_ISR_ATTR fb_on_event_isr(present_backend_t *backend, const esp_display_present_backend_event_t *event)
{
    return dispatch(backend, event, true);
}

static void fb_on_event(present_backend_t *backend, const esp_display_present_backend_event_t *event)
{
    (void)dispatch(backend, event, false);
}

/* Lock held. Receipt completion is independent of framebuffer retirement. */
static void terminal(present_backend_t *backend, size_t index)
{
    fb_record_t *record = &backend->core.frames[backend->slots[index].record];
    present_frame_core_terminal(&backend->core, record);
}

static void cover(present_backend_t *backend, esp_display_present_rect_t area)
{
    esp_err_t error = present_image_cover(&backend->image, area);
    if (error != ESP_OK) {
        present_lock_enter(&backend->lock, false);
        fault(backend, backend->active, error);
        present_lock_exit(&backend->lock, false);
    }
}

/* Reconstruct a logical region from the immutable physical baseline. FULL /
 * DIRECT writers share one logical canvas; physical images never alias it. */
static esp_display_present_copy_request_t baseline_copy(present_backend_t *backend, size_t target, esp_display_present_rect_t area)
{
    if (backend->resolved_direct) {
        /* Resolved DIRECT regions and their repair history are physical. The
         * logical scratch canvas need not be reconstructed from scanout. */
        esp_display_present_surface_t source = backend->scan_storage ? backend->slots[backend->baseline].scan : backend->slots[backend->baseline].surface;
        return (esp_display_present_copy_request_t) {
            .src = {source.pixels, source.capacity_bytes, source.layout},
            .dst = backend->scan_storage ? backend->slots[target].scan : backend->slots[target].surface,
            .src_area = area, .dst_x = area.x, .dst_y = area.y,
            .rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0
        };
    }
    esp_display_present_surface_t source = backend->scan_storage ? backend->slots[backend->baseline].scan : backend->slots[backend->baseline].surface;
    esp_display_present_rect_t src_area = area;
    esp_display_present_engine_rotation_t rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0;
    if (backend->scan_storage) {
        (void)esp_display_present_rotate_rect(backend->config.width, backend->config.height, backend->config.rotation, &area, &src_area);
        rotation = (4 - backend->config.rotation) % 4;
    }
    return (esp_display_present_copy_request_t) {
        .src = {source.pixels, source.capacity_bytes, source.layout},
        .dst = backend->slots[target].surface,
        .src_area = src_area,
        .dst_x = area.x,
        .dst_y = area.y,
        .rotation = rotation
    };
}

static bool copy_done(void *context, esp_display_present_copy_ticket_t ticket, const esp_display_present_copy_status_t *status, bool isr)
{
    (void)ticket;
    (void)status;
    present_backend_t *backend = context;
    return backend->config.on_progress ? backend->config.on_progress(backend->config.progress_user, isr) : false;
}

static bool tiles_pending(const present_backend_t *backend)
{
    for (size_t i = 0; i < backend->tile_count; i++)
        if (backend->tiles[i].copy.value) {
            return true;
        }
    return false;
}
static bool tiles_leased(const present_backend_t *backend)
{
    for (size_t i = 0; i < backend->tile_count; i++)
        if (backend->tiles[i].lease.id.value) {
            return true;
        }
    return false;
}
static esp_display_present_lease_t *find_draw_lease(present_backend_t *backend, uint64_t token, fb_tile_t **tile)
{
    *tile = NULL;
    if (!token) {
        return NULL;
    }
    if (!backend->tile_count) {
        return backend->lease.id.value == token ? &backend->lease : NULL;
    }
    for (size_t i = 0; i < backend->tile_count; i++)
        if (backend->tiles[i].lease.id.value == token) {
            *tile = &backend->tiles[i];
            return &backend->tiles[i].lease;
        }
    return NULL;
}

/* Owner task only. Scratch and candidate remain pinned through terminal copy. */
static void progress_copy(present_backend_t *backend, uint32_t timeout)
{
    for (size_t i = 0; i < backend->tile_count; i++) {
        fb_tile_t *tile = &backend->tiles[i];
        if (!tile->copy.value) {
            continue;
        }
        esp_display_present_copy_status_t status;
        if (esp_display_present_pixel_wait(backend->pixel, tile->copy, timeout, &status) != ESP_OK) {
            continue;
        }
        (void)esp_display_present_pixel_release(backend->pixel, tile->copy);
        tile->copy.value = 0;
        if (status.result == ESP_OK) {
            cover(backend, tile->area);
            backend->submitted = true;
        } else {
            present_lock_enter(&backend->lock, false);
            fault(backend, backend->active, status.result);
            present_lock_exit(&backend->lock, false);
        }
    }
    if (!backend->copy.value) {
        return;
    }
    esp_display_present_copy_status_t status;
    if (esp_display_present_pixel_wait(backend->pixel, backend->copy, 0, &status) != ESP_OK) {
        return;
    }
    (void)esp_display_present_pixel_release(backend->pixel, backend->copy);
    backend->copy.value = 0;
    if (status.result == ESP_OK) {
        if (backend->copy_is_tile) {
            cover(backend, backend->copy_area);
            backend->submitted = true;
        } else if (backend->active != SIZE_MAX && backend->slots[backend->active].state == FB_SLOT_PREFILL) {
            backend->slots[backend->active].state = FB_SLOT_DRAWING;
        }
    } else {
        present_lock_enter(&backend->lock, false);
        fault(backend, backend->active, status.result);
        present_lock_exit(&backend->lock, false);
    }
}

static void fail_active_build(present_backend_t *backend, esp_err_t error)
{
    present_lock_enter(&backend->lock, false);
    size_t idx = backend->active;
    present_image_cancel(&backend->image);
    fault(backend, idx, error);
    terminal(backend, idx);
    backend->slots[idx].state = idx == backend->scanning ? FB_SLOT_SCANNING : FB_SLOT_FREE;
    if (backend->config.buffer_count == 1) {
        backend->baseline = SIZE_MAX;
    }
    backend->active = SIZE_MAX;
    present_lock_exit(&backend->lock, false);
}

/*
 * Advance post-build stages for b->active:
 *   FB_SLOT_CANCELING -> FREE / SCANNING
 *   FB_SLOT_DRAIN_TILE -> FB_SLOT_REPAIR -> FB_SLOT_CONVERT -> FB_SLOT_READY
 */
/* Internal progress is independent of ESP operation errors: a hardware TIMEOUT
 * must never be interpreted as successful asynchronous acceptance. */
typedef enum {
    FB_ADVANCE_IDLE,
    FB_ADVANCE_PENDING,
    FB_ADVANCE_COMPLETE,
    FB_ADVANCE_FAILED,
} fb_advance_state_t;
typedef struct {
    fb_advance_state_t state;
    esp_err_t error;
} fb_advance_result_t;
/* ATTEMPT can reject end() and leave the producer frame open. Once accepted,
 * failures must terminate its receipt and retain a sticky fault. */
typedef enum {
    FB_SEAL_ATTEMPT,
    FB_SEAL_ACCEPTED,
} fb_seal_context_t;

static fb_advance_result_t advance_build_pipeline(present_backend_t *backend, fb_seal_context_t context)
{
    progress_copy(backend, 0);
    if (backend->active == SIZE_MAX) {
        return (fb_advance_result_t) {
            FB_ADVANCE_IDLE, ESP_OK
        };
    }
    fb_slot_t *slot = &backend->slots[backend->active];
    if (slot->state == FB_SLOT_CANCELING) {
        if (backend->copy.value || tiles_pending(backend)) {
            return (fb_advance_result_t) {
                FB_ADVANCE_PENDING, ESP_OK
            };
        }
        present_lock_enter(&backend->lock, false);
        size_t index = backend->active;
        present_image_cancel(&backend->image);
        uint64_t token = backend->core.frames[slot->record].common.token;
        backend->core.frames[slot->record].common.token = 0;
        slot->state = index == backend->scanning ? FB_SLOT_SCANNING : FB_SLOT_FREE;
        if (backend->config.buffer_count == 1) {
            backend->baseline = SIZE_MAX;
        }
        backend->lease.id.value = 0;
        backend->active = SIZE_MAX;
        present_cancel_publish(&backend->core.canceled, token);
        present_lock_exit(&backend->lock, false);
        return (fb_advance_result_t) {
            FB_ADVANCE_COMPLETE, ESP_OK
        };
    }
    if ((backend->core.life != PRESENT_CORE_RUNNING) || slot->state == FB_SLOT_PREFILL || slot->state == FB_SLOT_DRAWING) {
        return (fb_advance_result_t) {
            FB_ADVANCE_IDLE, ESP_OK
        };
    }

    if (backend->copy.value || tiles_pending(backend)) {
        return (fb_advance_result_t) {
            FB_ADVANCE_PENDING, ESP_OK
        };
    }

    present_lock_enter(&backend->lock, false);
    esp_err_t error = backend->core.fault.result;
    present_lock_exit(&backend->lock, false);
    if (error != ESP_OK) {
        if (context == FB_SEAL_ACCEPTED) {
            fail_active_build(backend, error);
        }
        return (fb_advance_result_t) {
            FB_ADVANCE_FAILED, error
        };
    }

    /* Stage 3 -> 4: FB_SLOT_DRAIN_TILE -> FB_SLOT_REPAIR */
    if (slot->state == FB_SLOT_DRAIN_TILE) {
        bool require_full = backend->baseline == SIZE_MAX || backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL ||
                            slot->require_full_redraw;
        if (require_full && !present_image_full(&backend->image)) {
            error = ESP_ERR_INVALID_STATE;
            if (context == FB_SEAL_ACCEPTED) {
                fail_active_build(backend, error);
            }
            return (fb_advance_result_t) {
                FB_ADVANCE_FAILED, error
            };
        }
        if (!backend->submitted) {
            error = ESP_ERR_INVALID_STATE;
            if (context == FB_SEAL_ACCEPTED) {
                fail_active_build(backend, error);
            }
            return (fb_advance_result_t) {
                FB_ADVANCE_FAILED, error
            };
        }
        slot->state = FB_SLOT_REPAIR;
    }

    /* Stage 4 -> 5: FB_SLOT_REPAIR -> FB_SLOT_CONVERT */
    if (slot->state == FB_SLOT_REPAIR) {
        if (backend->config.render_contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL &&
                backend->baseline != SIZE_MAX && backend->baseline != backend->active) {
            present_image_prepare_repair(&backend->image,
                                         backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT && !backend->damage_declared);
            esp_display_present_rect_t area;
            while (present_image_next_repair(&backend->image, &area)) {
                esp_display_present_copy_request_t copy = baseline_copy(backend, backend->active, area);
                if (!backend->config.async_copy) {
                    error = backend->config.copy_execute ? backend->config.copy_execute(backend->config.copy_user, &copy)
                            : esp_display_present_pixel_execute_cpu(&copy);
                    if (error != ESP_OK) {
                        if (context == FB_SEAL_ACCEPTED) {
                            fail_active_build(backend, error);
                        }
                        return (fb_advance_result_t) {
                            FB_ADVANCE_FAILED, error
                        };
                    }
                    present_image_mark_repaired(&backend->image, &area);
                    continue;
                }
                error = esp_display_present_pixel_submit(backend->pixel, &copy, &backend->copy);
                if (error != ESP_OK) {
                    if (context == FB_SEAL_ACCEPTED) {
                        fail_active_build(backend, error);
                    }
                    return (fb_advance_result_t) {
                        FB_ADVANCE_FAILED, error
                    };
                }
                backend->copy_is_tile = false;
                present_image_mark_repaired(&backend->image, &area);
                return (fb_advance_result_t) {
                    FB_ADVANCE_PENDING, ESP_OK
                };
            }
        }
        slot->state = FB_SLOT_CONVERT;
    }

    /* Stage 5 -> 6: FB_SLOT_CONVERT -> FB_SLOT_READY */
    if (slot->state == FB_SLOT_CONVERT) {
        if (backend->scan_storage && !backend->resolved_direct && !backend->converted) {
            esp_display_present_surface_t source = slot->surface;
            esp_display_present_copy_request_t copy = {
                .src = {source.pixels, source.capacity_bytes, source.layout},
                .dst = slot->scan,
                .src_area = {0, 0, backend->config.width, backend->config.height},
                .rotation = backend->config.rotation
            };
            error = esp_display_present_pixel_submit(backend->pixel, &copy, &backend->copy);
            if (error != ESP_OK) {
                if (context == FB_SEAL_ACCEPTED) {
                    fail_active_build(backend, error);
                }
                return (fb_advance_result_t) {
                    FB_ADVANCE_FAILED, error
                };
            }
            backend->copy_is_tile = false;
            backend->converted = true;
            if (backend->config.async_copy) {
                return (fb_advance_result_t) {
                    FB_ADVANCE_PENDING, ESP_OK
                };
            }
            progress_copy(backend, 0);
            present_lock_enter(&backend->lock, false);
            error = backend->core.fault.result;
            present_lock_exit(&backend->lock, false);
            if (error != ESP_OK) {
                if (context == FB_SEAL_ACCEPTED) {
                    fail_active_build(backend, error);
                }
                return (fb_advance_result_t) {
                    FB_ADVANCE_FAILED, error
                };
            }
        }
        if (backend->sequence == UINT64_MAX) {
            error = ESP_ERR_NO_MEM;
            if (context == FB_SEAL_ACCEPTED) {
                fail_active_build(backend, error);
            }
            return (fb_advance_result_t) {
                FB_ADVANCE_FAILED, error
            };
        }
        present_lock_enter(&backend->lock, false);
        error = present_image_commit(&backend->image);
        if (error != ESP_OK) {
            present_lock_exit(&backend->lock, false);
            if (context == FB_SEAL_ACCEPTED) {
                fail_active_build(backend, error);
            }
            return (fb_advance_result_t) {
                FB_ADVANCE_FAILED, error
            };
        }
        slot->sequence = ++backend->sequence;
        slot->state = FB_SLOT_READY;
        backend->baseline = backend->active;
        backend->active = SIZE_MAX;
        present_lock_exit(&backend->lock, false);
        return (fb_advance_result_t) {
            FB_ADVANCE_COMPLETE, ESP_OK
        };
    }
    return (fb_advance_result_t) {
        FB_ADVANCE_IDLE, ESP_OK
    };
}

static esp_err_t present_fb_service(present_backend_t *backend)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)advance_build_pipeline(backend, FB_SEAL_ACCEPTED);
    present_lock_enter(&backend->lock, false);
    if (backend->pending != SIZE_MAX && backend->slots[backend->pending].state == FB_SLOT_PENDING && backend->slots[backend->pending].confirmed) {
        size_t next = backend->pending;
        if (backend->scanning != SIZE_MAX && backend->scanning != next) {
            present_allocation_drop(&backend->image.allocations[backend->scanning], PRESENT_REF_SCANOUT);
            backend->slots[backend->scanning].state = FB_SLOT_FREE;
        }
        present_allocation_drop(&backend->image.allocations[next], PRESENT_REF_TRANSFER);
        if (backend->scanning != next) {
            present_allocation_hold(&backend->image.allocations[next], PRESENT_REF_SCANOUT);
        }
        backend->slots[next].state = FB_SLOT_SCANNING;
        backend->scanning = next;
        backend->pending = SIZE_MAX;
        terminal(backend, next);
    }
    if (backend->core.fault.result != ESP_OK) {
        for (size_t i = 0; i < backend->config.buffer_count; i++) {
            if (backend->slots[i].state == FB_SLOT_READY) {
                fault(backend, i, backend->core.fault.result);
                terminal(backend, i);
                backend->slots[i].state = FB_SLOT_FREE;
            }
        }
    }
    if ((backend->core.life != PRESENT_CORE_RUNNING) || backend->pending != SIZE_MAX || backend->core.fault.result != ESP_OK) {
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    size_t next = SIZE_MAX;
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        if (backend->slots[i].state == FB_SLOT_READY && (next == SIZE_MAX || backend->slots[i].sequence < backend->slots[next].sequence)) {
            next = i;
        }
    }
    if (next == SIZE_MAX) {
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    fb_slot_t *slot = &backend->slots[next];
    slot->cookie = present_new_token();
    if (!slot->cookie) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_NO_MEM;
    }
    slot->confirmed = false;
    present_transaction_prepare(&slot->transaction);
    present_allocation_hold(&backend->image.allocations[next], PRESENT_REF_TRANSFER);
    slot->state = FB_SLOT_SUBMITTING;
    backend->pending = next;
    present_lock_exit(&backend->lock, false);
    const esp_display_present_surface_t *surface = backend->scan_storage ? &slot->scan : &slot->surface;
    esp_display_present_rect_t area = {0, 0, surface->layout.width, surface->layout.height};
    esp_display_present_tx_result_t tx = backend->config.panel.submit(backend->config.panel.context, surface, &area, slot->cookie);
    present_lock_enter(&backend->lock, false);
    present_transaction_resolution_t resolved = present_transaction_resolve(&slot->transaction, tx);
    if (resolved.rejected) {
        present_allocation_drop(&backend->image.allocations[next], PRESENT_REF_TRANSFER);
        slot->state = FB_SLOT_READY;
        slot->cookie = 0;
        backend->pending = SIZE_MAX;
        present_lock_exit(&backend->lock, false);
        return tx.result == ESP_OK ? ESP_FAIL : tx.result;
    }
    fault(backend, next, resolved.result);
    slot->state = FB_SLOT_PENDING;
    /* Early confirmation is consumed by the next pump, not recursively sending. */
    present_lock_exit(&backend->lock, false);
    return ESP_OK;
}

static void free_backend(present_backend_t *backend)
{
    if (backend->pixel) {
        (void)esp_display_present_pixel_stop(backend->pixel, UINT32_MAX);
    }
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        if (backend->slots[i].draw_owned) {
            present_free(backend->slots[i].surface.pixels);
        }
        if (backend->slots[i].scan_owned) {
            present_free(backend->slots[i].scan.pixels);
        }
    }
    if (backend->pixel) {
        (void)esp_display_present_pixel_stop(backend->pixel, 0);
        (void)esp_display_present_pixel_destroy(backend->pixel);
    }
    present_frame_core_deinit(&backend->core);
    if (backend->tiles)
        for (size_t i = 0; i < backend->tile_count; i++) {
            present_free(backend->tiles[i].pixels);
        }
    present_free(backend->tiles);
    present_image_deinit(&backend->image);
    present_lock_destroy(&backend->lock);
    present_free(backend);
}

static esp_err_t fb_create(const esp_display_present_backend_config_t *config, present_backend_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!config || !config->panel.attach || !config->panel.submit || !config->panel.quiesce) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->tile_count > PRESENT_MAX_TILE_BUFFERS || !config->width || !config->height || !config->buffer_count || config->buffer_count > FB_LIMIT ||
            !config->max_frames || config->max_frames > RECORD_LIMIT) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t shared = present_backend_config_validate_shared(config);
    if (shared != ESP_OK) {
        return shared;
    }
    if ((config->rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || config->rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270) || config->in_place ||
            config->render_contract > ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT || config->render_contract < ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ||
            (config->buffer_count == 1 && !config->active_writes)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION && (!config->tile_rows || config->tile_rows > config->height)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bpp = config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (config->width > SIZE_MAX / config->height || (size_t)config->width * config->height > (SIZE_MAX - (PRESENT_BUFFER_ALIGNMENT - 1)) / bpp) {
        return ESP_ERR_INVALID_SIZE;
    }
    size_t pixels = (size_t)config->width * config->height, bytes = pixels * bpp;
    if ((!config->ext_bufs && config->ext_bytes) ||
            (config->ext_bufs && config->ext_bytes < bytes)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->ext_bufs) {
        for (size_t i = 0; i < config->buffer_count; i++) {
            uintptr_t p = (uintptr_t)config->ext_bufs[i];
            if (!p || p % PRESENT_BUFFER_ALIGNMENT || p > UINTPTR_MAX - config->ext_bytes) {
                return ESP_ERR_INVALID_ARG;
            }
            for (size_t j = 0; j < i; j++) {
                uintptr_t q = (uintptr_t)config->ext_bufs[j];
                if (p < q + config->ext_bytes && q < p + config->ext_bytes) {
                    return ESP_ERR_INVALID_ARG;
                }
            }
        }
    }
    present_backend_t *backend = present_alloc(sizeof(*backend));
    if (!backend) {
        return ESP_ERR_NO_MEM;
    }
    if (!present_lock_init(&backend->lock)) {
        present_free(backend);
        return ESP_ERR_NO_MEM;
    }
    backend->config = *config;
    if (present_frame_core_init(&backend->core, config->max_frames) != ESP_OK) {
        free_backend(backend);
        return ESP_ERR_NO_MEM;
    }
    backend->bytes = bytes;
    backend->tile_bytes = config->render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? (size_t)config->width * config->tile_rows * bpp : bytes;
    backend->active = backend->pending = backend->scanning = backend->baseline = SIZE_MAX;
    bool quarter = config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 || config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270;
    bool physical_partial = config->render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION;
    esp_err_t image_error = present_image_init(&backend->image,
                                               physical_partial && quarter ? config->height : config->width,
                                               physical_partial && quarter ? config->width : config->height);
    backend->image.allow_scanout_writes = config->active_writes;
    backend->image_physical = physical_partial;
    if (config->render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
        backend->tile_count = config->tile_count ? config->tile_count : 1;
        backend->tiles = present_alloc(backend->tile_count * sizeof(*backend->tiles));
        if (!backend->tiles) {
            free_backend(backend);
            return ESP_ERR_NO_MEM;
        }
        for (size_t i = 0; i < backend->tile_count; i++) {
            backend->tiles[i].pixels = present_alloc_pixels_in(backend->tile_bytes, config->tile_in_psram);
            if (!backend->tiles[i].pixels) {
                free_backend(backend);
                return ESP_ERR_NO_MEM;
            }
        }
    }
    if (image_error != ESP_OK) {
        free_backend(backend);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < config->buffer_count; i++) {
        backend->slots[i].surface = (esp_display_present_surface_t) {
            config->ext_bufs ? config->ext_bufs[i] : present_alloc_pixels_in(bytes, config->image_in_psram), bytes,
            {config->width, config->height, config->width * bpp, config->format, config->panel_order}
        };
        backend->slots[i].draw_owned = !config->ext_bufs;
        if (!backend->slots[i].surface.pixels) {
            free_backend(backend);
            return ESP_ERR_NO_MEM;
        }
        if (physical_partial) {
            backend->slots[i].surface.layout.width = quarter ? config->height : config->width;
            backend->slots[i].surface.layout.height = quarter ? config->width : config->height;
            backend->slots[i].surface.layout.stride_bytes = backend->slots[i].surface.layout.width * bpp;
        } else if (config->rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || config->draw_order != config->panel_order) {
            bool quarter = config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 || config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270;
            backend->slots[i].scan = backend->slots[i].surface;
            backend->slots[i].scan_owned = backend->slots[i].draw_owned;
            backend->slots[i].scan.layout.width = quarter ? config->height : config->width;
            backend->slots[i].scan.layout.height = quarter ? config->width : config->height;
            backend->slots[i].scan.layout.stride_bytes = backend->slots[i].scan.layout.width * bpp;
            backend->slots[i].surface.layout.byte_order = config->draw_order;
            backend->slots[i].surface.pixels = i ? backend->slots[0].surface.pixels : present_alloc_pixels_in(bytes, config->logical_in_psram);
            backend->slots[i].draw_owned = i == 0;
            if (!backend->slots[i].surface.pixels) {
                free_backend(backend);
                return ESP_ERR_NO_MEM;
            }
            backend->scan_storage = true;
        }
    }
    if (config->scan_buf) {
        if (!config->ext_bufs) {
            free_backend(backend);
            return ESP_ERR_INVALID_ARG;
        }
        for (size_t i = 0; i < config->buffer_count; ++i) {
            if (config->ext_bufs[i] == config->scan_buf) {
                backend->slots[i].state = FB_SLOT_SCANNING;
                backend->scanning = i;
                present_allocation_hold(&backend->image.allocations[i], PRESENT_REF_SCANOUT);
            }
        }
        if (backend->scanning == SIZE_MAX) {
            free_backend(backend);
            return ESP_ERR_INVALID_ARG;
        }
    }
    esp_display_present_pixel_config_t pc = {
        .max_jobs = backend->tile_count ? backend->tile_count : 1,
        .asynchronous = config->async_copy,
        .execute = config->copy_execute,
        .execute_user = config->copy_user,
        .on_event = config->async_copy ? copy_done : NULL,
        .user = backend,
    };
    esp_err_t error = esp_display_present_pixel_create(&pc, &backend->pixel);
    if (error == ESP_OK) {
        error = config->panel.attach(config->panel.context, dispatch, backend);
    }
    if (error != ESP_OK) {
        free_backend(backend);
        return error;
    }
    *out = backend;
    return ESP_OK;
}

static esp_err_t fb_get_caps(present_backend_t *backend, esp_display_present_backend_caps_t *out)
{
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (esp_display_present_backend_caps_t) {
        .contract = backend->config.render_contract,
        .draw_layout = backend->slots[0].surface.layout,
        .drawbuf_capacity_bytes = backend->tile_bytes,
        .buffer_alignment = PRESENT_BUFFER_ALIGNMENT,
        .max_damage_areas = PRESENT_MAX_DAMAGE_AREAS,
        .max_tile_rows = backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? backend->config.tile_rows : backend->config.height,
        .retains_content = true,
        .supports_present_confirmation = true,
        .writes_active_scanout = backend->config.buffer_count == 1
    };
    out->draw_layout.width = backend->config.width;
    out->draw_layout.height = backend->config.height;
    out->draw_layout.stride_bytes = backend->config.width * (backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3);
    out->draw_layout.byte_order = backend->config.draw_order;
    return ESP_OK;
}

static esp_err_t validate_damage(present_backend_t *backend, const esp_display_present_rect_t *areas, size_t count)
{
    return present_validate_damage(backend->config.width, backend->config.height, areas, count);
}

static bool post_seal_stage(fb_state_t state)
{
    return state == FB_SLOT_DRAIN_TILE || state == FB_SLOT_REPAIR || state == FB_SLOT_CONVERT;
}

static esp_err_t fb_begin(present_backend_t *backend, const esp_display_present_frame_request_t *request, esp_display_present_frame_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    out->value = 0;
    if (!backend || !request) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t error = validate_damage(backend, request->damage, request->damage_count);
    if (error != ESP_OK) {
        return error;
    }
    (void)present_fb_service(backend);
    if (backend->active != SIZE_MAX) {
        return post_seal_stage(backend->slots[backend->active].state) ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_STATE;
    }
    error = present_frame_core_admit(&backend->core, &backend->lock, backend->config.width, backend->config.height, request, out);
    if (error != ESP_OK) {
        return error;
    }
    size_t slot = SIZE_MAX, record = SIZE_MAX;
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        if (backend->slots[i].state == FB_SLOT_FREE || (backend->config.buffer_count == 1 && backend->slots[i].state == FB_SLOT_SCANNING)) {
            slot = i;
            break;
        }
    }
    for (size_t i = 0; i < backend->config.max_frames; i++) {
        if (!backend->core.frames[i].common.token) {
            record = i;
            break;
        }
    }
    if (slot == SIZE_MAX || record == SIZE_MAX) {
        return ESP_ERR_NO_MEM;
    }
    uint64_t token = present_new_token();
    if (!token) {
        return ESP_ERR_NO_MEM;
    }
    /* Prepopulate a stable latest sealed frame. Sealed buffers are immutable;
     * single-producer pump cannot retire/reuse it during this CPU copy. */
    bool direct = backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_DIRECT;
    bool different_baseline = backend->baseline != SIZE_MAX && backend->baseline != slot;
    /* A producer that requests a full raster need not read the previous image.
     * The request is not coverage: seal checks actual submitted pixels below. */
    bool require_full_redraw = direct && different_baseline && request->full_redraw;
    bool resolved = direct && request->resolved_pixels;
    bool prefill = direct && !resolved && different_baseline && !request->full_redraw;
    bool selective_direct = prefill && !request->full_redraw && request->damage_count > 0 &&
                            !backend->scan_storage && backend->image.versions[slot] != 0;
    esp_display_present_rect_t prefill_area = {0, 0, backend->config.width, backend->config.height};
    if (selective_direct) {
        uint32_t x1 = request->damage[0].x, y1 = request->damage[0].y;
        uint32_t x2 = x1 + request->damage[0].width, y2 = y1 + request->damage[0].height;
        for (size_t i = 1; i < request->damage_count; ++i) {
            if (request->damage[i].x < x1) {
                x1 = request->damage[i].x;
            }
            if (request->damage[i].y < y1) {
                y1 = request->damage[i].y;
            }
            if (request->damage[i].x + request->damage[i].width > x2) {
                x2 = request->damage[i].x + request->damage[i].width;
            }
            if (request->damage[i].y + request->damage[i].height > y2) {
                y2 = request->damage[i].y + request->damage[i].height;
            }
        }
        prefill_area = (esp_display_present_rect_t) {
            x1, y1, x2 - x1, y2 - y1
        };
    }
    /* Switching producer contracts invalidates history, not the immutable
     * physical baseline or hardware borrows. Unknown history repairs in full. */
    bool physical = resolved || backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION;
    if (backend->image_physical != physical) {
        bool quarter = physical && (backend->config.rotation & 1);
        present_image_reset(&backend->image, quarter ? backend->config.height : backend->config.width,
                            quarter ? backend->config.width : backend->config.height);
        backend->image_physical = physical;
    }
    backend->resolved_direct = resolved;
    error = present_image_begin(&backend->image, slot, backend->baseline);
    if (error != ESP_OK) {
        return error;
    }
    if (prefill && !backend->config.async_copy) {
        esp_display_present_copy_request_t copy = baseline_copy(backend, slot, prefill_area);
        error = esp_display_present_pixel_execute_cpu(&copy);
        if (error != ESP_OK) {
            present_image_cancel(&backend->image);
            return error;
        }
        /* Record the initialized area only. It is not application coverage. */
        present_image_seed(&backend->image, prefill_area);
    }
    present_lock_enter(&backend->lock, false);
    backend->active = slot;
    backend->slots[slot].state = (prefill && backend->config.async_copy) ? FB_SLOT_PREFILL : FB_SLOT_DRAWING;
    backend->slots[slot].record = record;
    backend->slots[slot].require_full_redraw = require_full_redraw;
    backend->core.frames[record] = (fb_record_t) {
        .common = {.token = token}
    };
    present_lock_exit(&backend->lock, false);
    backend->submitted = false;
    backend->repair_pos = 0;
    backend->converted = false;
    backend->damage_declared = selective_direct || resolved;
    if (prefill && backend->config.async_copy) {
        esp_display_present_copy_request_t copy = baseline_copy(backend, slot, prefill_area);
        error = esp_display_present_pixel_submit(backend->pixel, &copy, &backend->copy);
        if (error != ESP_OK) {
            present_lock_enter(&backend->lock, false);
            present_image_cancel(&backend->image);
            backend->core.frames[record].common.token = 0;
            backend->slots[slot].state = FB_SLOT_FREE;
            backend->active = SIZE_MAX;
            present_lock_exit(&backend->lock, false);
            return error;
        }
        present_image_seed(&backend->image, prefill_area);
        backend->copy_is_tile = false;
    }
    out->value = token;
    return ESP_OK;
}

static bool building(present_backend_t *backend, esp_display_present_frame_t frame)
{
    if (!backend || (backend->core.life != PRESENT_CORE_RUNNING) || backend->active == SIZE_MAX || !frame.value) {
        return false;
    }
    fb_state_t st = backend->slots[backend->active].state;
    return (st == FB_SLOT_PREFILL || st == FB_SLOT_DRAWING) &&
           backend->core.frames[backend->slots[backend->active].record].common.token == frame.value;
}

static esp_err_t fb_acquire(present_backend_t *backend, esp_display_present_frame_t frame, const esp_display_present_rect_t *area,
                            uint32_t timeout, esp_display_present_lease_t *out)
{
    (void)timeout;
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (esp_display_present_lease_t) {
        0
    };
    if (!building(backend, frame)) {
        return ESP_ERR_INVALID_STATE;
    }
    progress_copy(backend, 0);
    present_lock_enter(&backend->lock, false);
    bool failed = backend->core.fault.result != ESP_OK;
    present_lock_exit(&backend->lock, false);
    if (failed) {
        return ESP_ERR_INVALID_STATE;
    }
    if (backend->slots[backend->active].state == FB_SLOT_PREFILL || backend->lease.id.value || backend->copy.value) {
        return ESP_ERR_TIMEOUT;
    }
    fb_tile_t *tile = NULL;
    if (backend->tile_count) {
        for (size_t i = 0; i < backend->tile_count; i++) {
            size_t idx = (backend->next_tile + i) % backend->tile_count;
            if (!backend->tiles[idx].lease.id.value && !backend->tiles[idx].copy.value) {
                tile = &backend->tiles[idx];
                backend->next_tile = (idx + 1) % backend->tile_count;
                break;
            }
        }
        if (!tile) {
            return ESP_ERR_TIMEOUT;
        }
    }
    esp_display_present_rect_t actual = {0, 0, backend->config.width, backend->config.height};
    esp_display_present_surface_t surface = backend->slots[backend->active].surface;
    if (area && !valid_area(backend, area)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
        actual = area ? *area : (esp_display_present_rect_t) {
            0, 0, backend->config.width, backend->config.tile_rows
        };
        if (actual.height > backend->config.tile_rows) {
            actual.height = backend->config.tile_rows;
        }
        size_t bpp = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
        surface = (esp_display_present_surface_t) {
            tile->pixels, backend->tile_bytes,
            {actual.width, actual.height, actual.width * bpp, backend->config.format, backend->config.draw_order}
        };
    }
    uint64_t token = present_new_token();
    if (!token) {
        return ESP_ERR_NO_MEM;
    }
    esp_display_present_lease_t *lease = tile ? &tile->lease : &backend->lease;
    *lease = (esp_display_present_lease_t) {
        .id = {token},
        .surface = surface,
        .logical_area = actual
    };
    *out = *lease;
    return ESP_OK;
}

/* Copy only resolved damage out of the logical staging canvas. The single
 * ticket pins that canvas until completion; acquire must not reuse it sooner.
 * Both output coverage and image history use the mapped physical coordinates. */
static esp_err_t submit_resolved_region(present_backend_t *backend, esp_display_present_rect_t area)
{
    fb_slot_t *slot = &backend->slots[backend->active];
    esp_display_present_rect_t physical;
    esp_err_t error = esp_display_present_rotate_rect(backend->config.width, backend->config.height,
                                                      backend->config.rotation, &area, &physical);
    if (error != ESP_OK) {
        return error;
    }
    esp_display_present_copy_request_t copy = {
        .src = {slot->surface.pixels, slot->surface.capacity_bytes, slot->surface.layout},
        .dst = slot->scan, .src_area = area,
        .dst_x = physical.x, .dst_y = physical.y, .rotation = backend->config.rotation
    };
    esp_display_present_copy_ticket_t ticket;
    error = esp_display_present_pixel_submit(backend->pixel, &copy, &ticket);
    if (error != ESP_OK) {
        return error;
    }
    if (backend->config.async_copy) {
        backend->copy = ticket;
        backend->copy_is_tile = true;
        backend->copy_area = physical;
        return ESP_OK;
    }
    esp_display_present_copy_status_t status;
    error = esp_display_present_pixel_wait(backend->pixel, ticket, UINT32_MAX, &status);
    (void)esp_display_present_pixel_release(backend->pixel, ticket);
    if (error != ESP_OK || status.result != ESP_OK) {
        return error != ESP_OK ? error : status.result;
    }
    cover(backend, physical);
    backend->submitted = true;
    return ESP_OK;
}

static esp_err_t fb_submit_region_ex(present_backend_t *backend, esp_display_present_frame_t frame,
                                     esp_display_present_lease_id_t lease, const esp_display_present_region_submit_t *submit)
{
    fb_tile_t *tile = NULL;
    esp_display_present_lease_t *draw = backend ? find_draw_lease(backend, lease.value, &tile) : NULL;
    if (!building(backend, frame) || !draw) {
        return ESP_ERR_INVALID_STATE;
    }
    present_lock_enter(&backend->lock, false);
    bool failed = backend->core.fault.result != ESP_OK;
    present_lock_exit(&backend->lock, false);
    if (failed) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_display_present_rect_t area = draw->logical_area;
    if (submit && (submit->logical_area.x || submit->logical_area.y || submit->logical_area.width || submit->logical_area.height)) {
        area = submit->logical_area;
    }
    if (!valid_area(backend, &area)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bpp = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
        size_t stride = submit && submit->stride_bytes ? submit->stride_bytes : area.width * bpp;
        esp_display_present_rect_t physical;
        esp_err_t map_error = esp_display_present_rotate_rect(backend->config.width, backend->config.height, backend->config.rotation, &area, &physical);
        if (map_error != ESP_OK) {
            return map_error;
        }
        esp_display_present_copy_request_t copy = {
            .src = {tile->pixels, backend->tile_bytes, {area.width, area.height, stride, backend->config.format, backend->config.draw_order}},
            .src_area = {0, 0, area.width, area.height},
            .dst = backend->slots[backend->active].surface,
            .dst_x = physical.x,
            .dst_y = physical.y,
            .rotation = backend->config.rotation
        };
        esp_display_present_copy_ticket_t ticket;
        esp_err_t error = esp_display_present_pixel_submit(backend->pixel, &copy, &ticket);
        if (error != ESP_OK) {
            return error;
        }
        if (backend->config.async_copy) {
            tile->copy = ticket;
            tile->area = physical;
            draw->id.value = 0;
            return ESP_OK;
        }
        esp_display_present_copy_status_t status;
        error = esp_display_present_pixel_wait(backend->pixel, ticket, UINT32_MAX, &status);
        (void)esp_display_present_pixel_release(backend->pixel, ticket);
        if (error != ESP_OK || status.result != ESP_OK) {
            return error != ESP_OK ? error : status.result;
        }
        cover(backend, physical);
    } else {
        if (submit && submit->stride_bytes && submit->stride_bytes != draw->surface.layout.stride_bytes) {
            return ESP_ERR_INVALID_ARG;
        }
        if (backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_FULL &&
                (area.x || area.y || area.width != backend->config.width || area.height != backend->config.height)) {
            return ESP_ERR_INVALID_ARG;
        }
        if (backend->resolved_direct && backend->scan_storage) {
            esp_err_t error = submit_resolved_region(backend, area);
            if (error != ESP_OK) {
                return error;
            }
            draw->id.value = 0;
            return ESP_OK;
        }
        cover(backend, area);
    }
    backend->submitted = true;
    draw->id.value = 0;
    return ESP_OK;
}

static esp_err_t fb_submit_region(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease)
{
    return fb_submit_region_ex(backend, frame, lease, NULL);
}

static esp_err_t fb_cancel_region(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease)
{
    fb_tile_t *tile = NULL;
    esp_display_present_lease_t *draw = backend ? find_draw_lease(backend, lease.value, &tile) : NULL;
    if (!building(backend, frame) || !draw) {
        return ESP_ERR_INVALID_STATE;
    }
    draw->id.value = 0;
    return ESP_OK;
}

static esp_err_t fb_get_draw_buffers(present_backend_t *backend, void **out, size_t capacity, size_t *count, size_t *bytes)
{
    if (!backend || !count || (capacity && !out)) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = backend->tile_count ? backend->tile_count : backend->scan_storage ? 1 : backend->config.buffer_count;
    if (bytes) {
        *bytes = backend->tile_bytes;
    }
    if (capacity < *count) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < *count; i++) {
        out[i] = backend->tile_count ? backend->tiles[i].pixels : backend->slots[i].surface.pixels;
    }
    return ESP_OK;
}

static esp_err_t fb_end_ex(present_backend_t *backend, esp_display_present_frame_t frame,
                           const esp_display_present_frame_coverage_t *coverage, esp_display_present_receipt_t *out)
{
    if (out) {
        out->value = 0;
    }
    if (!building(backend, frame) || backend->lease.id.value || tiles_leased(backend)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (coverage) {
        esp_err_t error = validate_damage(backend, coverage->areas, coverage->area_count);
        if (error != ESP_OK) {
            return error;
        }
    }
    fb_slot_t *slot = &backend->slots[backend->active];
    fb_record_t *record = &backend->core.frames[slot->record];
    slot->state = FB_SLOT_DRAIN_TILE;
    record->common.external = out != NULL;
    record->sealed = true;
    record->pending = 1;
    fb_advance_result_t progress = advance_build_pipeline(backend, FB_SEAL_ATTEMPT);
    if (progress.state == FB_ADVANCE_PENDING) {
        if (out) {
            out->value = frame.value;
        }
        return ESP_OK;
    }
    if (progress.state != FB_ADVANCE_COMPLETE) {
        slot->state = FB_SLOT_DRAWING;
        record->common.external = false;
        record->sealed = false;
        record->pending = 0;
        return progress.state == FB_ADVANCE_FAILED ? progress.error : ESP_ERR_INVALID_STATE;
    }
    if (out) {
        out->value = frame.value;
    }
    return ESP_OK;
}

static esp_err_t fb_end(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_receipt_t *out)
{
    return fb_end_ex(backend, frame, NULL, out);
}

static esp_err_t fb_cancel_frame(present_backend_t *backend, esp_display_present_frame_t frame, uint32_t timeout)
{
    if (!backend || !frame.value) {
        return ESP_ERR_INVALID_ARG;
    }
    if (present_cancel_matches(&backend->core.canceled, frame.value)) {
        return ESP_OK;
    }
    if (backend->active != SIZE_MAX && backend->slots[backend->active].state == FB_SLOT_CANCELING &&
            backend->core.frames[backend->slots[backend->active].record].common.token == frame.value) {
        /* Retry the same cancellation, without reopening drawing. */
    } else if (!building(backend, frame)) {
        return ESP_ERR_INVALID_STATE;
    }
    backend->slots[backend->active].state = FB_SLOT_CANCELING;
    backend->lease.id.value = 0;
    for (size_t i = 0; i < backend->tile_count; i++) {
        backend->tiles[i].lease.id.value = 0;
    }
    uint64_t start = present_now_ms();
    for (;;) {
        (void)advance_build_pipeline(backend, FB_SEAL_ACCEPTED);
        if (present_cancel_matches(&backend->core.canceled, frame.value)) {
            return ESP_OK;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
}

static esp_err_t fb_query(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_frame_status_t *out)
{
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_fb_service(backend);
    return present_frame_core_query(&backend->core, &backend->lock, receipt, true, out);
}

static esp_err_t fb_get_fault(present_backend_t *backend, esp_display_present_backend_fault_t *out)
{
    return backend ? present_frame_core_get_fault(&backend->core, &backend->lock, out) : ESP_ERR_INVALID_ARG;
}

static esp_err_t fb_wait(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_completion_point_t point,
                         uint32_t timeout, esp_display_present_frame_status_t *out)
{
    return present_frame_core_wait(backend, fb_query, receipt, point, true, timeout, out);
}

static esp_err_t fb_release_receipt(present_backend_t *backend, esp_display_present_receipt_t receipt)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_fb_service(backend);
    return present_frame_core_release_receipt(&backend->core, &backend->lock, receipt);
}

static esp_err_t fb_stop(present_backend_t *backend, uint32_t timeout)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((backend->core.life == PRESENT_CORE_QUIESCENT)) {
        return ESP_OK;
    }
    backend->core.life = PRESENT_CORE_CLOSING;
    uint64_t start = present_now_ms();
    esp_err_t error = esp_display_present_pixel_stop(backend->pixel, timeout);
    if (error != ESP_OK) {
        return error;
    }
    progress_copy(backend, 0);
    uint32_t left = present_remaining_ms(start, timeout);
    error = backend->config.panel.quiesce(backend->config.panel.context, left);
    if (error != ESP_OK) {
        return error;
    }
    (void)present_fb_service(backend);
    present_lock_enter(&backend->lock, false);
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        fb_slot_t *slot = &backend->slots[i];
        if (slot->state != FB_SLOT_FREE && slot->state != FB_SLOT_SCANNING) {
            fault(backend, i, ESP_ERR_INVALID_STATE);
            terminal(backend, i);
        }
        slot->state = FB_SLOT_FREE;
        backend->image.allocations[i] = (present_allocation_t) {
            0
        };
    }
    backend->image.published_baseline = SIZE_MAX;
    backend->active = backend->pending = backend->scanning = backend->baseline = SIZE_MAX;
    backend->lease.id.value = 0;
    for (size_t i = 0; i < backend->tile_count; i++) {
        backend->tiles[i].lease.id.value = 0;
    }
    backend->core.life = PRESENT_CORE_QUIESCENT;
    present_lock_exit(&backend->lock, false);
    return ESP_OK;
}

static esp_err_t fb_destroy(present_backend_t *backend)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = present_frame_core_can_destroy(&backend->core);
    if (err != ESP_OK) {
        return err;
    }
    free_backend(backend);
    return ESP_OK;
}

static esp_err_t fb_set_rotation(present_backend_t *backend, esp_display_present_engine_rotation_t rotation)
{
    if (!backend || rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((backend->core.life != PRESENT_CORE_RUNNING) || backend->active != SIZE_MAX || backend->pending != SIZE_MAX) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < backend->config.max_frames; ++i) {
        if (backend->core.frames[i].common.token) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    if (rotation == backend->config.rotation) {
        return ESP_OK;
    }
    bool axes = !backend->config.fixed_rotation && (((unsigned)rotation ^ (unsigned)backend->config.rotation) & 1);
    uint32_t width = axes ? backend->config.height : backend->config.width;
    uint32_t height = axes ? backend->config.width : backend->config.height;
    size_t bpp = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    size_t rows = backend->tile_bytes / ((size_t)width * bpp);
    if (!rows) {
        return ESP_ERR_INVALID_SIZE;
    }
    uint32_t physical_width = rotation & 1 ? height : width;
    uint32_t physical_height = rotation & 1 ? width : height;
    if (backend->config.render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION) {
        for (size_t i = 0; i < backend->config.buffer_count; ++i) {
            backend->slots[i].surface.layout.width = physical_width;
            backend->slots[i].surface.layout.height = physical_height;
            backend->slots[i].surface.layout.stride_bytes = (size_t)physical_width * bpp;
        }
        backend->config.width = width;
        backend->config.height = height;
        backend->config.rotation = rotation;
        backend->config.tile_rows = rows > height ? height : rows;
        backend->baseline = SIZE_MAX;
        present_image_reset(&backend->image, physical_width, physical_height);
        return ESP_OK;
    }
    bool need_scan_storage = rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || backend->config.draw_order != backend->config.panel_order;
    if (!need_scan_storage && backend->scan_storage) {
        /* Drained receipts do not retire current scanout. Preserve the physical
         * pool and each slot's FB_SLOT_SCANNING/FB_SLOT_FREE state; free only logical storage. */
        for (size_t i = 0; i < backend->config.buffer_count; i++) {
            if (backend->slots[i].draw_owned) {
                present_free(backend->slots[i].surface.pixels);
            }
            backend->slots[i].surface = backend->slots[i].scan;
            backend->slots[i].draw_owned = backend->slots[i].scan_owned;
            backend->slots[i].scan = (esp_display_present_surface_t) {
                0
            };
            backend->slots[i].scan_owned = false;
        }
        backend->scan_storage = false;
    } else if (need_scan_storage && !backend->scan_storage) {
        void *draw = present_alloc_pixels_in(backend->bytes, backend->config.logical_in_psram);
        if (!draw) {
            return ESP_ERR_NO_MEM;
        }
        /* Preserve every existing physical scan pointer, including current. */
        for (size_t i = 0; i < backend->config.buffer_count; ++i) {
            backend->slots[i].scan = backend->slots[i].surface;
            backend->slots[i].scan_owned = backend->slots[i].draw_owned;
            backend->slots[i].surface.pixels = draw;
            backend->slots[i].surface.layout.byte_order = backend->config.draw_order;
            backend->slots[i].draw_owned = i == 0;
        }
        backend->scan_storage = true;
    }
    for (size_t i = 0; i < backend->config.buffer_count; ++i) {
        backend->slots[i].surface.layout.width = width;
        backend->slots[i].surface.layout.height = height;
        backend->slots[i].surface.layout.stride_bytes = width * bpp;
        backend->slots[i].surface.layout.byte_order = backend->config.draw_order;
        if (backend->scan_storage) {
            backend->slots[i].scan.layout.width = physical_width;
            backend->slots[i].scan.layout.height = physical_height;
            backend->slots[i].scan.layout.stride_bytes = (size_t)physical_width * bpp;
        }
    }
    backend->config.width = width;
    backend->config.height = height;
    backend->config.rotation = rotation;
    backend->config.tile_rows = rows > height ? height : rows;
    backend->baseline = SIZE_MAX; /* Logical orientation changed: require full redraw. */
    present_image_reset(&backend->image, backend->image_physical ? physical_width : width,
                        backend->image_physical ? physical_height : height);
    return ESP_OK;
}

const present_backend_ops_t present_fb_backend = {
    .name = "fb",
    .service = present_fb_service,
    .create = fb_create,
    .get_caps = fb_get_caps,
    .begin = fb_begin,
    .acquire = fb_acquire,
    .get_draw_buffers = fb_get_draw_buffers,
    .submit_region = fb_submit_region,
    .submit_region_ex = fb_submit_region_ex,
    .cancel_region = fb_cancel_region,
    .end = fb_end,
    .end_ex = fb_end_ex,
    .cancel_frame = fb_cancel_frame,
    .query = fb_query,
    .get_fault = fb_get_fault,
    .wait = fb_wait,
    .release_receipt = fb_release_receipt,
    .on_event_isr = fb_on_event_isr,
    .on_event = fb_on_event,
    .stop = fb_stop,
    .destroy = fb_destroy,
    .set_rotation = fb_set_rotation,
};

size_t present_fb_memory(const esp_display_present_backend_config_t *config)
{
    size_t tiles = config->render_contract == ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION ? (config->tile_count ? config->tile_count : 1) : 0;
    return sizeof(present_backend_t) + tiles * sizeof(fb_tile_t) + config->max_frames * sizeof(present_core_frame_t) +
           present_pixel_memory(tiles ? tiles : 1, config->async_copy);
}
