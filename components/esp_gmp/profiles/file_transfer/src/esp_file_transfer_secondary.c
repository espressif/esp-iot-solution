/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esp_file_transfer_secondary.c
 * @brief Per-link session helpers (OTA-style sessions[] keyed by link).
 */

#include "esp_file_transfer_internal.h"

#include <stdlib.h>
#include <string.h>

ft_sess_t *ft_sess_find(ft_instance_t *instance, esp_gmp_link_t link)
{
    if (!instance || !link) {
        return NULL;
    }
    for (int i = 0; i < FT_MAX_SESSIONS; i++) {
        if (instance->sessions[i].used && instance->sessions[i].link == link) {
            return &instance->sessions[i];
        }
    }
    return NULL;
}

ft_sess_t *ft_sess_from_ctx(ft_instance_t *instance, const ft_context_t *ctx)
{
    if (!instance || !ctx) {
        return NULL;
    }
    for (int i = 0; i < FT_MAX_SESSIONS; i++) {
        if (instance->sessions[i].used && &instance->sessions[i].ctx == ctx) {
            return &instance->sessions[i];
        }
    }
    return NULL;
}

bool ft_sess_owns_ctx(const ft_instance_t *instance, const ft_context_t *ctx)
{
    return ft_sess_from_ctx((ft_instance_t *)instance, ctx) != NULL;
}

void ft_sess_apply_caps_to_instance(ft_instance_t *instance, const ft_sess_t *sess)
{
    if (!instance || !sess) {
        return;
    }
    instance->block_size = sess->block_size;
    instance->effective_payload = sess->effective_payload;
}

esp_err_t ft_sess_refresh_capabilities(ft_instance_t *instance, ft_sess_t *sess)
{
    if (!instance || !sess || !sess->used || !sess->link) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t effective = esp_gmp_max_payload_effective(sess->link);
    if (effective < ESP_FT_MIN_EFFECTIVE_PAYLOAD) {
        return ESP_FT_ERR_CAPABILITY_MISMATCH;
    }

    size_t block_limit = effective - ESP_FT_DATA_REQ_HDR_LEN;
    if (block_limit > ESP_FT_DEFAULT_MAX_BLOCK_SIZE) {
        block_limit = ESP_FT_DEFAULT_MAX_BLOCK_SIZE;
    }

    size_t block_size = instance->configured_block_size
                        ? instance->configured_block_size
                        : block_limit;

    if (block_size == 0 || block_size > UINT32_MAX || block_size > block_limit) {
        return ESP_FT_ERR_CAPABILITY_MISMATCH;
    }

    sess->effective_payload = effective;
    sess->block_size = block_size;
    return ESP_OK;
}

ft_sess_t *ft_sess_get_or_alloc(ft_instance_t *instance, esp_gmp_link_t link)
{
    if (!instance || !link) {
        return NULL;
    }

    /* Caller must hold sessions_mutex */

    ft_sess_t *existing = ft_sess_find(instance, link);
    if (existing) {
        /* finish_transfer memset clears ctx.link; restore on reuse. */
        existing->ctx.link = link;
        return existing;
    }

    for (int i = 0; i < FT_MAX_SESSIONS; i++) {
        if (!instance->sessions[i].used) {
            ft_sess_t *sess = &instance->sessions[i];
            memset(sess, 0, sizeof(*sess));
            sess->used = true;
            sess->link = link;
            sess->ctx.link = link;
            sess->ctx.role = ESP_FILE_TRANSFER_ROLE_NONE;
            sess->ctx.state = TRANSFER_STATE_PREPARING;

            if (ft_sess_refresh_capabilities(instance, sess) != ESP_OK) {
                sess->used = false;
                return NULL;
            }
            return sess;
        }
    }
    return NULL;
}

static void sess_release_ctx_resources(ft_context_t *ctx)
{
    if (!ctx) {
        return;
    }
    if (ctx->file) {
        fclose(ctx->file);
        ctx->file = NULL;
    }
    if (ctx->hash) {
        ft_hash_destroy(ctx->hash);
        ctx->hash = NULL;
    }
    free(ctx->block_buffer);
    ctx->block_buffer = NULL;
    free(ctx->source_path);
    ctx->source_path = NULL;
    if (ctx->temp_path) {
        if (!ctx->target_committed) {
            ft_fs_remove(ctx->temp_path);
        }
        free(ctx->temp_path);
        ctx->temp_path = NULL;
    }
    free(ctx->target_path);
    ctx->target_path = NULL;
}

void ft_sess_free(ft_instance_t *instance, ft_sess_t *sess)
{
    if (!instance || !sess || !sess->used) {
        return;
    }

    /* Caller must hold sessions_mutex */

    if (instance->active_ctx == &sess->ctx) {
        ft_timer_disarm(instance);
        instance->active_ctx = NULL;
    }

    sess_release_ctx_resources(&sess->ctx);
    memset(sess, 0, sizeof(*sess));
}

void ft_sess_cleanup_by_link(ft_instance_t *instance, esp_gmp_link_t link)
{
    if (!instance || !link || !instance->sessions_mutex) {
        return;
    }

    typedef struct {
        bool emit;
        esp_file_transfer_event_t event;
        char file_name[ESP_FT_FILE_NAME_MAX_LEN + 1];
        char saved_name[ESP_FT_FILE_NAME_MAX_LEN + 1];
    } pending_fail_t;

    pending_fail_t pending[FT_MAX_SESSIONS];
    int pending_count = 0;
    esp_file_transfer_event_cb_t event_cb = NULL;
    void *event_ctx = NULL;
    void (*profile_event_cb)(const esp_gmp_profile_event_t *event, void *ctx) = NULL;
    void *profile_event_ctx = NULL;

    xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);

    event_cb = instance->event_cb;
    event_ctx = instance->event_ctx;
    profile_event_cb = instance->profile_event_cb;
    profile_event_ctx = instance->profile_event_ctx;

    for (int i = 0; i < FT_MAX_SESSIONS; i++) {
        ft_sess_t *sess = &instance->sessions[i];
        if (!sess->used || sess->link != link) {
            continue;
        }

        if (sess->ctx.state != TRANSFER_STATE_ERROR &&
                sess->ctx.role != ESP_FILE_TRANSFER_ROLE_NONE &&
                !sess->ctx.terminal_emitted) {
            sess->ctx.reason_code = ESP_FT_REASON_LINK_ERROR;
            sess->ctx.detail = ESP_FAIL;
            sess->ctx.terminal_emitted = true;

            if ((event_cb || profile_event_cb) && pending_count < FT_MAX_SESSIONS) {
                pending_fail_t *p = &pending[pending_count++];
                memset(p, 0, sizeof(*p));
                p->emit = true;
                strncpy(p->file_name, sess->ctx.file_name, sizeof(p->file_name) - 1);
                strncpy(p->saved_name, sess->ctx.saved_name, sizeof(p->saved_name) - 1);
                p->event = (esp_file_transfer_event_t) {
                    .event_id = ESP_FT_EVENT_FAILED,
                    .role = sess->ctx.role,
                    .file_name = p->file_name,
                    .bytes_transferred = sess->ctx.bytes_transferred,
                    .total_bytes = sess->ctx.file_size,
                    .percent = sess->ctx.last_reported_percent,
                    .reason_code = sess->ctx.reason_code,
                    .saved_name = p->saved_name[0] ? p->saved_name : NULL,
                    .detail = sess->ctx.detail,
                };
            }
        }

        ft_sess_free(instance, sess);
    }

    xSemaphoreGive(instance->sessions_mutex);

    for (int i = 0; i < pending_count; i++) {
        if (!pending[i].emit) {
            continue;
        }
        if (event_cb) {
            event_cb(&pending[i].event, event_ctx);
        }
        if (profile_event_cb) {
            esp_gmp_profile_event_t pev = {
                .event_id = (uint8_t)pending[i].event.event_id,
                .role = (uint8_t)pending[i].event.role,
                .transferred = pending[i].event.bytes_transferred,
                .total = pending[i].event.total_bytes,
                .percent = pending[i].event.percent,
                .reason = pending[i].event.reason_code,
                .detail = pending[i].event.detail,
                .profile_extra = &pending[i].event,
            };
            profile_event_cb(&pev, profile_event_ctx);
        }
    }

    /* Restore focused caps from the default link session when active_ctx cleared. */
    if (!instance->active_ctx) {
        ft_sess_t *def = ft_sess_find(instance, instance->gmp_link);
        if (def) {
            ft_sess_apply_caps_to_instance(instance, def);
        }
        ft_snapshot_update(instance);
    }
}
