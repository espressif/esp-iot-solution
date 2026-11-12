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
#include "platform/present_allocation.h"

/* Region submission strategy, independent of panel GRAM vs driver framebuffer.
 * The panel adapter defines source-read completion, not scanout retirement.
 *
 * Draw tiles and transfer tiles have independent lifetimes. A finished copy
 * returns the draw tile while the transfer tile stays owned until the panel
 * transaction really completes. Direct DMA keeps the draw tile instead. */
typedef enum {
    DRAW_FREE = 0, /* present_alloc is calloc; zero means available. */
    DRAW_LEASED,
    DRAW_HELD,
} region_draw_state_t;

typedef enum {
    TX_FREE = 0,
    TX_RESERVED,
    TX_COPYING,
    TX_READY,
    TX_SUBMITTING,
    TX_INFLIGHT,
} region_tx_state_t;

typedef present_core_frame_t frame_record_t;

typedef struct {
    void *pixels;
    present_allocation_t storage;
    esp_display_present_lease_t lease;
    size_t frame;
    region_draw_state_t state;
} region_draw_t;

typedef struct {
    void *converted; /* Owned; NULL until eager or lazy allocation. */
    present_allocation_t storage;
    size_t draw;
    size_t frame;
    bool direct;
    bool draw_held;
    uint64_t cookie;
    region_tx_state_t state;
    present_transaction_t transaction;
    esp_display_present_copy_ticket_t copy;
    esp_display_present_surface_t surface;
    esp_display_present_rect_t area;
} region_tx_t;

struct present_backend {
    present_lock_t lock;
    esp_display_present_backend_config_t config;
    esp_display_present_pixel_engine_t *pixel;
    region_draw_t *draws;
    region_tx_t *txs;
    present_frame_core_t core;
    size_t tile_bytes, next_draw;
};

static bool converted_external(const esp_display_present_backend_config_t *config)
{
    return config->tile_in_psram && !config->tx_staging;
}

static bool valid_area(const present_backend_t *backend, const esp_display_present_rect_t *area)
{
    return present_valid_rect(backend->config.width, backend->config.height, area);
}

static frame_record_t *find_frame(present_backend_t *backend, uint64_t token)
{
    return present_frame_core_find(&backend->core, token);
}

static region_draw_t *find_lease(present_backend_t *backend, uint64_t token, size_t frame)
{
    if (!token) {
        return NULL;
    }
    for (size_t i = 0; i < backend->config.buffer_count; ++i) {
        region_draw_t *draw = &backend->draws[i];
        if (draw->state == DRAW_LEASED && draw->lease.id.value == token && draw->frame == frame) {
            return draw;
        }
    }
    return NULL;
}

/* Called under backend lock; preserve first failure even after auto-recycle. */
static void PRESENT_ISR_ATTR record_fault(present_backend_t *backend, frame_record_t *record,
                                          uint64_t cookie, esp_err_t error)
{
    present_frame_core_fail(&backend->core, record, cookie, error);
}

/* Reservation only. converted and its allocation must survive reuse. */
static region_tx_t *claim_tx(present_backend_t *backend, size_t draw_index, size_t frame)
{
    for (size_t i = 0; i < backend->config.buffer_count; ++i) {
        region_tx_t *tx = &backend->txs[i];
        if (tx->state != TX_FREE) {
            continue;
        }
        tx->draw = draw_index;
        tx->frame = frame;
        tx->direct = false;
        tx->draw_held = false;
        tx->cookie = 0;
        tx->copy.value = 0;
        tx->state = TX_RESERVED;
        return tx;
    }
    return NULL;
}

static void abandon_tx(region_tx_t *tx)
{
    tx->state = TX_FREE;
    tx->cookie = 0;
    tx->direct = false;
    tx->draw_held = false;
    tx->copy.value = 0;
}

static void PRESENT_ISR_ATTR complete_tx(present_backend_t *backend, region_tx_t *tx)
{
    frame_record_t *record = &backend->core.frames[tx->frame];
    if (tx->draw_held) {
        region_draw_t *draw = &backend->draws[tx->draw];
        if (draw->storage.readers[PRESENT_REF_COPY]) {
            present_allocation_drop(&draw->storage, PRESENT_REF_COPY);
        }
        if (tx->direct) {
            present_allocation_drop(&draw->storage, PRESENT_REF_TRANSFER);
        }
        draw->state = DRAW_FREE;
        tx->draw_held = false;
    }
    if (!tx->direct) {
        present_allocation_drop(&tx->storage, PRESENT_REF_TRANSFER);
    }
    tx->state = TX_FREE;
    tx->cookie = 0;
    tx->direct = false;
    tx->copy.value = 0;
    present_frame_core_complete(&backend->core, record);
}

static void settle_submit(present_backend_t *backend, region_tx_t *tx, frame_record_t *record,
                          present_transaction_resolution_t resolved)
{
    record_fault(backend, record, tx->cookie, resolved.result);
    if (resolved.complete) {
        complete_tx(backend, tx);
    } else {
        tx->state = TX_INFLIGHT;
        if (!tx->direct && tx->draw_held) {
            backend->draws[tx->draw].state = DRAW_FREE;
            tx->draw_held = false;
        }
    }
}

static bool copy_done(void *context, esp_display_present_copy_ticket_t ticket, const esp_display_present_copy_status_t *status, bool isr)
{
    (void)ticket;
    (void)status;
    present_backend_t *backend = context;
    return backend->config.on_progress ? backend->config.on_progress(backend->config.progress_user, isr) : false;
}

/* Lock held; ready copies are not hardware transfers. */
static bool channel_full(const present_backend_t *backend)
{
    size_t used = 0, limit = backend->config.transfer_capacity ? backend->config.transfer_capacity : backend->config.buffer_count;
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        used += backend->txs[i].state == TX_SUBMITTING || backend->txs[i].state == TX_INFLIGHT;
    }
    return used >= limit;
}

static esp_err_t present_region_service(present_backend_t *backend)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    for (;;) {
        present_lock_enter(&backend->lock, false);
        region_tx_t *tx = NULL;
        for (size_t i = 0; i < backend->config.buffer_count; ++i) {
            region_tx_t *candidate = &backend->txs[i];
            if ((candidate->state == TX_COPYING || candidate->state == TX_READY) &&
                    (!tx || candidate->cookie < tx->cookie)) {
                tx = candidate;
            }
        }
        if (!tx) {
            present_lock_exit(&backend->lock, false);
            return ESP_OK;
        }
        bool copying = tx->state == TX_COPYING;
        esp_display_present_copy_ticket_t ticket = tx->copy;
        present_lock_exit(&backend->lock, false);
        esp_err_t copy_result = ESP_OK;
        if (copying) {
            esp_display_present_copy_status_t status;
            esp_err_t error = esp_display_present_pixel_wait(backend->pixel, ticket, 0, &status);
            if (error == ESP_ERR_TIMEOUT) {
                return ESP_OK; /* Keep submission order. Do not clear the ticket. */
            }
            if (error != ESP_OK) {
                return error;
            }
            copy_result = status.result;
            (void)esp_display_present_pixel_release(backend->pixel, ticket);
        }
        present_lock_enter(&backend->lock, false);
        frame_record_t *record = &backend->core.frames[tx->frame];
        if (copying) {
            region_draw_t *draw = &backend->draws[tx->draw];
            if (draw->storage.readers[PRESENT_REF_COPY]) {
                present_allocation_drop(&draw->storage, PRESENT_REF_COPY);
            }
            tx->copy.value = 0;
            if (copy_result == ESP_OK && !tx->direct && tx->draw_held) {
                draw->state = DRAW_FREE;
                tx->draw_held = false;
            }
        }
        record_fault(backend, record, tx->cookie, copy_result);
        if (record->canceled || backend->core.life != PRESENT_CORE_RUNNING || backend->core.fault.result != ESP_OK) {
            if (!record->canceled && record->common.result == ESP_OK) {
                record_fault(backend, record, tx->cookie, backend->core.fault.result != ESP_OK ? backend->core.fault.result : ESP_ERR_INVALID_STATE);
            }
            complete_tx(backend, tx);
            present_lock_exit(&backend->lock, false);
            continue;
        }
        if (channel_full(backend)) {
            tx->state = TX_READY;
            present_lock_exit(&backend->lock, false);
            return ESP_ERR_NO_MEM;
        }
        tx->state = TX_SUBMITTING;
        present_transaction_prepare(&tx->transaction);
        uint64_t cookie = tx->cookie;
        present_lock_exit(&backend->lock, false);
        esp_display_present_tx_result_t result = backend->config.panel.submit(backend->config.panel.context, &tx->surface, &tx->area, cookie);
        present_lock_enter(&backend->lock, false);
        record = &backend->core.frames[tx->frame];
        present_transaction_resolution_t resolved = present_transaction_resolve(&tx->transaction, result);
        if (resolved.rejected) {
            tx->state = TX_READY; /* Accepted pixels stay owned; retry by service. */
            present_lock_exit(&backend->lock, false);
            return result.result == ESP_OK ? ESP_FAIL : result.result;
        }
        settle_submit(backend, tx, record, resolved);
        present_lock_exit(&backend->lock, false);
    }
}

static bool PRESENT_ISR_ATTR event_dispatch(void *user, const esp_display_present_backend_event_t *event, bool isr)
{
    present_backend_t *backend = user;
    if (!backend || !event || !event->transaction_cookie) {
        return false;
    }
    present_lock_enter(&backend->lock, isr);
    if (backend->core.life != PRESENT_CORE_QUIESCENT) {
        for (size_t i = 0; i < backend->config.buffer_count; ++i) {
            region_tx_t *tx = &backend->txs[i];
            if (!tx->cookie || tx->cookie != event->transaction_cookie ||
                    (tx->state != TX_SUBMITTING && tx->state != TX_INFLIGHT)) {
                continue;
            }
            frame_record_t *record = &backend->core.frames[tx->frame];
            if (event->kind == ESP_DISPLAY_PRESENT_EVENT_FAULT) {
                record_fault(backend, record, tx->cookie, event->result != ESP_OK ? event->result : ESP_FAIL);
                break; /* Fault does NOT establish that hardware stopped. */
            }
            if (event->kind != ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE) {
                break;
            }
            if (!present_transaction_done(&tx->transaction, event->result)) {
                record_fault(backend, record, tx->cookie, ESP_ERR_INVALID_STATE);
                break;
            }
            record_fault(backend, record, tx->cookie, event->result);
            /* Early completion during SUBMITTING only marks the transaction.
             * The caller completes once when resolve reports complete. */
            if (tx->state == TX_INFLIGHT) {
                complete_tx(backend, tx);
            }
            break;
        }
    }
    present_lock_exit(&backend->lock, isr);
    return backend->config.on_progress ? backend->config.on_progress(backend->config.progress_user, isr) : false;
}

static bool PRESENT_ISR_ATTR region_on_event_isr(present_backend_t *backend, const esp_display_present_backend_event_t *event)
{
    return event_dispatch(backend, event, true);
}

static void region_on_event(present_backend_t *backend, const esp_display_present_backend_event_t *event)
{
    (void)event_dispatch(backend, event, false);
}

static void free_backend(present_backend_t *backend)
{
    if (backend->draws) {
        for (size_t i = 0; i < backend->config.buffer_count; ++i) {
            present_free(backend->draws[i].pixels);
        }
    }
    if (backend->txs) {
        for (size_t i = 0; i < backend->config.buffer_count; ++i) {
            present_free(backend->txs[i].converted);
        }
    }
    if (backend->pixel) {
        (void)esp_display_present_pixel_stop(backend->pixel, UINT32_MAX);
        (void)esp_display_present_pixel_destroy(backend->pixel);
    }
    present_frame_core_deinit(&backend->core);
    present_free(backend->draws);
    present_free(backend->txs);
    present_lock_destroy(&backend->lock);
    present_free(backend);
}

static esp_err_t region_create(const esp_display_present_backend_config_t *config, present_backend_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (config && config->copy_execute && !config->async_copy) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config && ((config->tx_staging && !config->async_copy) ||
                   ((config->async_copy || config->rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0) && config->in_place))) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!config || !config->panel.attach || !config->panel.submit || !config->panel.quiesce) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!config->width || !config->height || !config->tile_rows || config->tile_rows > config->height ||
            !config->buffer_count || config->buffer_count > PRESENT_MAX_TILE_BUFFERS || !config->max_frames || config->max_frames > PRESENT_MAX_FRAMES) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t shared = present_backend_config_validate_shared(config);
    if (shared != ESP_OK) {
        return shared;
    }
    if (config->render_contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION || config->scan_buf || config->ext_bufs || config->ext_bytes) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bytes = config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (config->width > SIZE_MAX / bytes || config->tile_rows > SIZE_MAX / (config->width * bytes)) {
        return ESP_ERR_INVALID_SIZE;
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
    backend->core.active = SIZE_MAX;
    backend->tile_bytes = config->width * bytes * config->tile_rows;
    backend->draws = present_alloc(config->buffer_count * sizeof(*backend->draws));
    backend->txs = present_alloc(config->buffer_count * sizeof(*backend->txs));
    esp_err_t core_error = present_frame_core_init(&backend->core, config->max_frames);
    if (!backend->draws || !backend->txs || core_error != ESP_OK) {
        free_backend(backend);
        return ESP_ERR_NO_MEM;
    }
    /* async_copy alone does not allocate a conversion tile. Identity DMA reuses the draw. */
    bool eager = config->rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || config->tx_staging ||
                 (config->draw_order != config->panel_order && !config->in_place);
    for (size_t i = 0; i < config->buffer_count; ++i) {
        backend->draws[i].pixels = present_alloc_pixels_in(backend->tile_bytes, backend->config.tile_in_psram);
        if (eager) {
            backend->txs[i].converted = present_alloc_pixels_in(backend->tile_bytes, converted_external(config));
        }
        if (!backend->draws[i].pixels || (eager && !backend->txs[i].converted)) {
            free_backend(backend);
            return ESP_ERR_NO_MEM;
        }
    }
    esp_display_present_pixel_config_t pixel_config = {
        .max_jobs = config->buffer_count,
        .asynchronous = config->async_copy,
        .execute = config->copy_execute,
        .execute_user = config->copy_user,
        .on_event = config->async_copy ? copy_done : NULL,
        .user = backend,
    };
    esp_err_t ret = esp_display_present_pixel_create(&pixel_config, &backend->pixel);
    if (ret != ESP_OK) {
        free_backend(backend);
        return ret;
    }
    ret = config->panel.attach(config->panel.context, event_dispatch, backend);
    if (ret != ESP_OK) {
        free_backend(backend);
        return ret;
    }
    *out = backend;
    return ESP_OK;
}

static esp_err_t region_get_caps(present_backend_t *backend, esp_display_present_backend_caps_t *out)
{
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bytes = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    *out = (esp_display_present_backend_caps_t) {
        .contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION,
        .draw_layout = { backend->config.width, backend->config.height, backend->config.width * bytes,
                         backend->config.format, backend->config.draw_order
                       },
        .drawbuf_capacity_bytes = backend->tile_bytes,
        .buffer_alignment = PRESENT_BUFFER_ALIGNMENT,
        .max_tile_rows = backend->config.tile_rows,
        .max_damage_areas = PRESENT_MAX_DAMAGE_AREAS,
    };
    return ESP_OK;
}

static esp_err_t region_begin(present_backend_t *backend, const esp_display_present_frame_request_t *request, esp_display_present_frame_t *out)
{
    if (!backend) {
        if (out) {
            out->value = 0;
        }
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = present_frame_core_admit(&backend->core, &backend->lock, backend->config.width, backend->config.height, request, out);
    if (ret != ESP_OK) {
        return ret;
    }
    present_lock_enter(&backend->lock, false);
    ret = present_frame_core_begin(&backend->core, out);
    present_lock_exit(&backend->lock, false);
    return ret;
}

static esp_err_t region_acquire(present_backend_t *backend, esp_display_present_frame_t frame, const esp_display_present_rect_t *area,
                                uint32_t timeout, esp_display_present_lease_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (esp_display_present_lease_t) {
        0
    };
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_display_present_rect_t default_area = { 0, 0, backend->config.width, backend->config.tile_rows };
    const esp_display_present_rect_t *req_area = area ? area : &default_area;
    if (!valid_area(backend, req_area)) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t start = present_now_ms();
    for (;;) {
        (void)present_region_service(backend);
        present_lock_enter(&backend->lock, false);
        frame_record_t *record = find_frame(backend, frame.value);
        if (backend->core.life != PRESENT_CORE_RUNNING || !record || record->sealed || record->common.result != ESP_OK) {
            present_lock_exit(&backend->lock, false);
            return ESP_ERR_INVALID_STATE;
        }
        for (size_t i = 0; i < backend->config.buffer_count; ++i) {
            region_draw_t *draw = &backend->draws[(backend->next_draw + i) % backend->config.buffer_count];
            if (draw->state != DRAW_FREE) {
                continue;
            }
            uint64_t token = present_new_token();
            if (!token) {
                present_lock_exit(&backend->lock, false);
                return ESP_ERR_NO_MEM;
            }
            backend->next_draw = ((size_t)(draw - backend->draws) + 1) % backend->config.buffer_count;
            esp_display_present_rect_t actual = *req_area;
            if (actual.height > backend->config.tile_rows) {
                actual.height = backend->config.tile_rows;
            }
            size_t bytes = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
            draw->lease = (esp_display_present_lease_t) {
                .id = { token },
                .logical_area = actual,
                .surface = { draw->pixels, backend->tile_bytes,
                    { actual.width, actual.height, actual.width * bytes, backend->config.format, backend->config.draw_order }
                }
            };
            draw->frame = (size_t)(record - backend->core.frames);
            present_allocation_claim(&draw->storage);
            draw->state = DRAW_LEASED;
            ++record->leased;
            *out = draw->lease;
            present_lock_exit(&backend->lock, false);
            return ESP_OK;
        }
        present_lock_exit(&backend->lock, false);
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        if (backend->config.on_progress) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
}

static void revert_in_place_swap(void *pixels, size_t byte_count)
{
    uint8_t *byte_data = pixels;
    size_t i = 0;
    if (((uintptr_t)byte_data & 3u) == 0) {
        uint32_t *p32 = (uint32_t *)byte_data;
        for (; i + 4 <= byte_count; i += 4, ++p32) {
            uint32_t v = *p32;
            *p32 = ((v & 0x00ff00ffu) << 8) | ((v & 0xff00ff00u) >> 8);
        }
    }
    for (; i + 1 < byte_count; i += 2) {
        uint8_t tmp = byte_data[i];
        byte_data[i] = byte_data[i + 1];
        byte_data[i + 1] = tmp;
    }
}

static esp_err_t ensure_converted(present_backend_t *backend, region_tx_t *tx)
{
    if (tx->converted) {
        return ESP_OK;
    }
    tx->converted = present_alloc_pixels_in(backend->tile_bytes, converted_external(&backend->config));
    return tx->converted ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t prepare_pixels(present_backend_t *backend, region_draw_t *draw, region_tx_t *tx,
                                const esp_display_present_rect_t *area, size_t src_stride,
                                esp_display_present_surface_t *out, bool *out_swapped_in_place)
{
    size_t bytes = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    size_t tight_stride = area->width * bytes;
    bool need_swap = backend->config.draw_order != backend->config.panel_order;
    bool need_pack = src_stride != tight_stride;
    bool quarter = backend->config.rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 || backend->config.rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270;
    uint32_t dw = quarter ? area->height : area->width, dh = quarter ? area->width : area->height;
    *out_swapped_in_place = false;
    *out = (esp_display_present_surface_t) {
        .pixels = draw->pixels,
        .capacity_bytes = backend->tile_bytes,
        .layout = { dw, dh, dw * bytes,
                    backend->config.format, backend->config.panel_order
                  },
    };
    if (!need_swap && !need_pack && backend->config.rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0) {
        return ESP_OK;
    }
    if (!need_pack && backend->config.in_place && backend->config.rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 && bytes == 2) {
        revert_in_place_swap(draw->pixels, (size_t)area->height * tight_stride);
        *out_swapped_in_place = true;
        return ESP_OK;
    }
    if (need_pack || !backend->config.in_place || backend->config.rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0) {
        esp_err_t alloc = ensure_converted(backend, tx);
        if (alloc != ESP_OK) {
            return alloc;
        }
        out->pixels = tx->converted;
    }
    esp_display_present_copy_request_t request = {
        .src = {
            .pixels = draw->pixels,
            .capacity_bytes = backend->tile_bytes,
            .layout = {
                area->width, area->height, src_stride,
                backend->config.format, backend->config.draw_order
            },
        },
        .dst = *out,
        .src_area = { 0, 0, area->width, area->height },
        .dst_x = 0,
        .dst_y = 0,
        .rotation = backend->config.rotation,
    };
    esp_display_present_copy_ticket_t ticket;
    esp_err_t ret = esp_display_present_pixel_submit(backend->pixel, &request, &ticket);
    if (ret != ESP_OK) {
        return ret;
    }
    esp_display_present_copy_status_t status;
    ret = esp_display_present_pixel_wait(backend->pixel, ticket, UINT32_MAX, &status);
    if (ret == ESP_OK) {
        ret = status.result;
    }
    (void)esp_display_present_pixel_release(backend->pixel, ticket);
    if (ret == ESP_OK && out->pixels == draw->pixels && need_swap) {
        *out_swapped_in_place = true;
    }
    return ret;
}

typedef struct {
    frame_record_t *record;
    region_draw_t *draw;
    size_t draw_index;
    size_t frame;
    esp_display_present_rect_t submit_area;
    esp_display_present_rect_t physical_area;
    size_t bytes;
    size_t src_stride;
    size_t row_bytes;
} region_submit_context_t;

static bool region_direct_dma(const present_backend_t *backend, size_t src_stride, size_t row_bytes)
{
    return !backend->config.tx_staging && backend->config.rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 &&
           backend->config.draw_order == backend->config.panel_order && src_stride == row_bytes;
}

static void revert_uncommitted(present_backend_t *backend, region_draw_t *draw, region_tx_t *tx, bool swapped, size_t bytes)
{
    present_lock_enter(&backend->lock, false);
    bool revert = swapped && draw->state == DRAW_LEASED;
    abandon_tx(tx);
    present_lock_exit(&backend->lock, false);
    if (revert) {
        revert_in_place_swap(draw->pixels, bytes);
    }
}

/* Lock is not held on return. The direct-sync channel check stays here, before
 * unlock, so a full channel never reaches pixel preparation. */
static esp_err_t region_prepare_submit_context(present_backend_t *backend, esp_display_present_frame_t frame,
                                               esp_display_present_lease_id_t lease, const esp_display_present_region_submit_t *sub,
                                               region_submit_context_t *ctx)
{
    present_lock_enter(&backend->lock, false);
    frame_record_t *record = find_frame(backend, frame.value);
    region_draw_t *draw = record ? find_lease(backend, lease.value, (size_t)(record - backend->core.frames)) : NULL;
    if (backend->core.life != PRESENT_CORE_RUNNING || !record || record->sealed || record->common.result != ESP_OK || !draw) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    esp_display_present_rect_t submit_area = draw->lease.logical_area;
    size_t bytes = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    size_t src_stride = draw->lease.surface.layout.stride_bytes;
    if (sub) {
        if (sub->logical_area.width || sub->logical_area.height ||
                sub->logical_area.x || sub->logical_area.y) {
            submit_area = sub->logical_area;
        }
        if (sub->stride_bytes) {
            src_stride = sub->stride_bytes;
        } else {
            src_stride = submit_area.width * bytes;
        }
    }
    size_t row_bytes = submit_area.width * bytes;
    if (!valid_area(backend, &submit_area) || src_stride < row_bytes || src_stride % bytes ||
            submit_area.height - 1 > (SIZE_MAX - row_bytes) / src_stride ||
            (submit_area.height - 1) * src_stride + row_bytes > backend->tile_bytes) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_INVALID_ARG;
    }
    if (!backend->config.async_copy && !backend->config.deferred_submit && channel_full(backend)) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_NO_MEM;
    }
    present_lock_exit(&backend->lock, false);
    esp_display_present_rect_t physical_area;
    esp_err_t geometry = esp_display_present_rotate_rect(backend->config.width, backend->config.height, backend->config.rotation, &submit_area, &physical_area);
    if (geometry != ESP_OK) {
        return geometry;
    }
    *ctx = (region_submit_context_t) {
        .record = record,
        .draw = draw,
        .draw_index = (size_t)(draw - backend->draws),
        .frame = (size_t)(record - backend->core.frames),
        .submit_area = submit_area,
        .physical_area = physical_area,
        .bytes = bytes,
        .src_stride = src_stride,
        .row_bytes = row_bytes,
    };
    return ESP_OK;
}

static bool submit_source_live(const present_backend_t *backend, const region_submit_context_t *ctx)
{
    return backend->core.life == PRESENT_CORE_RUNNING && ctx->draw->state == DRAW_LEASED && ctx->record->common.result == ESP_OK;
}

static esp_err_t region_submit_async_copy(present_backend_t *backend, const region_submit_context_t *ctx)
{
    uint64_t cookie = present_new_token();
    if (!cookie) {
        return ESP_ERR_NO_MEM;
    }
    present_lock_enter(&backend->lock, false);
    if (!submit_source_live(backend, ctx)) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    region_tx_t *tx = claim_tx(backend, ctx->draw_index, ctx->frame);
    if (!tx) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_NO_MEM;
    }
    present_lock_exit(&backend->lock, false);
    if (ensure_converted(backend, tx) != ESP_OK) {
        present_lock_enter(&backend->lock, false);
        abandon_tx(tx);
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_NO_MEM;
    }
    esp_display_present_surface_t target = {
        .pixels = tx->converted,
        .capacity_bytes = backend->tile_bytes,
        .layout = {
            ctx->physical_area.width, ctx->physical_area.height, ctx->physical_area.width * ctx->bytes,
            backend->config.format, backend->config.panel_order
        },
    };
    esp_display_present_copy_request_t request = {
        .src = {
            .pixels = ctx->draw->pixels,
            .capacity_bytes = backend->tile_bytes,
            .layout = {
                ctx->submit_area.width, ctx->submit_area.height, ctx->src_stride,
                backend->config.format, backend->config.draw_order
            },
        },
        .dst = target,
        .src_area = { 0, 0, ctx->submit_area.width, ctx->submit_area.height },
        .dst_x = 0,
        .dst_y = 0,
        .rotation = backend->config.rotation,
    };
    esp_display_present_copy_ticket_t ticket;
    esp_err_t error = esp_display_present_pixel_submit(backend->pixel, &request, &ticket);
    if (error != ESP_OK) {
        present_lock_enter(&backend->lock, false);
        abandon_tx(tx);
        present_lock_exit(&backend->lock, false);
        return error;
    }
    present_lock_enter(&backend->lock, false);
    if (!submit_source_live(backend, ctx)) {
        present_lock_exit(&backend->lock, false);
        esp_display_present_copy_status_t status;
        (void)esp_display_present_pixel_wait(backend->pixel, ticket, UINT32_MAX, &status);
        (void)esp_display_present_pixel_release(backend->pixel, ticket);
        present_lock_enter(&backend->lock, false);
        abandon_tx(tx);
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    present_allocation_written(&ctx->draw->storage);
    present_allocation_hold(&ctx->draw->storage, PRESENT_REF_COPY);
    present_allocation_hold(&tx->storage, PRESENT_REF_TRANSFER);
    ctx->draw->state = DRAW_HELD;
    tx->draw_held = true;
    tx->direct = false;
    tx->surface = target;
    tx->area = ctx->physical_area;
    tx->cookie = cookie;
    tx->copy = ticket;
    tx->state = TX_COPYING;
    --ctx->record->leased;
    ++ctx->record->pending;
    present_lock_exit(&backend->lock, false);
    return ESP_OK;
}

/* TX is SUBMITTING and the lock is released. Driver rejection restores the
 * lease. An early callback is acceptance, not a retryable rejection. */
static esp_err_t region_submit_panel(present_backend_t *backend, region_submit_context_t *ctx, region_tx_t *tx, bool swapped)
{
    esp_display_present_tx_result_t result = backend->config.panel.submit(backend->config.panel.context, &tx->surface, &tx->area, tx->cookie);
    present_lock_enter(&backend->lock, false);
    frame_record_t *record = &backend->core.frames[tx->frame];
    region_draw_t *draw = &backend->draws[tx->draw];
    present_transaction_resolution_t resolved = present_transaction_resolve(&tx->transaction, result);
    if (resolved.rejected) {
        --record->pending;
        if (tx->direct) {
            present_allocation_drop(&draw->storage, PRESENT_REF_TRANSFER);
        } else {
            present_allocation_drop(&tx->storage, PRESENT_REF_TRANSFER);
        }
        present_allocation_claim(&draw->storage);
        draw->state = DRAW_LEASED;
        abandon_tx(tx);
        present_lock_exit(&backend->lock, false);
        if (swapped) {
            revert_in_place_swap(draw->pixels, ctx->submit_area.height * ctx->row_bytes);
        }
        return result.result != ESP_OK ? result.result : ESP_FAIL;
    }
    --record->leased;
    settle_submit(backend, tx, record, resolved);
    present_lock_exit(&backend->lock, false);
    return ESP_OK;
}

static esp_err_t region_submit_sync_pixels(present_backend_t *backend, region_submit_context_t *ctx)
{
    present_lock_enter(&backend->lock, false);
    if (!submit_source_live(backend, ctx)) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    region_tx_t *tx = claim_tx(backend, ctx->draw_index, ctx->frame);
    if (!tx) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_NO_MEM;
    }
    present_lock_exit(&backend->lock, false);
    esp_display_present_surface_t surface;
    bool swapped = false;
    size_t revert_bytes = ctx->submit_area.height * ctx->row_bytes;
    esp_err_t ret = prepare_pixels(backend, ctx->draw, tx, &ctx->submit_area, ctx->src_stride, &surface, &swapped);
    if (ret != ESP_OK) {
        revert_uncommitted(backend, ctx->draw, tx, swapped, revert_bytes);
        return ret;
    }
    uint64_t cookie = present_new_token();
    if (!cookie) {
        revert_uncommitted(backend, ctx->draw, tx, swapped, revert_bytes);
        return ESP_ERR_NO_MEM;
    }
    present_lock_enter(&backend->lock, false);
    if (!submit_source_live(backend, ctx)) {
        bool revert = swapped && ctx->draw->state == DRAW_LEASED;
        abandon_tx(tx);
        present_lock_exit(&backend->lock, false);
        if (revert) {
            revert_in_place_swap(ctx->draw->pixels, revert_bytes);
        }
        return ESP_ERR_INVALID_STATE;
    }
    if (!backend->config.deferred_submit && channel_full(backend)) {
        bool revert = swapped && ctx->draw->state == DRAW_LEASED;
        abandon_tx(tx);
        present_lock_exit(&backend->lock, false);
        if (revert) {
            revert_in_place_swap(ctx->draw->pixels, revert_bytes);
        }
        return ESP_ERR_NO_MEM;
    }
    bool direct = surface.pixels == ctx->draw->pixels;
    present_allocation_written(&ctx->draw->storage);
    if (direct) {
        present_allocation_hold(&ctx->draw->storage, PRESENT_REF_TRANSFER);
    } else {
        present_allocation_hold(&tx->storage, PRESENT_REF_TRANSFER);
    }
    ctx->draw->state = DRAW_HELD;
    tx->draw_held = true;
    tx->direct = direct;
    tx->surface = surface;
    tx->area = ctx->physical_area;
    tx->cookie = cookie;
    present_transaction_prepare(&tx->transaction);
    ++ctx->record->pending;
    if (backend->config.deferred_submit) {
        if (!direct) {
            ctx->draw->state = DRAW_FREE;
            tx->draw_held = false;
        }
        tx->state = TX_READY;
        --ctx->record->leased;
        present_lock_exit(&backend->lock, false);
        if (backend->config.on_progress) {
            (void)backend->config.on_progress(backend->config.progress_user, false);
        }
        return ESP_OK;
    }
    tx->state = TX_SUBMITTING;
    present_lock_exit(&backend->lock, false);
    return region_submit_panel(backend, ctx, tx, swapped);
}

static esp_err_t region_submit_region_ex(present_backend_t *backend, esp_display_present_frame_t frame,
                                         esp_display_present_lease_id_t lease,
                                         const esp_display_present_region_submit_t *sub)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    region_submit_context_t ctx;
    esp_err_t error = region_prepare_submit_context(backend, frame, lease, sub, &ctx);
    if (error != ESP_OK) {
        return error;
    }
    /* Async identity stays on the sync path and does not call copy_execute.
     * tx_staging still forces a copy into the internal transfer tile. */
    if (backend->config.async_copy && !region_direct_dma(backend, ctx.src_stride, ctx.row_bytes)) {
        return region_submit_async_copy(backend, &ctx);
    }
    return region_submit_sync_pixels(backend, &ctx);
}

static esp_err_t region_submit_region(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease)
{
    return region_submit_region_ex(backend, frame, lease, NULL);
}

static esp_err_t region_cancel_region(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&backend->lock, false);
    frame_record_t *record = find_frame(backend, frame.value);
    region_draw_t *draw = record ? find_lease(backend, lease.value, (size_t)(record - backend->core.frames)) : NULL;
    if (!record || record->sealed || !draw) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_INVALID_STATE;
    }
    --record->leased;
    present_allocation_written(&draw->storage);
    draw->state = DRAW_FREE;
    present_lock_exit(&backend->lock, false);
    return ESP_OK;
}

static esp_err_t region_get_draw_buffers(present_backend_t *backend, void **out_buffers,
                                         size_t max_buffers, size_t *out_count,
                                         size_t *out_buffer_bytes)
{
    if (!backend || !out_count || (max_buffers && !out_buffers)) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_count = backend->config.buffer_count;
    if (out_buffer_bytes) {
        *out_buffer_bytes = backend->tile_bytes;
    }
    if (max_buffers < backend->config.buffer_count) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < backend->config.buffer_count; ++i) {
        out_buffers[i] = backend->draws[i].pixels;
    }
    return ESP_OK;
}

static esp_err_t region_end_ex(present_backend_t *backend, esp_display_present_frame_t frame,
                               const esp_display_present_frame_coverage_t *coverage,
                               esp_display_present_receipt_t *out)
{
    if (out) {
        out->value = 0;
    }
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    if (coverage && present_validate_damage(backend->config.width, backend->config.height,
                                            coverage->areas, coverage->area_count) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&backend->lock, false);
    frame_record_t *record = find_frame(backend, frame.value);
    esp_err_t ret = present_frame_core_seal(&backend->core, record, out);
    present_lock_exit(&backend->lock, false);
    return ret;
}

static esp_err_t region_end(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_receipt_t *out)
{
    return region_end_ex(backend, frame, NULL, out);
}

static void cancel_build(present_backend_t *backend, size_t index)
{
    frame_record_t *record = &backend->core.frames[index];
    record->sealed = true;
    record->canceled = true;
    for (size_t i = 0; i < backend->config.buffer_count; ++i) {
        region_draw_t *draw = &backend->draws[i];
        if (draw->state == DRAW_LEASED && draw->frame == index) {
            present_allocation_written(&draw->storage);
            draw->state = DRAW_FREE;
            --record->leased;
        }
    }
}

static esp_err_t region_cancel_frame(present_backend_t *backend, esp_display_present_frame_t frame, uint32_t timeout)
{
    if (!backend || !frame.value) {
        return ESP_ERR_INVALID_ARG;
    }
    uint64_t start = present_now_ms();
    for (;;) {
        present_lock_enter(&backend->lock, false);
        frame_record_t *record = find_frame(backend, frame.value);
        if (!record) {
            if (present_cancel_matches(&backend->core.canceled, frame.value)) {
                present_lock_exit(&backend->lock, false);
                return ESP_OK;
            }
            present_lock_exit(&backend->lock, false);
            return ESP_ERR_INVALID_STATE;
        }
        if (record->common.external || (record->sealed && !record->canceled)) {
            present_lock_exit(&backend->lock, false);
            return ESP_ERR_INVALID_STATE;
        }
        cancel_build(backend, (size_t)(record - backend->core.frames));
        if (!record->pending) {
            present_frame_core_reap(&backend->core, record);
            present_lock_exit(&backend->lock, false);
            return ESP_OK;
        }
        present_lock_exit(&backend->lock, false);
        (void)present_region_service(backend);
        present_lock_enter(&backend->lock, false);
        bool canceled = present_cancel_matches(&backend->core.canceled, frame.value);
        present_lock_exit(&backend->lock, false);
        if (canceled) {
            return ESP_OK;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    }
}

static esp_err_t region_query(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_frame_status_t *out)
{
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_region_service(backend);
    return present_frame_core_query(&backend->core, &backend->lock, receipt, false, out);
}

static esp_err_t region_get_fault(present_backend_t *backend, esp_display_present_backend_fault_t *out)
{
    return backend ? present_frame_core_get_fault(&backend->core, &backend->lock, out) : ESP_ERR_INVALID_ARG;
}

static esp_err_t region_wait(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_completion_point_t point,
                             uint32_t timeout, esp_display_present_frame_status_t *out)
{
    return present_frame_core_wait(backend, region_query, receipt, point, false, timeout, out);
}

static esp_err_t region_release_receipt(present_backend_t *backend, esp_display_present_receipt_t receipt)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    return present_frame_core_release_receipt(&backend->core, &backend->lock, receipt);
}

static esp_err_t region_stop(present_backend_t *backend, uint32_t timeout)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&backend->lock, false);
    if (backend->core.life == PRESENT_CORE_QUIESCENT) {
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    backend->core.life = PRESENT_CORE_CLOSING;
    if (backend->core.active != SIZE_MAX) {
        cancel_build(backend, backend->core.active);
    }
    present_lock_exit(&backend->lock, false);
    uint64_t start = present_now_ms();
    esp_err_t ret = esp_display_present_pixel_stop(backend->pixel, timeout);
    if (ret != ESP_OK) {
        return ret;
    }
    (void)present_region_service(backend); /* Closing: only retire copies, never submit. */
    uint32_t left = present_remaining_ms(start, timeout);
    ret = backend->config.panel.quiesce(backend->config.panel.context, left);
    if (ret != ESP_OK) {
        return ret;
    }
    /* Adapter now guarantees no access/callback. Pending transfers may have
     * been safely aborted; do not falsely report their frames successful. */
    present_lock_enter(&backend->lock, false);
    for (size_t i = 0; i < backend->config.buffer_count; ++i) {
        region_tx_t *tx = &backend->txs[i];
        if (tx->state == TX_INFLIGHT) {
            frame_record_t *record = &backend->core.frames[tx->frame];
            record_fault(backend, record, tx->cookie, ESP_ERR_INVALID_STATE);
            complete_tx(backend, tx);
        }
    }
    ret = present_frame_core_quiescent(&backend->core);
    present_lock_exit(&backend->lock, false);
    return ret;
}

static esp_err_t region_destroy(present_backend_t *backend)
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

static esp_err_t region_set_rotation(present_backend_t *backend, esp_display_present_engine_rotation_t rotation)
{
    if (!backend || rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270) {
        return ESP_ERR_INVALID_ARG;
    }
    if (backend->core.life != PRESENT_CORE_RUNNING || backend->core.active != SIZE_MAX) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < backend->config.max_frames; ++i)
        if (backend->core.frames[i].common.token) {
            return ESP_ERR_INVALID_STATE;
        }
    if (backend->config.in_place && rotation != ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    bool change_axes = !backend->config.fixed_rotation && (((unsigned)backend->config.rotation ^ (unsigned)rotation) & 1);
    uint32_t width = change_axes ? backend->config.height : backend->config.width;
    uint32_t height = change_axes ? backend->config.width : backend->config.height;
    size_t bpp = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    size_t rows = backend->tile_bytes / ((size_t)width * bpp);
    if (!rows) {
        return ESP_ERR_INVALID_SIZE;
    }
    backend->config.width = width;
    backend->config.height = height;
    backend->config.tile_rows = rows > height ? height : rows;
    backend->config.rotation = rotation;
    return ESP_OK;
}

const present_backend_ops_t present_region_backend = {
    .name = "region",
    .service = present_region_service,
    .create = region_create,
    .get_caps = region_get_caps,
    .begin = region_begin,
    .acquire = region_acquire,
    .get_draw_buffers = region_get_draw_buffers,
    .submit_region = region_submit_region,
    .submit_region_ex = region_submit_region_ex,
    .cancel_region = region_cancel_region,
    .end = region_end,
    .end_ex = region_end_ex,
    .cancel_frame = region_cancel_frame,
    .query = region_query,
    .get_fault = region_get_fault,
    .wait = region_wait,
    .release_receipt = region_release_receipt,
    .on_event_isr = region_on_event_isr,
    .on_event = region_on_event,
    .stop = region_stop,
    .destroy = region_destroy,
    .set_rotation = region_set_rotation,
};

size_t present_region_memory(const esp_display_present_backend_config_t *config)
{
    return sizeof(present_backend_t) + config->buffer_count * (sizeof(region_draw_t) + sizeof(region_tx_t)) +
           config->max_frames * sizeof(present_core_frame_t) + present_pixel_memory(config->buffer_count, config->async_copy);
}
