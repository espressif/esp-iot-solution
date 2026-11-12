/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */
#include "pixel/present_pixel_engine.h"
#include "resources/present_memory.h"
#include "backend/present_te_composer.h"
#include "platform/present_os.h"
#include "platform/present_limits.h"
#include "backend/present_image_repair.h"
#include <string.h>

typedef enum {
    IDLE,
    BUILDING,
    REPAIR,
    READY,
    INFLIGHT,
} compose_state_t;
typedef struct {
    compose_state_t state;
    present_te_frame_t frame;
    size_t candidate;
} compose_job_t;

struct present_te_composer {
    esp_display_present_pixel_engine_t *pixel;
    esp_display_present_surface_t buffers[2];
    present_image_builder_t image;
    int baseline;
    size_t image_count;
    bool pipelined;
    compose_job_t jobs[2];
    struct {
        esp_display_present_copy_ticket_t ticket;
        esp_display_present_rect_t area;
    } writes[PRESENT_MAX_COPY_JOBS];
    size_t pending;
    bool asynchronous;
    esp_display_present_engine_rotation_t rotation;
    size_t repair_pos;
    esp_display_present_copy_ticket_t repair;
};

static compose_job_t *find_job(present_te_composer_t *composer, present_te_frame_t f)
{
    if (!composer || !f.value) {
        return NULL;
    }
    for (size_t i = 0; i < composer->image_count; i++)
        if (composer->jobs[i].state != IDLE && composer->jobs[i].frame.value == f.value) {
            return &composer->jobs[i];
        }
    return NULL;
}

static esp_err_t copy(present_te_composer_t *composer, compose_job_t *job, const esp_display_present_source_t *source,
                      const esp_display_present_rect_t *area, uint32_t x, uint32_t y)
{
    esp_display_present_copy_request_t request = {
        .src = *source,
        .dst = composer->buffers[job->candidate],
        .src_area = *area,
        .dst_x = x,
        .dst_y = y,
        .rotation = ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0,
    };
    esp_display_present_copy_ticket_t ticket;
    esp_err_t err = esp_display_present_pixel_submit(composer->pixel, &request, &ticket);
    if (err != ESP_OK) {
        return err;
    }
    if (composer->asynchronous) {
        composer->repair = ticket;
        return ESP_ERR_TIMEOUT;
    }
    /* Synchronous compatibility path. */
    esp_display_present_copy_status_t status;
    err = esp_display_present_pixel_wait(composer->pixel, ticket, UINT32_MAX, &status);
    esp_err_t release = esp_display_present_pixel_release(composer->pixel, ticket);
    return err != ESP_OK ? err : status.result != ESP_OK ? status.result : release;
}

esp_err_t present_te_composer_create(const present_te_composer_config_t *cfg, present_te_composer_t **out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!cfg || cfg->image_count > 2 || !cfg->width || !cfg->height || cfg->rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || cfg->rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270 ||
            (cfg->format != ESP_DISPLAY_PRESENT_FORMAT_RGB565 && cfg->format != ESP_DISPLAY_PRESENT_FORMAT_RGB888) ||
            (cfg->panel_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE && cfg->panel_order != ESP_DISPLAY_PRESENT_ORDER_SWAPPED) ||
            (cfg->format == ESP_DISPLAY_PRESENT_FORMAT_RGB888 && cfg->panel_order != ESP_DISPLAY_PRESENT_ORDER_NATIVE)) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t bpp = cfg->format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3;
    if (cfg->width > SIZE_MAX / cfg->height) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t pixels = (size_t)cfg->width * cfg->height;
    if (pixels > (SIZE_MAX - (PRESENT_BUFFER_ALIGNMENT - 1)) / bpp) {
        return ESP_ERR_INVALID_ARG;
    }
    present_te_composer_t *composer = present_alloc(sizeof(*composer));
    if (!composer) {
        return ESP_ERR_NO_MEM;
    }
    composer->baseline = -1;
    composer->image_count = cfg->image_count ? cfg->image_count : 2;
    composer->pipelined = cfg->image_count != 0;
    composer->rotation = cfg->rotation;
    composer->asynchronous = cfg->pixel.asynchronous;
    esp_err_t image_error = present_image_init(&composer->image, cfg->width, cfg->height);
    for (size_t i = 0; i < composer->image_count; i++) {
        composer->buffers[i] = (esp_display_present_surface_t) {
            .pixels = present_alloc_pixels_in(pixels * bpp, cfg->image_in_psram),
            .capacity_bytes = pixels * bpp,
            .layout = {cfg->width, cfg->height, cfg->width * bpp, cfg->format, cfg->panel_order},
        };
    }
    esp_err_t err = ESP_ERR_NO_MEM;
    if (image_error != ESP_OK || !composer->buffers[0].pixels || (composer->image_count == 2 && !composer->buffers[1].pixels)) {
        goto fail;
    }
    esp_display_present_pixel_config_t pc = cfg->pixel;
    if (!pc.max_jobs) {
        pc.max_jobs = 1;
    }
    err = esp_display_present_pixel_create(&pc, &composer->pixel);
    if (err != ESP_OK) {
        goto fail;
    }
    *out = composer;
    return ESP_OK;
fail:
    present_image_deinit(&composer->image);
    present_free(composer->buffers[0].pixels);
    present_free(composer->buffers[1].pixels);
    present_free(composer);
    return err;
}

esp_err_t present_te_composer_begin(present_te_composer_t *composer, present_te_frame_t *out)
{
    if (!composer || !out) {
        return ESP_ERR_INVALID_ARG;
    }
    *out = (present_te_frame_t) {
        0
    };
    compose_job_t *job = NULL;
    for (size_t i = 0; i < composer->image_count; i++) {
        if (!composer->pipelined && composer->jobs[i].state != IDLE) {
            return ESP_ERR_INVALID_STATE;
        }
        if (composer->jobs[i].state == BUILDING || composer->jobs[i].state == REPAIR) {
            return ESP_ERR_INVALID_STATE;
        }
        if (composer->jobs[i].state == IDLE && (composer->image_count == 1 || i != (size_t)composer->baseline)) {
            job = &composer->jobs[i];
        }
    }
    if (!job) {
        return ESP_ERR_NO_MEM;
    }
    uint64_t token = present_new_token();
    if (!token) {
        return ESP_ERR_NO_MEM;
    }
    size_t candidate = (size_t)(job - composer->jobs);
    *job = (compose_job_t) {
        .state = BUILDING,
        .frame = {token},
        .candidate = candidate
    };
    esp_err_t error = present_image_begin(&composer->image, candidate, composer->baseline < 0 ? SIZE_MAX : (size_t)composer->baseline);
    if (error != ESP_OK) {
        job->state = IDLE;
        return error;
    }
    composer->repair_pos = 0;
    *out = job->frame;
    return ESP_OK;
}

esp_err_t present_te_composer_queue_write(present_te_composer_t *composer, present_te_frame_t f,
                                          const esp_display_present_source_t *source, const esp_display_present_rect_t *area, uint32_t x, uint32_t y,
                                          esp_display_present_copy_ticket_t *ticket)
{
    compose_job_t *job = find_job(composer, f);
    if (!ticket) {
        return ESP_ERR_INVALID_ARG;
    }
    ticket->value = 0;
    if (!job || job->state != BUILDING) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!source || !area) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t slot = 0;
    while (slot < PRESENT_MAX_COPY_JOBS && composer->writes[slot].ticket.value) {
        slot++;
    }
    if (slot == PRESENT_MAX_COPY_JOBS) {
        return ESP_ERR_NO_MEM;
    }
    esp_display_present_copy_request_t r = {
        .src = *source,
        .dst = composer->buffers[job->candidate],
        .src_area = *area,
        .dst_x = x,
        .dst_y = y,
        .rotation = composer->rotation,
    };
    esp_err_t error = esp_display_present_pixel_submit(composer->pixel, &r, ticket);
    if (error != ESP_OK) {
        return error;
    }
    composer->writes[slot].ticket = *ticket;
    bool quarter = composer->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_90 || composer->rotation == ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270;
    composer->writes[slot].area = (esp_display_present_rect_t) {
        x, y, quarter ? area->height : area->width, quarter ? area->width : area->height
    };
    composer->pending++;
    return ESP_OK;
}

esp_err_t present_te_composer_finish_write(present_te_composer_t *composer, esp_display_present_copy_ticket_t ticket,
                                           esp_err_t *result)
{
    if (!composer || !ticket.value || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t slot = 0;
    while (slot < PRESENT_MAX_COPY_JOBS && composer->writes[slot].ticket.value != ticket.value) {
        slot++;
    }
    if (slot == PRESENT_MAX_COPY_JOBS) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_display_present_copy_status_t status;
    esp_err_t error = esp_display_present_pixel_wait(composer->pixel, ticket, 0, &status);
    if (error != ESP_OK) {
        return error;
    }
    *result = status.result;
    if (status.result == ESP_OK) {
        esp_display_present_rect_t a = composer->writes[slot].area;
        *result = present_image_cover(&composer->image, a);
    }
    error = esp_display_present_pixel_release(composer->pixel, ticket);
    if (error != ESP_OK) {
        return error;
    }
    composer->writes[slot].ticket.value = 0;
    composer->pending--;
    return ESP_OK;
}

esp_err_t present_te_composer_write(present_te_composer_t *composer, present_te_frame_t f,
                                    const esp_display_present_source_t *source, const esp_display_present_rect_t *area, uint32_t x, uint32_t y)
{
    esp_display_present_copy_ticket_t ticket;
    esp_err_t error = present_te_composer_queue_write(composer, f, source, area, x, y, &ticket);
    if (error != ESP_OK) {
        return error;
    }
    esp_display_present_copy_status_t status;
    error = esp_display_present_pixel_wait(composer->pixel, ticket, UINT32_MAX, &status);
    if (error != ESP_OK) {
        return error;
    }
    esp_err_t result;
    error = present_te_composer_finish_write(composer, ticket, &result);
    return error == ESP_OK ? result : error;
}

esp_err_t present_te_composer_stop(present_te_composer_t *composer, uint32_t timeout)
{
    if (!composer) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t error = esp_display_present_pixel_stop(composer->pixel, timeout);
    if (error == ESP_OK && composer->repair.value) {
        error = esp_display_present_pixel_release(composer->pixel, composer->repair);
        if (error == ESP_OK) {
            composer->repair.value = 0;
        }
    }
    return error;
}

esp_err_t present_te_composer_seal(present_te_composer_t *composer, present_te_frame_t f)
{
    compose_job_t *job = find_job(composer, f);
    if (!job || (job->state != BUILDING && job->state != REPAIR)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (composer->pending) {
        return ESP_ERR_TIMEOUT;
    }
    if (composer->repair.value) {
        esp_display_present_copy_status_t status;
        esp_err_t error = esp_display_present_pixel_wait(composer->pixel, composer->repair, 0, &status);
        if (error != ESP_OK) {
            return error;
        }
        (void)esp_display_present_pixel_release(composer->pixel, composer->repair);
        composer->repair.value = 0;
        if (status.result != ESP_OK) {
            return status.result;
        }
    }
    esp_display_present_surface_t *surface = &composer->buffers[job->candidate];
    (void)surface;
    if (composer->baseline < 0) {
        if (!present_image_full(&composer->image)) {
            return ESP_ERR_INVALID_STATE;
        }
    } else {
        job->state = REPAIR;
        esp_display_present_surface_t *base = &composer->buffers[composer->baseline];
        esp_display_present_source_t source = {base->pixels, base->capacity_bytes, base->layout};
        present_image_prepare_repair(&composer->image, false);
        esp_display_present_rect_t area;
        while (present_image_next_repair(&composer->image, &area)) {
            esp_err_t error = copy(composer, job, &source, &area, area.x, area.y);
            if (error == ESP_OK || error == ESP_ERR_TIMEOUT) {
                present_image_mark_repaired(&composer->image, &area);
            }
            if (error != ESP_OK) {
                return error;
            }
        }
    }
    if (composer->image.coverage_error != ESP_OK) {
        return composer->image.coverage_error;
    }
    if (composer->pipelined) {
        esp_err_t commit = present_image_commit(&composer->image);
        if (commit != ESP_OK) {
            return commit;
        }
        composer->baseline = job->candidate;
    }
    if (!composer->pipelined) {
        present_allocation_written(&composer->image.allocations[job->candidate]);
    }
    job->state = READY;
    return ESP_OK;
}

esp_err_t present_te_composer_pin(present_te_composer_t *composer, present_te_frame_t f, esp_display_present_source_t *out)
{
    compose_job_t *job = find_job(composer, f);
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!job || job->state != READY) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_display_present_surface_t *surface = &composer->buffers[job->candidate];
    *out = (esp_display_present_source_t) {
        surface->pixels, surface->capacity_bytes, surface->layout
    };
    present_allocation_hold(&composer->image.allocations[job->candidate], PRESENT_REF_TRANSFER);
    job->state = INFLIGHT;
    return ESP_OK;
}

esp_err_t present_te_composer_reject(present_te_composer_t *composer, present_te_frame_t f)
{
    compose_job_t *job = find_job(composer, f);
    if (!job || job->state != INFLIGHT) {
        return ESP_ERR_INVALID_STATE;
    }
    present_allocation_drop(&composer->image.allocations[job->candidate], PRESENT_REF_TRANSFER);
    job->state = READY;
    return ESP_OK;
}

esp_err_t present_te_composer_complete(present_te_composer_t *composer, present_te_frame_t f, esp_err_t result)
{
    compose_job_t *job = find_job(composer, f);
    if (!job || job->state != INFLIGHT) {
        return ESP_ERR_INVALID_STATE;
    }
    present_allocation_drop(&composer->image.allocations[job->candidate], PRESENT_REF_TRANSFER);
    if (!composer->pipelined) {
        if (result == ESP_OK) {
            composer->image.allocations[job->candidate].writer = true; /* metadata publication after a drained legacy transfer */
            esp_err_t error = present_image_commit(&composer->image);
            if (error != ESP_OK) {
                return error;
            }
            composer->baseline = job->candidate;
        } else {
            present_image_cancel(&composer->image);
        }
    }
    job->state = IDLE;
    job->frame.value = 0;
    return ESP_OK;
}

esp_err_t present_te_composer_cancel(present_te_composer_t *composer, present_te_frame_t f)
{
    compose_job_t *job = find_job(composer, f);
    if (!job || job->state == INFLIGHT) {
        return ESP_ERR_INVALID_STATE;
    }
    if (composer->pending) {
        return ESP_ERR_TIMEOUT;
    }
    if (composer->repair.value) {
        esp_display_present_copy_status_t status;
        esp_err_t error = esp_display_present_pixel_cancel(composer->pixel, composer->repair, 0, &status);
        if (error != ESP_OK) {
            return error;
        }
        (void)esp_display_present_pixel_release(composer->pixel, composer->repair);
        composer->repair.value = 0;
    }
    if (job->state == BUILDING || job->state == REPAIR || !composer->pipelined) {
        present_image_cancel(&composer->image);
    }
    composer->image.versions[job->candidate] = 0;
    if (composer->baseline == (int)job->candidate) {
        composer->baseline = -1;
    }
    job->state = IDLE;
    job->frame.value = 0;
    return ESP_OK;
}

esp_err_t present_te_composer_destroy(present_te_composer_t *composer)
{
    if (!composer) {
        return ESP_ERR_INVALID_ARG;
    }
    if (composer->jobs[0].state != IDLE || composer->jobs[1].state != IDLE || composer->pending) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_display_present_pixel_stop(composer->pixel, 0);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_display_present_pixel_destroy(composer->pixel);
    if (err != ESP_OK) {
        return err;
    }
    present_image_deinit(&composer->image);
    present_free(composer->buffers[0].pixels);
    present_free(composer->buffers[1].pixels);
    present_free(composer);
    return ESP_OK;
}

esp_err_t present_te_composer_reorient(present_te_composer_t *composer, esp_display_present_engine_rotation_t rotation, uint32_t width, uint32_t height)
{
    if (!composer || rotation < ESP_DISPLAY_PRESENT_ENGINE_ROTATE_0 || rotation > ESP_DISPLAY_PRESENT_ENGINE_ROTATE_270) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!width || !height || (uint64_t)width * height != (uint64_t)composer->image.width * composer->image.height) {
        return ESP_ERR_INVALID_ARG;
    }
    if (composer->jobs[0].state != IDLE || composer->jobs[1].state != IDLE) {
        return ESP_ERR_INVALID_STATE;
    }
    composer->rotation = rotation;
    for (size_t i = 0; i < composer->image_count; ++i) {
        composer->buffers[i].layout.width = width;
        composer->buffers[i].layout.height = height;
        composer->buffers[i].layout.stride_bytes = (size_t)width * (composer->buffers[i].layout.format == ESP_DISPLAY_PRESENT_FORMAT_RGB565 ? 2 : 3);
    }
    present_image_reset(&composer->image, width, height);
    composer->baseline = -1;
    return ESP_OK;
}

esp_err_t present_te_composer_set_rotation(present_te_composer_t *composer, esp_display_present_engine_rotation_t rotation)
{
    return composer ? present_te_composer_reorient(composer, rotation, composer->image.width, composer->image.height) : ESP_ERR_INVALID_ARG;
}

size_t present_composer_memory(void)
{
    return sizeof(present_te_composer_t);
}
