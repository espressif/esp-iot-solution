/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_engine.h"
#include "pixel/present_pixel_ops.h"
#include "backend/present_backend_config.h"
#include "backend/present_backend.h"
#include "resources/present_memory.h"
#include "platform/present_os.h"
#include "platform/present_limits.h"
#include "backend/present_frame_core.h"
#include "backend/present_transaction.h"
#include "backend/present_te_composer.h"
#include "platform/te/present_te_timer.h"
#include "platform/te/present_te_timing.h"

enum {
    TE_MAX_BUFFERS = PRESENT_MAX_TILE_BUFFERS,
    TE_MAX_FRAMES = PRESENT_MAX_FRAMES,
    TE_MAX_DAMAGE = PRESENT_MAX_DAMAGE_AREAS,
};

/* Frame job phases; tile copy completion and full-frame transfer completion
 * release different buffers. The timer only wakes service to recheck TE. */
typedef enum {
    TE_IDLE,       /* Job record is available. */
    TE_BUILDING,   /* Producer submits tiles to the composer. */
    TE_COPY_DRAIN, /* Sealed frame waits for copies/repair to finish. */
    TE_CANCELING,  /* Drain outstanding copies before discarding the frame. */
    TE_READY,      /* Composed frame awaits TE admission and transfer capacity. */
    TE_SUBMITTING, /* Adapter call in progress; completion may arrive early. */
    TE_INFLIGHT,   /* Await completion before releasing the compose image. */
} te_phase_t;
typedef present_core_frame_t te_record_t;
typedef struct {
    void *pixels;
    size_t frame;
    esp_display_present_copy_ticket_t copy;
    esp_display_present_lease_t lease; /* id == 0 when not borrowed by the producer. */
} te_tile_t;

typedef struct {
    present_te_frame_t compose_frame;
    te_phase_t phase;
    uint64_t cookie;
    present_transaction_t transaction;
    uint64_t ready_us, attempted_edge, started_us;
    bool attempted;
} te_job_t;

struct present_backend {
    present_lock_t lock;
    esp_display_present_backend_config_t config;
    present_te_composer_t *composer; /* Task-only; never accessed by callbacks. */
    present_te_timer_t *phase_timer;

    te_tile_t *tiles;
    te_job_t *jobs;
    present_frame_core_t core;
    size_t tile_bytes, next_tile;

    uint64_t latest_edge;
    bool has_edge;
    bool measured;
};

/* Timer fires in task context and only wakes the owner/backend worker. It neither
 * manufactures a TE edge nor sends hardware; service rechecks the phase. */
static void phase_wakeup(void *user)
{
    present_backend_t *backend = user;
    present_lock_enter(&backend->lock, false);
    bool running = backend->core.life == PRESENT_CORE_RUNNING;
    present_lock_exit(&backend->lock, false);
    if (running && backend->config.on_progress) {
        (void)backend->config.on_progress(backend->config.progress_user, false);
    }
}

static bool valid_area(const present_backend_t *backend, const esp_display_present_rect_t *area)
{
    return present_valid_rect(backend->config.width, backend->config.height, area);
}

/* Lock held. Only accepted operations/channel faults use this sticky record. */
static void PRESENT_ISR_ATTR record_fault(present_backend_t *backend, size_t index, esp_err_t error)
{
    if (error == ESP_OK) {
        return;
    }
    uint64_t token = 0;
    if (index != SIZE_MAX) {
        te_record_t *record = &backend->core.frames[index];
        token = record->common.token;
        present_record_fail(&record->common, error);
    }
    present_fault_first(&backend->core.fault, token, index == SIZE_MAX ? 0 : backend->jobs[index].cookie, error);
}

static bool PRESENT_ISR_ATTR te_dispatch(void *user, const esp_display_present_backend_event_t *event, bool isr)
{
    present_backend_t *backend = user;
    if (!backend || !event) {
        return false;
    }
    present_lock_enter(&backend->lock, isr);
    if (backend->core.life != PRESENT_CORE_QUIESCENT) {
        if (event->kind == ESP_DISPLAY_PRESENT_EVENT_TE_EDGE && backend->core.life == PRESENT_CORE_RUNNING) {
            if (!backend->has_edge || event->timestamp_us > backend->latest_edge) {
                if (backend->config.te_timing.period_us && backend->has_edge) {
                    uint64_t period = event->timestamp_us - backend->latest_edge;
                    uint64_t expected = backend->config.te_timing.period_us;
                    uint64_t delta = period > expected ? period - expected : expected - period;
                    backend->measured = delta <= backend->config.te_timing.jitter_us;
                }
                backend->latest_edge = event->timestamp_us;
                backend->has_edge = true;
            }
        } else if (event->kind == ESP_DISPLAY_PRESENT_EVENT_FAULT && !event->transaction_cookie) {
            esp_err_t error = event->result == ESP_OK ? ESP_FAIL : event->result;
            bool found = false;
            for (size_t i = 0; i < backend->config.max_frames; i++)
                if (backend->jobs[i].phase != TE_IDLE) {
                    record_fault(backend, i, error);
                    found = true;
                }
            if (!found) {
                record_fault(backend, SIZE_MAX, error);
            }
        } else
            for (size_t i = 0; i < backend->config.max_frames; i++) {
                te_job_t *job = &backend->jobs[i];
                if (!job->cookie || job->cookie != event->transaction_cookie ||
                        (job->phase != TE_SUBMITTING && job->phase != TE_INFLIGHT)) {
                    continue;
                }
                if (event->kind == ESP_DISPLAY_PRESENT_EVENT_TRANSFER_DONE) {
                    bool first = present_transaction_done(&job->transaction, event->result);
                    if (first && backend->config.te_gate.done) {
                        backend->config.te_gate.done(backend->config.te_gate.user, event->result);
                    }
                    record_fault(backend, i, first ? event->result : ESP_ERR_INVALID_STATE);
                    if (backend->config.te_timing.period_us && present_now_us() - job->started_us > backend->config.te_budget_us) {
                        record_fault(backend, i, ESP_ERR_TIMEOUT);
                    }
                    job->transaction.done = true;
                } else if (event->kind == ESP_DISPLAY_PRESENT_EVENT_FAULT) {
                    record_fault(backend, i, event->result == ESP_OK ? ESP_FAIL : event->result);
                }
                break;
            }
    }
    present_lock_exit(&backend->lock, isr);
    return false; /* Cooperative pump; no task waiter is registered yet. */
}

static bool PRESENT_ISR_ATTR te_on_event_isr(present_backend_t *backend, const esp_display_present_backend_event_t *event)
{
    return te_dispatch(backend, event, true);
}

static void te_on_event(present_backend_t *backend, const esp_display_present_backend_event_t *event)
{
    (void)te_dispatch(backend, event, false);
}

/* Task-only, lock held, after composer no longer borrows the candidate. */
static void retire_frame(present_backend_t *backend, size_t index)
{
    present_frame_core_terminal(&backend->core, &backend->core.frames[index]);
    if (backend->core.active == index) {
        backend->core.active = SIZE_MAX;
    }
    backend->jobs[index] = (te_job_t) {
        0
    };
}

/* Pixel callback cannot mutate composer/tiles; owner service does that. */
static bool copy_done(void *user, esp_display_present_copy_ticket_t ticket,
                      const esp_display_present_copy_status_t *status, bool isr)
{
    (void)ticket;
    (void)status;
    present_backend_t *backend = user;
    return backend->config.on_progress ? backend->config.on_progress(backend->config.progress_user, isr) : false;
}

static size_t progress_copies(present_backend_t *backend)
{
    size_t pending = 0;
    for (size_t i = 0; i < backend->config.buffer_count; ++i) {
        te_tile_t *tile = &backend->tiles[i];
        if (!tile->copy.value) {
            continue;
        }
        esp_err_t result = ESP_OK;
        esp_err_t error = present_te_composer_finish_write(backend->composer, tile->copy, &result);
        if (error != ESP_OK) {
            pending++;
            continue;
        }
        tile->copy.value = 0;
        if (result != ESP_OK) {
            present_lock_enter(&backend->lock, false);
            record_fault(backend, tile->frame, result);
            present_lock_exit(&backend->lock, false);
        }
    }
    return pending;
}

static esp_err_t service_frame(present_backend_t *backend, size_t index, size_t pending)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    te_job_t *job = &backend->jobs[index];
    if (!pending && (job->phase == TE_COPY_DRAIN || job->phase == TE_CANCELING)) {
        bool cancel = job->phase == TE_CANCELING;
        present_lock_enter(&backend->lock, false);
        esp_err_t error = backend->core.fault.result;
        present_lock_exit(&backend->lock, false);
        if (!cancel && error == ESP_OK && backend->core.life == PRESENT_CORE_RUNNING) {
            error = present_te_composer_seal(backend->composer, job->compose_frame);
        } else if (!cancel && error == ESP_OK) {
            return ESP_OK; /* stop owns drain, never seals or sends. */
        }
        if (error == ESP_ERR_TIMEOUT) {
            return ESP_OK;
        }
        if (cancel || error != ESP_OK) {
            if (present_te_composer_cancel(backend->composer, job->compose_frame) == ESP_ERR_TIMEOUT) {
                return ESP_OK;
            }
            present_lock_enter(&backend->lock, false);
            if (cancel) {
                present_cancel_publish(&backend->core.canceled, job->compose_frame.value);
            } else {
                record_fault(backend, index, error);
            }
            retire_frame(backend, index);
            present_lock_exit(&backend->lock, false);
        } else {
            present_lock_enter(&backend->lock, false);
            job->ready_us = present_now_us();
            job->phase = TE_READY;
            present_lock_exit(&backend->lock, false);
        }
    }
    present_lock_enter(&backend->lock, false);
    if (job->phase == TE_INFLIGHT && job->transaction.done) {
        esp_err_t result = backend->core.frames[index].common.result;
        /* This changes only composer metadata, never copies/frees memory. */
        (void)present_te_composer_complete(backend->composer, job->compose_frame, result);
        retire_frame(backend, index);
    }
    if (backend->core.life != PRESENT_CORE_RUNNING || job->phase != TE_READY) {
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    if (backend->core.fault.result != ESP_OK) {
        record_fault(backend, index, backend->core.fault.result);
        if (present_te_composer_cancel(backend->composer, job->compose_frame) != ESP_OK) {
            present_lock_exit(&backend->lock, false);
            return ESP_OK;
        }
        retire_frame(backend, index);
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    for (size_t i = 0; i < backend->config.max_frames; i++) {
        if (i == index) {
            continue;
        }
        te_job_t *other = &backend->jobs[i];
        if (other->phase == TE_INFLIGHT || other->phase == TE_SUBMITTING ||
                ((other->phase == TE_READY || other->phase == TE_COPY_DRAIN) &&
                 other->compose_frame.value < job->compose_frame.value)) {
            present_lock_exit(&backend->lock, false);
            return ESP_OK;
        }
    }
    if (!backend->has_edge && !backend->config.te_gate.admit) {
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    uint64_t edge = backend->latest_edge;
    bool measured = backend->measured;
    present_lock_exit(&backend->lock, false);
    esp_display_present_source_t source;
    esp_err_t error;
    if (backend->config.te_gate.admit) {
        error = present_te_composer_pin(backend->composer, job->compose_frame, &source);
    } else if (backend->config.te_timing.period_us) {
        if (job->attempted && edge <= job->attempted_edge) {
            return ESP_OK;
        }
        uint64_t now = present_now_us();
        esp_display_present_te_window_t window = esp_display_present_te_window(&backend->config.te_timing,
                                                                               backend->config.te_budget_us, edge, now, measured);
        if (window.admission == ESP_DISPLAY_PRESENT_TE_WAIT && backend->phase_timer) {
            uint64_t opening;
            if (present_te_next_open(&backend->config.te_timing, &window, edge, now, &opening)) {
                error = present_te_timer_arm(backend->phase_timer, opening);
                if (error != ESP_OK) {
                    return error;
                }
            }
        }
        if (window.admission != ESP_DISPLAY_PRESENT_TE_ALLOW) {
            return ESP_OK;
        }
        error = present_te_composer_pin(backend->composer, job->compose_frame, &source);
    } else {
        /* Legacy fixed edge window: no inferred scan model. This frame can
         * use only an actual edge observed after its image became READY. */
        uint64_t now = present_now_us();
        uint64_t window = backend->config.te_window_us - backend->config.te_budget_us;
        if (now < edge || edge < job->ready_us || now - edge >= window ||
                (job->attempted && edge <= job->attempted_edge)) {
            return ESP_OK;
        }
        error = present_te_composer_pin(backend->composer, job->compose_frame, &source);
    }
    if (error == ESP_ERR_TIMEOUT) {
        return ESP_OK;
    }
    if (error != ESP_OK) {
        return error;
    }
    uint64_t cookie = present_new_token();
    if (!cookie) {
        (void)present_te_composer_reject(backend->composer, job->compose_frame);
        return ESP_ERR_NO_MEM;
    }
    if (backend->config.te_gate.admit) {
        /* A custom gate may have side effects: call it exactly once, after
         * pin and cookie allocation, as close to submission as practical. */
        size_t bytes = (size_t)backend->config.width * backend->config.height *
                       (backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3);
        error = backend->config.te_gate.admit(backend->config.te_gate.user, bytes,
                                              job->attempted ? job->attempted_edge : 0, &edge);
        if (error != ESP_OK) {
            (void)present_te_composer_reject(backend->composer, job->compose_frame);
            return error == ESP_ERR_TIMEOUT ? ESP_OK : error;
        }
    }
    present_lock_enter(&backend->lock, false);
    /* Tasks are serialized. A channel fault can arrive while offering. */
    if (backend->core.fault.result != ESP_OK) {
        (void)present_te_composer_reject(backend->composer, job->compose_frame);
        if (present_te_composer_cancel(backend->composer, job->compose_frame) != ESP_OK) {
            present_lock_exit(&backend->lock, false);
            return ESP_OK;
        }
        retire_frame(backend, index);
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    if (!backend->config.te_gate.admit) {
        /* Pinning can race a newer ISR edge. Revalidate and consume exactly
         * the edge used for the final decision, never the stale snapshot. */
        edge = backend->latest_edge;
        uint64_t now = present_now_us();
        bool fresh = backend->has_edge && (!job->attempted || edge > job->attempted_edge);
        bool allowed;
        if (backend->config.te_timing.period_us) {
            allowed = fresh && esp_display_present_te_window(&backend->config.te_timing,
                                                             backend->config.te_budget_us, edge, now, backend->measured).admission == ESP_DISPLAY_PRESENT_TE_ALLOW;
        } else {
            uint64_t window = backend->config.te_window_us - backend->config.te_budget_us;
            allowed = fresh && edge >= job->ready_us && now >= edge && now - edge < window;
        }
        if (!allowed) {
            (void)present_te_composer_reject(backend->composer, job->compose_frame);
            present_lock_exit(&backend->lock, false);
            return ESP_OK;
        }
    }
    job->attempted = true;
    job->attempted_edge = edge;
    job->started_us = present_now_us();
    job->cookie = cookie;
    present_transaction_prepare(&job->transaction);
    job->phase = TE_SUBMITTING;
    present_lock_exit(&backend->lock, false);
    esp_display_present_surface_t surface = {(void *)source.pixels, source.capacity_bytes, source.layout};
    /* One window covering the configured physical canvas. Panel gap is outside
     * this component and cannot turn a smaller canvas into a partial-screen mode. */
    esp_display_present_rect_t area = {0, 0, source.layout.width, source.layout.height};
    if (backend->config.te_gate.start) {
        backend->config.te_gate.start(backend->config.te_gate.user);
    }
    esp_display_present_tx_result_t tx = backend->config.panel.submit(backend->config.panel.context, &surface, &area, cookie);
    present_lock_enter(&backend->lock, false);
    present_transaction_resolution_t resolved = present_transaction_resolve(&job->transaction, tx);
    if (resolved.rejected && backend->config.te_gate.done) {
        backend->config.te_gate.done(backend->config.te_gate.user, tx.result == ESP_OK ? ESP_FAIL : tx.result);
    }
    if (resolved.rejected) {
        job->phase = TE_READY;
        job->cookie = 0;
        (void)present_te_composer_reject(backend->composer, job->compose_frame);
        present_lock_exit(&backend->lock, false);
        return tx.result == ESP_OK ? ESP_FAIL : tx.result;
    }
    record_fault(backend, index, resolved.result);
    job->phase = TE_INFLIGHT;
    if (job->transaction.done) {
        (void)present_te_composer_complete(backend->composer, job->compose_frame,
                                           backend->core.frames[index].common.result);
        retire_frame(backend, index);
    }
    present_lock_exit(&backend->lock, false);
    return ESP_OK;
}

static esp_err_t present_te_service(present_backend_t *backend)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t pending = progress_copies(backend);
    esp_err_t first = ESP_OK;
    for (size_t i = 0; i < backend->config.max_frames; i++) {
        esp_err_t error = service_frame(backend, i, pending);
        if (first == ESP_OK) {
            first = error;
        }
    }
    return first;
}

static void free_backend(present_backend_t *backend)
{
    if (backend->tiles) {
        for (size_t i = 0; i < backend->config.buffer_count; i++) {
            present_free(backend->tiles[i].pixels);
        }
    }
    if (backend->phase_timer) {
        (void)present_te_timer_destroy(backend->phase_timer, UINT32_MAX);
        backend->phase_timer = NULL;
    }
    if (backend->composer) {
        (void)present_te_composer_stop(backend->composer, UINT32_MAX);
        (void)present_te_composer_destroy(backend->composer);
    }
    present_free(backend->tiles);
    present_free(backend->jobs);
    present_frame_core_deinit(&backend->core);
    present_lock_destroy(&backend->lock);
    present_free(backend);
}

static esp_err_t te_create_common(const esp_display_present_backend_config_t *config, present_backend_t **out, bool autonomous)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!config || !config->panel.attach || !config->panel.submit || !config->panel.quiesce) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->copy_execute && !config->async_copy) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->compose_count > 2 || !config->width || !config->height || !config->tile_rows || config->tile_rows > config->height ||
            !config->buffer_count || config->buffer_count > TE_MAX_BUFFERS ||
            !config->max_frames || config->max_frames > TE_MAX_FRAMES) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((config->draw_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE && config->draw_order != ESP_DISPLAY_PRESENT_ORDER_SWAPPED) ||
            (config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB888 && config->draw_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->render_contract != ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION || (config->rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || config->rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270) || config->scan_buf || config->ext_bufs || config->ext_bytes || config->in_place) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->te_gate.admit && config->te_timing.period_us) {
        return ESP_ERR_INVALID_ARG; /* One admission policy per instance. */
    }
    if (!config->te_gate.admit && (!config->te_budget_us || (!config->te_timing.period_us &&
                                                             config->te_budget_us >= config->te_window_us) ||
                                   (config->te_timing.period_us && esp_display_present_te_window(&config->te_timing,
                                                                                                 config->te_budget_us, 0, 0, false).admission == ESP_DISPLAY_PRESENT_TE_UNSUPPORTED))) {
        return ESP_ERR_INVALID_ARG;
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
    bool quarter = config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 || config->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270;
    const present_te_composer_config_t compose_config = {
        .image_count = config->compose_count,
        .image_in_psram = config->image_in_psram,
        .width = quarter ? config->height : config->width,
        .height = quarter ? config->width : config->height,
        .rotation = config->rotation,
        .format = config->format,
        .panel_order = config->panel_order,
        .pixel = {
            .max_jobs = config->buffer_count,
            .asynchronous = config->async_copy,
            .execute = config->copy_execute,
            .execute_user = config->copy_user,
            .on_event = config->async_copy ? copy_done : NULL,
            .user = backend
        },
    };
    esp_err_t error = present_te_composer_create(&compose_config, &backend->composer);
    if (error != ESP_OK) {
        free_backend(backend);
        return error;
    }
    size_t bpp = config->format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    backend->tile_bytes = (size_t)config->width * config->tile_rows * bpp;
    backend->jobs = present_alloc(config->max_frames * sizeof(*backend->jobs));
    backend->tiles = present_alloc(config->buffer_count * sizeof(*backend->tiles));
    esp_err_t core_error = present_frame_core_init(&backend->core, config->max_frames);
    if (!backend->jobs || !backend->tiles || core_error != ESP_OK) {
        free_backend(backend);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < config->buffer_count; i++) {
        backend->tiles[i].pixels = present_alloc_pixels_in(backend->tile_bytes, config->tile_in_psram);
        if (!backend->tiles[i].pixels) {
            free_backend(backend);
            return ESP_ERR_NO_MEM;
        }
    }
    if (config->te_timing.period_us && config->on_progress) {
        error = present_te_timer_create(phase_wakeup, backend, &backend->phase_timer);
        if (error != ESP_OK && !(error == ESP_ERR_NOT_SUPPORTED && !autonomous)) {
            free_backend(backend);
            return error;
        }
    }
    error = config->panel.attach(config->panel.context, te_dispatch, backend);
    if (error != ESP_OK) {
        free_backend(backend);
        return error;
    }
    *out = backend;
    return ESP_OK;
}

static esp_err_t te_create(const esp_display_present_backend_config_t *config, present_backend_t **out)
{
    return te_create_common(config, out, true);
}

static esp_err_t te_get_caps(present_backend_t *backend, esp_display_present_backend_caps_t *out)
{
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bpp = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    *out = (esp_display_present_backend_caps_t) {
        .contract = ESP_DISPLAY_PRESENT_ENGINE_CONTRACT_PARTITION,
        .draw_layout = {backend->config.width, backend->config.height,
                        backend->config.width * bpp, backend->config.format, backend->config.draw_order
                       },
        .drawbuf_capacity_bytes = backend->tile_bytes,
        .buffer_alignment = PRESENT_BUFFER_ALIGNMENT,
        .max_tile_rows = backend->config.tile_rows,
        .max_damage_areas = TE_MAX_DAMAGE,
        .retains_content = true,
    };
    return ESP_OK;
}

static esp_err_t validate_damage(present_backend_t *backend, const esp_display_present_rect_t *areas, size_t count)
{
    return present_validate_damage(backend->config.width, backend->config.height, areas, count);
}

static esp_err_t te_begin(present_backend_t *backend, const esp_display_present_frame_request_t *request, esp_display_present_frame_t *out)
{
    if (!backend) {
        if (out) {
            out->value = 0;
        }
        return ESP_ERR_INVALID_ARG;
    }
    if (request) {
        esp_err_t error = validate_damage(backend, request->damage, request->damage_count);
        if (error != ESP_OK) {
            if (out) {
                out->value = 0;
            }
            return error;
        }
    }
    (void)present_te_service(backend);
    esp_err_t error = present_frame_core_admit(&backend->core, &backend->lock,
                                               backend->config.width, backend->config.height, request, out);
    if (error != ESP_OK) {
        return error;
    }
    present_lock_enter(&backend->lock, false);
    bool busy = backend->core.active != SIZE_MAX;
    if (!backend->config.compose_count)
        for (size_t k = 0; k < backend->config.max_frames; k++) {
            busy |= backend->jobs[k].phase != TE_IDLE;
        }
    if (busy) {
        present_lock_exit(&backend->lock, false);
        return ESP_ERR_NO_MEM;
    }
    for (size_t i = 0; i < backend->config.max_frames; i++) {
        if (backend->core.frames[i].common.token) {
            continue;
        }
        /* begin allocates a token and clears a full coverage bitmap; outside lock. */
        present_lock_exit(&backend->lock, false);
        error = present_te_composer_begin(backend->composer, &backend->jobs[i].compose_frame);
        if (error != ESP_OK) {
            return error == ESP_ERR_INVALID_STATE ? ESP_ERR_NO_MEM : error;
        }
        present_lock_enter(&backend->lock, false);
        if (backend->core.fault.result != ESP_OK) {
            (void)present_te_composer_cancel(backend->composer, backend->jobs[i].compose_frame);
            present_lock_exit(&backend->lock, false);
            return ESP_ERR_INVALID_STATE;
        }
        backend->core.frames[i] = (te_record_t) {
            .common = {.token = backend->jobs[i].compose_frame.value}
        };
        backend->core.active = i;
        backend->jobs[i].phase = TE_BUILDING;
        out->value = backend->jobs[i].compose_frame.value;
        present_lock_exit(&backend->lock, false);
        return ESP_OK;
    }
    present_lock_exit(&backend->lock, false);
    return ESP_ERR_NO_MEM;
}

static bool building(present_backend_t *backend, esp_display_present_frame_t frame)
{
    return backend->core.life == PRESENT_CORE_RUNNING && backend->core.active != SIZE_MAX && backend->jobs[backend->core.active].phase == TE_BUILDING &&
           frame.value && backend->core.frames[backend->core.active].common.token == frame.value;
}

static esp_err_t te_acquire(present_backend_t *backend, esp_display_present_frame_t frame, const esp_display_present_rect_t *area,
                            uint32_t timeout, esp_display_present_lease_t *out)
{
    (void)timeout; /* Presenter performs bounded event waits and retries. */
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (esp_display_present_lease_t) {
        0
    };
    esp_display_present_rect_t actual = area ? *area : (esp_display_present_rect_t) {
        0, 0, backend->config.width, backend->config.tile_rows
    };
    if (!valid_area(backend, &actual)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (actual.height > backend->config.tile_rows) {
        actual.height = backend->config.tile_rows;
    }
    (void)progress_copies(backend);
    present_lock_enter(&backend->lock, false);
    bool allowed = building(backend, frame) && backend->core.fault.result == ESP_OK;
    present_lock_exit(&backend->lock, false);
    if (!allowed) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        size_t idx = (backend->next_tile + i) % backend->config.buffer_count;
        te_tile_t *tile = &backend->tiles[idx];
        if (tile->lease.id.value || tile->copy.value) {
            continue;
        }
        uint64_t token = present_new_token();
        if (!token) {
            return ESP_ERR_NO_MEM;
        }
        backend->next_tile = (idx + 1) % backend->config.buffer_count;
        size_t bpp = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
        tile->frame = backend->core.active;
        tile->lease = (esp_display_present_lease_t) {
            .id = {token},
            .logical_area = actual,
            .surface = {tile->pixels, backend->tile_bytes,
                {
                    actual.width, actual.height, actual.width * bpp,
                    backend->config.format, backend->config.draw_order
                }
            }
        };
        *out = tile->lease;
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

static te_tile_t *find_lease(present_backend_t *backend, esp_display_present_lease_id_t lease)
{
    for (size_t i = 0; lease.value && i < backend->config.buffer_count; i++) {
        if (backend->tiles[i].lease.id.value == lease.value) {
            return &backend->tiles[i];
        }
    }
    return NULL;
}

static esp_err_t te_submit_region_ex(present_backend_t *backend, esp_display_present_frame_t frame,
                                     esp_display_present_lease_id_t lease, const esp_display_present_region_submit_t *submit)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    present_lock_enter(&backend->lock, false);
    bool allowed = building(backend, frame) && backend->core.fault.result == ESP_OK;
    present_lock_exit(&backend->lock, false);
    te_tile_t *tile = find_lease(backend, lease);
    if (!allowed || !tile) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_display_present_rect_t area = tile->lease.logical_area;
    if (submit && (submit->logical_area.x || submit->logical_area.y ||
                   submit->logical_area.width || submit->logical_area.height)) {
        area = submit->logical_area;
    }
    if (!valid_area(backend, &area)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bpp = backend->config.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    size_t stride = submit && submit->stride_bytes ? submit->stride_bytes : area.width * bpp;
    esp_display_present_source_t source = {tile->pixels, backend->tile_bytes,
        {area.width, area.height, stride, backend->config.format, backend->config.draw_order}
    };
    esp_display_present_rect_t physical;
    esp_err_t mapped = esp_display_present_rotate_rect(backend->config.width, backend->config.height, backend->config.rotation, &area, &physical);
    if (mapped != ESP_OK) {
        return mapped;
    }
    esp_display_present_rect_t src_area = {0, 0, area.width, area.height};
    esp_err_t error;
    if (backend->config.async_copy) {
        error = present_te_composer_queue_write(backend->composer, backend->jobs[backend->core.active].compose_frame,
                                                &source, &src_area, physical.x, physical.y, &tile->copy);
    } else {
        error = present_te_composer_write(backend->composer, backend->jobs[backend->core.active].compose_frame,
                                          &source, &src_area, physical.x, physical.y);
    }
    if (error == ESP_OK) {
        tile->lease.id.value = 0;
    }
    return error;
}

static esp_err_t te_submit_region(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease)
{
    return te_submit_region_ex(backend, frame, lease, NULL);
}

static esp_err_t te_cancel_region(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_lease_id_t lease)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    te_tile_t *tile = find_lease(backend, lease);
    if (!building(backend, frame) || !tile) {
        return ESP_ERR_INVALID_STATE;
    }
    tile->lease.id.value = 0;
    return ESP_OK;
}

static esp_err_t te_get_draw_buffers(present_backend_t *backend, void **buffers, size_t capacity,
                                     size_t *count, size_t *bytes)
{
    if (!backend || !count || (capacity && !buffers)) {
        return ESP_ERR_INVALID_ARG;
    }
    *count = backend->config.buffer_count;
    if (bytes) {
        *bytes = backend->tile_bytes;
    }
    if (capacity < *count) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < *count; i++) {
        buffers[i] = backend->tiles[i].pixels;
    }
    return ESP_OK;
}

static esp_err_t te_end_ex(present_backend_t *backend, esp_display_present_frame_t frame,
                           const esp_display_present_frame_coverage_t *coverage, esp_display_present_receipt_t *out)
{
    if (out) {
        out->value = 0;
    }
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    if (coverage) {
        esp_err_t error = validate_damage(backend, coverage->areas, coverage->area_count);
        if (error != ESP_OK) {
            return error;
        }
    }
    if (!building(backend, frame)) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        if (backend->tiles[i].lease.id.value) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    size_t pending = progress_copies(backend);
    /* Successful copies, never damage hints, establish coverage. */
    esp_err_t error = pending ? ESP_OK :
                      present_te_composer_seal(backend->composer, backend->jobs[backend->core.active].compose_frame);
    if (error != ESP_OK && error != ESP_ERR_TIMEOUT) {
        return error;
    }
    present_lock_enter(&backend->lock, false);
    backend->jobs[backend->core.active].phase = (pending || error == ESP_ERR_TIMEOUT) ? TE_COPY_DRAIN : TE_READY;
    if (backend->jobs[backend->core.active].phase == TE_READY) {
        backend->jobs[backend->core.active].ready_us = present_now_us();
    }
    if (out) {
        backend->core.frames[backend->core.active].common.external = true;
        out->value = frame.value;
    }
    backend->core.frames[backend->core.active].sealed = true;
    backend->core.frames[backend->core.active].pending = 1;
    backend->core.active = SIZE_MAX;
    present_lock_exit(&backend->lock, false);
    /* end consumes frame regardless of later panel acceptance; service reports
     * definite rejection separately, never returns an error implying end rollback. */
    return ESP_OK;
}

static esp_err_t te_end(present_backend_t *backend, esp_display_present_frame_t frame, esp_display_present_receipt_t *out)
{
    return te_end_ex(backend, frame, NULL, out);
}

static esp_err_t te_cancel_frame(present_backend_t *backend, esp_display_present_frame_t frame, uint32_t timeout)
{
    if (!backend || !frame.value) {
        return ESP_ERR_INVALID_ARG;
    }
    if (present_cancel_matches(&backend->core.canceled, frame.value)) {
        return ESP_OK;
    }
    size_t index = SIZE_MAX;
    for (size_t i = 0; i < backend->config.max_frames; i++)
        if (backend->core.frames[i].common.token == frame.value) {
            index = i;
        }
    if (index == SIZE_MAX || (backend->jobs[index].phase != TE_BUILDING && backend->jobs[index].phase != TE_CANCELING)) {
        return ESP_ERR_INVALID_STATE;
    }
    present_lock_enter(&backend->lock, false);
    backend->jobs[index].phase = TE_CANCELING;
    present_lock_exit(&backend->lock, false);
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        backend->tiles[i].lease.id.value = 0;
    }
    uint64_t start = present_now_ms();
    do {
        (void)present_te_service(backend);
        if (present_cancel_matches(&backend->core.canceled, frame.value)) {
            return ESP_OK;
        }
        if (present_expired(start, timeout)) {
            return ESP_ERR_TIMEOUT;
        }
        present_pause();
    } while (true);
}

static esp_err_t te_query(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_frame_status_t *out)
{
    if (!backend || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_te_service(backend);
    return present_frame_core_query(&backend->core, &backend->lock, receipt, false, out);
}

static esp_err_t te_get_fault(present_backend_t *backend, esp_display_present_backend_fault_t *out)
{
    return backend ? present_frame_core_get_fault(&backend->core, &backend->lock, out) : ESP_ERR_INVALID_ARG;
}

static esp_err_t te_wait(present_backend_t *backend, esp_display_present_receipt_t receipt, esp_display_present_completion_point_t point,
                         uint32_t timeout, esp_display_present_frame_status_t *out)
{
    return present_frame_core_wait(backend, te_query, receipt, point, false, timeout, out);
}

static esp_err_t te_release_receipt(present_backend_t *backend, esp_display_present_receipt_t receipt)
{
    if (!backend) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)present_te_service(backend);
    return present_frame_core_release_receipt(&backend->core, &backend->lock, receipt);
}

static esp_err_t te_stop(present_backend_t *backend, uint32_t timeout)
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
    present_lock_exit(&backend->lock, false);
    uint64_t start = present_now_ms();
    esp_err_t error = present_te_timer_destroy(backend->phase_timer, timeout);
    if (error != ESP_OK) {
        return error;
    }
    backend->phase_timer = NULL;
    error = present_te_composer_stop(backend->composer, present_remaining_ms(start, timeout));
    if (error != ESP_OK) {
        return error;
    }
    size_t pending = progress_copies(backend);
    /* Cancellation is normal retirement, including when stop drained the copy.
     * Never seal or send other jobs while closing. */
    for (size_t i = 0; i < backend->config.max_frames; ++i) {
        if (backend->jobs[i].phase == TE_CANCELING) {
            (void)service_frame(backend, i, pending);
            if (backend->jobs[i].phase == TE_CANCELING) {
                return ESP_ERR_TIMEOUT;
            }
        }
    }
    uint32_t left = present_remaining_ms(start, timeout);
    /* Copy callbacks have exited. Adapter joins panel callbacks separately. */
    error = backend->config.panel.quiesce(backend->config.panel.context, left);
    if (error != ESP_OK) {
        return error;
    }
    present_lock_enter(&backend->lock, false);
    for (size_t i = 0; i < backend->config.max_frames; i++) {
        te_job_t *job = &backend->jobs[i];
        if (job->phase == TE_IDLE) {
            continue;
        }
        if (job->phase == TE_INFLIGHT) {
            if (!job->transaction.done) {
                record_fault(backend, i, ESP_ERR_INVALID_STATE);
            }
            (void)present_te_composer_complete(backend->composer, job->compose_frame, backend->core.frames[i].common.result);
        } else {
            record_fault(backend, i, ESP_ERR_INVALID_STATE);
            (void)present_te_composer_cancel(backend->composer, job->compose_frame);
        }
        retire_frame(backend, i);
    }
    for (size_t i = 0; i < backend->config.buffer_count; i++) {
        backend->tiles[i].lease.id.value = 0;
    }
    backend->core.life = PRESENT_CORE_QUIESCENT;
    present_lock_exit(&backend->lock, false);
    return ESP_OK;
}

static esp_err_t te_destroy(present_backend_t *backend)
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

static esp_err_t te_set_rotation(present_backend_t *backend, esp_display_present_engine_rotation_t rotation)
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
    esp_err_t error = present_te_composer_reorient(backend->composer, rotation,
                                                   rotation & 1 ? height : width, rotation & 1 ? width : height);
    if (error != ESP_OK) {
        return error;
    }
    backend->config.width = width;
    backend->config.height = height;
    backend->config.tile_rows = rows > height ? height : rows;
    backend->config.rotation = rotation;
    return ESP_OK;
}

const present_backend_ops_t present_te_backend = {
    .name = "te",
    .service = present_te_service,
    .create = te_create,
    .create_scheduled = te_create_common,
    .get_caps = te_get_caps,
    .begin = te_begin,
    .acquire = te_acquire,
    .get_draw_buffers = te_get_draw_buffers,
    .submit_region = te_submit_region,
    .submit_region_ex = te_submit_region_ex,
    .cancel_region = te_cancel_region,
    .end = te_end,
    .end_ex = te_end_ex,
    .cancel_frame = te_cancel_frame,
    .query = te_query,
    .get_fault = te_get_fault,
    .wait = te_wait,
    .release_receipt = te_release_receipt,
    .on_event_isr = te_on_event_isr,
    .on_event = te_on_event,
    .stop = te_stop,
    .destroy = te_destroy,
    .set_rotation = te_set_rotation,
};

size_t present_te_memory(const esp_display_present_backend_config_t *config)
{
    return sizeof(present_backend_t) + config->buffer_count * sizeof(te_tile_t) +
           config->max_frames * (sizeof(present_core_frame_t) + sizeof(te_job_t)) + present_composer_memory() +
           present_pixel_memory(config->buffer_count, config->async_copy) +
           (config->te_timing.period_us && !config->te_gate.admit ? present_te_timer_memory() : 0);
}
