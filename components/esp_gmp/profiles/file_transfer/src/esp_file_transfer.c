/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_file_transfer_internal.h"
#include "esp_gmp.h"
#include "sdkconfig.h"

#include <stdlib.h>
#include <string.h>

#if CONFIG_ESP_GMP_PROFILE_OS
#include "esp_gmp_os.h"
#endif

static ft_instance_t s_ft;
static esp_gmp_link_event_sub_t s_link_sub;

static void ft_handle_mtu_changed(esp_gmp_link_t link)
{
    ft_instance_t *instance = ft_instance_get();
    if (!link || !ft_async_enter(instance)) {
        return;
    }
    ft_pending_mtu_push(instance, link);
    ft_worker_wake(instance);
    ft_async_exit(instance);
}

static void ft_link_event_handler(esp_gmp_link_t link, esp_gmp_link_event_type_t event,
                                  esp_err_t error, void *ctx)
{
    (void)ctx;
    switch (event) {
    case ESP_GMP_LINK_EVENT_DOWN:
        esp_file_transfer_on_link_down(link);
        break;
    case ESP_GMP_LINK_EVENT_TRANSPORT_ERROR:
        esp_file_transfer_on_transport_error(link, error);
        break;
    case ESP_GMP_LINK_EVENT_MTU_CHANGED:
        ft_handle_mtu_changed(link);
        break;
    default:
        break;
    }
}

ft_instance_t *ft_instance_get(void)
{
    return &s_ft;
}

void ft_pending_link_down_push(ft_instance_t *instance, esp_gmp_link_t link)
{
    if (!instance || !link) {
        return;
    }
    portENTER_CRITICAL(&instance->pending_link_lock);
    bool found = false;
    for (uint8_t i = 0; i < instance->pending_link_down_count; i++) {
        if (instance->pending_link_downs[i] == link) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (instance->pending_link_down_count > 0) {
            /* Distinct second link (or overflow) → flush all sessions. */
            atomic_store(&instance->link_down_all_pending, true);
        }
        if (instance->pending_link_down_count < FT_MAX_SESSIONS) {
            instance->pending_link_downs[instance->pending_link_down_count++] = link;
        } else {
            atomic_store(&instance->link_down_all_pending, true);
        }
    }
    portEXIT_CRITICAL(&instance->pending_link_lock);
    atomic_store(&instance->link_down_pending, true);
}

void ft_pending_mtu_push(ft_instance_t *instance, esp_gmp_link_t link)
{
    if (!instance || !link) {
        return;
    }
    portENTER_CRITICAL(&instance->pending_link_lock);
    bool found = false;
    for (uint8_t i = 0; i < instance->pending_mtu_count; i++) {
        if (instance->pending_mtu_links[i] == link) {
            found = true;
            break;
        }
    }
    if (!found && instance->pending_mtu_count < FT_MAX_SESSIONS) {
        instance->pending_mtu_links[instance->pending_mtu_count++] = link;
    }
    portEXIT_CRITICAL(&instance->pending_link_lock);
    atomic_store(&instance->mtu_refresh_pending, true);
}

void ft_event_queue_lock(ft_instance_t *instance)
{
    if (instance && instance->event_queue_mutex) {
        xSemaphoreTake(instance->event_queue_mutex, portMAX_DELAY);
    }
}

void ft_event_queue_unlock(ft_instance_t *instance)
{
    if (instance && instance->event_queue_mutex) {
        xSemaphoreGive(instance->event_queue_mutex);
    }
}

BaseType_t ft_event_queue_send(ft_instance_t *instance, const ft_worker_event_t *event,
                               TickType_t ticks_to_wait)
{
    if (!instance || !event || !instance->event_queue) {
        return pdFALSE;
    }
    ft_event_queue_lock(instance);
    BaseType_t ok = xQueueSend(instance->event_queue, event, ticks_to_wait);
    ft_event_queue_unlock(instance);
    return ok;
}

void ft_discard_worker_event(ft_instance_t *instance, ft_worker_event_t *event, esp_err_t result)
{
    if (!event) {
        return;
    }
    if (event->type == FT_WORK_GMP_PACKET) {
        free(event->data.packet.payload);
        event->data.packet.payload = NULL;
    } else if (event->type == FT_WORK_SEND_CMD) {
        free(event->data.send.src_path);
        free(event->data.send.remote_name);
        free(event->data.send.sha256);
        event->data.send.src_path = NULL;
        event->data.send.remote_name = NULL;
        event->data.send.sha256 = NULL;
        if (event->data.send.reply.result) {
            *event->data.send.reply.result = result;
        }
        if (event->data.send.reply.done) {
            xSemaphoreGive(event->data.send.reply.done);
        }
    } else if (event->type == FT_WORK_ABORT_CMD) {
        if (event->data.abort.reply.result) {
            *event->data.abort.reply.result = result;
        }
        if (event->data.abort.reply.done) {
            xSemaphoreGive(event->data.abort.reply.done);
        }
    } else if (event->type == FT_WORK_DEINIT) {
        if (event->data.deinit.reply.result) {
            *event->data.deinit.reply.result = result;
        }
        if (event->data.deinit.reply.done) {
            xSemaphoreGive(event->data.deinit.reply.done);
        }
    } else if (event->type == FT_WORK_TIMEOUT && instance) {
        portENTER_CRITICAL(&instance->timeout_lock);
        instance->timeout_pending_tuple = event->data.timeout;
        atomic_store(&instance->timeout_pending, true);
        portEXIT_CRITICAL(&instance->timeout_lock);
    }
}

esp_err_t ft_refresh_capabilities(ft_instance_t *instance)
{
    if (!instance) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_gmp_link_t link = instance->gmp_link;
    if (instance->active_ctx && instance->active_ctx->link) {
        link = instance->active_ctx->link;
    }
    if (!link) {
        return ESP_ERR_INVALID_STATE;
    }

    ft_sess_t *sess = NULL;
    if (instance->sessions_mutex) {
        xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);
        sess = ft_sess_find(instance, link);
        if (sess) {
            esp_err_t err = ft_sess_refresh_capabilities(instance, sess);
            if (err == ESP_OK) {
                ft_sess_apply_caps_to_instance(instance, sess);
            }
            xSemaphoreGive(instance->sessions_mutex);
            return err;
        }
        xSemaphoreGive(instance->sessions_mutex);
    }

    size_t effective = esp_gmp_max_payload_effective(link);
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
    if (block_size == 0 || block_size > UINT32_MAX ||
            block_size > block_limit) {
        return ESP_FT_ERR_CAPABILITY_MISMATCH;
    }
    instance->effective_payload = effective;
    instance->block_size = block_size;
    return ESP_OK;
}

bool ft_async_enter(ft_instance_t *instance)
{
    if (!instance || !atomic_load(&instance->accepting_events)) {
        return false;
    }
    atomic_fetch_add(&instance->hook_ref_count, 1);
    if (!atomic_load(&instance->accepting_events)) {
        atomic_fetch_sub(&instance->hook_ref_count, 1);
        return false;
    }
    return true;
}

void ft_async_exit(ft_instance_t *instance)
{
    atomic_fetch_sub(&instance->hook_ref_count, 1);
}

void ft_worker_wake(ft_instance_t *instance)
{
    if (instance && instance->worker_task) {
        xTaskNotifyGive(instance->worker_task);
    }
}

uint16_t ft_next_sequence(ft_instance_t *instance)
{
    if (!instance) {
        return 0;
    }
    esp_gmp_link_t link = NULL;
    if (instance->active_ctx && instance->active_ctx->link) {
        link = instance->active_ctx->link;
    } else {
        link = instance->gmp_link;
    }
    if (link) {
        uint16_t seq = esp_gmp_seq_next(link);
        if (seq != 0) {
            return seq;
        }
    }

    /* Fallback when no link is bound yet */
    uint16_t value = instance->next_sequence++;
    if (value == 0) {
        value = instance->next_sequence++;
    }
    if (instance->next_sequence == 0) {
        instance->next_sequence = 1;
    }
    return value;
}

uint32_t ft_next_transfer_id(ft_instance_t *instance)
{
    uint32_t value = instance->next_transfer_id++;
    if (value == 0) {
        value = instance->next_transfer_id++;
    }
    if (instance->next_transfer_id == 0) {
        instance->next_transfer_id = 1;
    }
    return value;
}

static void timeout_callback(void *arg)
{
    ft_instance_t *instance = arg;
    atomic_fetch_add(&instance->timer_cb_inflight, 1);
    if (!ft_async_enter(instance)) {
        atomic_fetch_sub(&instance->timer_cb_inflight, 1);
        return;
    }
    ft_worker_event_t event = {
        .type = FT_WORK_TIMEOUT,
    };
    portENTER_CRITICAL(&instance->timeout_lock);
    event.data.timeout = instance->timeout_tuple;
    portEXIT_CRITICAL(&instance->timeout_lock);
    if (event.data.timeout.kind == FT_TIMER_NONE) {
        ft_async_exit(instance);
        atomic_fetch_sub(&instance->timer_cb_inflight, 1);
        return;
    }
    /* Never block the shared esp_timer task on event_queue_mutex. */
    BaseType_t queued = pdFALSE;
    if (instance->event_queue_mutex &&
            xSemaphoreTake(instance->event_queue_mutex, 0) == pdTRUE) {
        if (instance->event_queue) {
            queued = xQueueSend(instance->event_queue, &event, 0);
        }
        xSemaphoreGive(instance->event_queue_mutex);
    }
    if (queued != pdTRUE) {
        portENTER_CRITICAL(&instance->timeout_lock);
        instance->timeout_pending_tuple = event.data.timeout;
        portEXIT_CRITICAL(&instance->timeout_lock);
        atomic_store(&instance->timeout_pending, true);
    }
    ft_worker_wake(instance);
    ft_async_exit(instance);
    atomic_fetch_sub(&instance->timer_cb_inflight, 1);
}

esp_err_t ft_timer_arm(ft_instance_t *instance, ft_timer_kind_t kind, uint64_t timeout_us)
{
    if (!instance || !instance->timer || kind == FT_TIMER_NONE || !instance->active_ctx) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Invalidate any in-flight timeout_callback before rewriting the tuple.
     * esp_timer_stop() does not wait for a running callback on ESP_TIMER_TASK. */
    portENTER_CRITICAL(&instance->timeout_lock);
    if (++instance->timer_generation == 0) {
        ++instance->timer_generation;
    }
    instance->timer_kind = FT_TIMER_NONE;
    instance->timer_transfer_id = 0;
    instance->timeout_tuple = (ft_timeout_tuple_t) {
        0
    };
    instance->timeout_pending_tuple = (ft_timeout_tuple_t) {
        0
    };
    atomic_store(&instance->timeout_pending, false);
    portEXIT_CRITICAL(&instance->timeout_lock);

    (void)esp_timer_stop(instance->timer);
    /* IDF v5.5 has no esp_timer_stop_blocking — wait for in-flight callbacks
     * before publishing a new timeout_tuple (prevents SMP false timeouts). */
    for (int i = 0; i < 50 && atomic_load(&instance->timer_cb_inflight) > 0; i++) {
        taskYIELD();
    }

    portENTER_CRITICAL(&instance->timeout_lock);
    if (++instance->timer_generation == 0) {
        ++instance->timer_generation;
    }
    instance->timer_kind = kind;
    instance->timer_transfer_id = instance->active_ctx->transfer_id;
    instance->timeout_tuple.kind = kind;
    instance->timeout_tuple.generation = instance->timer_generation;
    instance->timeout_tuple.transfer_id = instance->timer_transfer_id;
    instance->timeout_pending_tuple = (ft_timeout_tuple_t) {
        0
    };
    atomic_store(&instance->timeout_pending, false);
    portEXIT_CRITICAL(&instance->timeout_lock);
    esp_err_t err = esp_timer_start_once(instance->timer, timeout_us);
    if (err != ESP_OK) {
        ft_timer_disarm(instance);
    }
    return err;
}

void ft_timer_disarm(ft_instance_t *instance)
{
    if (!instance || !instance->timer) {
        return;
    }
    esp_timer_stop(instance->timer);
    for (int i = 0; i < 50 && atomic_load(&instance->timer_cb_inflight) > 0; i++) {
        taskYIELD();
    }
    portENTER_CRITICAL(&instance->timeout_lock);
    if (++instance->timer_generation == 0) {
        ++instance->timer_generation;
    }
    instance->timer_kind = FT_TIMER_NONE;
    instance->timer_transfer_id = 0;
    instance->timeout_tuple = (ft_timeout_tuple_t) {
        0
    };
    instance->timeout_pending_tuple = (ft_timeout_tuple_t) {
        0
    };
    atomic_store(&instance->timeout_pending, false);
    portEXIT_CRITICAL(&instance->timeout_lock);
}

void ft_snapshot_update(ft_instance_t *instance)
{
    if (!instance || !instance->snapshot_mutex) {
        return;
    }
    xSemaphoreTake(instance->snapshot_mutex, portMAX_DELAY);
    ft_context_t *ctx = instance->active_ctx;
    memset(&instance->snapshot, 0, sizeof(instance->snapshot));
    if (!ctx) {
        instance->snapshot.role = ESP_FILE_TRANSFER_ROLE_NONE;
        instance->snapshot.state = ESP_FILE_TRANSFER_STATE_IDLE;
    } else {
        instance->snapshot.in_progress = true;
        instance->snapshot.role = ctx->role;
        instance->snapshot.state = (esp_file_transfer_state_t)(ctx->state + 1);
        memcpy(instance->snapshot.file_name, ctx->file_name, sizeof(instance->snapshot.file_name));
        instance->snapshot.bytes_transferred = ctx->bytes_transferred;
        instance->snapshot.total_bytes = ctx->file_size;
        instance->snapshot.percent = ctx->file_size == 0
                                     ? 100
                                     : (uint8_t)((ctx->bytes_transferred * 100) / ctx->file_size);
        instance->snapshot.reason_code = ctx->reason_code;
    }
    xSemaphoreGive(instance->snapshot_mutex);
}

void ft_emit_event(ft_instance_t *instance, esp_file_transfer_event_id_t event_id)
{
    if (!instance || !instance->active_ctx) {
        return;
    }
    if (!instance->event_cb && !instance->profile_event_cb) {
        return;
    }
    ft_context_t *ctx = instance->active_ctx;
    uint8_t percent = ctx->file_size == 0
                      ? ((event_id == ESP_FT_EVENT_PROGRESS ||
                          event_id == ESP_FT_EVENT_COMPLETED ||
                          event_id == ESP_FT_EVENT_VERIFYING) ? 100 : 0)
                      : (uint8_t)((ctx->bytes_transferred * 100) / ctx->file_size);
    esp_file_transfer_event_t event = {
        .event_id = event_id,
        .role = ctx->role,
        .file_name = ctx->file_name,
        .bytes_transferred = ctx->bytes_transferred,
        .total_bytes = ctx->file_size,
        .percent = percent,
        .reason_code = ctx->reason_code,
        .saved_name = ctx->saved_name[0] ? ctx->saved_name : NULL,
        .detail = ctx->detail,
    };
    ft_snapshot_update(instance);
    if (instance->event_cb) {
        instance->event_cb(&event, instance->event_ctx);
    }
    if (instance->profile_event_cb) {
        esp_gmp_profile_event_t pev = {
            .event_id = (uint8_t)event_id,
            .role = (uint8_t)ctx->role,
            .transferred = ctx->bytes_transferred,
            .total = ctx->file_size,
            .percent = percent,
            .reason = ctx->reason_code,
            .detail = ctx->detail,
            .profile_extra = &event,
        };
        instance->profile_event_cb(&pev, instance->profile_event_ctx);
    }
}

void ft_emit_rejected_event(ft_instance_t *instance, const char *file_name,
                            uint64_t file_size, uint16_t reason, esp_err_t detail)
{
    if (!instance || (!instance->event_cb && !instance->profile_event_cb)) {
        return;
    }
    esp_file_transfer_event_t event = {
        .event_id = ESP_FT_EVENT_META_RECEIVED,
        .role = ESP_FILE_TRANSFER_ROLE_RECEIVER,
        .file_name = file_name,
        .total_bytes = file_size,
        .reason_code = ESP_FT_REASON_OK,
        .detail = ESP_OK,
    };
    if (instance->event_cb) {
        instance->event_cb(&event, instance->event_ctx);
    }
    if (instance->profile_event_cb) {
        esp_gmp_profile_event_t pev = {
            .event_id = (uint8_t)event.event_id,
            .role = (uint8_t)event.role,
            .transferred = 0,
            .total = file_size,
            .percent = 0,
            .reason = event.reason_code,
            .detail = event.detail,
            .profile_extra = &event,
        };
        instance->profile_event_cb(&pev, instance->profile_event_ctx);
    }
    event.event_id = ESP_FT_EVENT_FAILED;
    event.reason_code = reason;
    event.detail = detail;
    if (instance->event_cb) {
        instance->event_cb(&event, instance->event_ctx);
    }
    if (instance->profile_event_cb) {
        esp_gmp_profile_event_t pev = {
            .event_id = (uint8_t)event.event_id,
            .role = (uint8_t)event.role,
            .transferred = 0,
            .total = file_size,
            .percent = 0,
            .reason = event.reason_code,
            .detail = event.detail,
            .profile_extra = &event,
        };
        instance->profile_event_cb(&pev, instance->profile_event_ctx);
    }
}

void ft_finish_transfer(ft_instance_t *instance, esp_file_transfer_event_id_t terminal_event,
                        uint16_t reason, esp_err_t detail)
{
    if (!instance || !instance->active_ctx || instance->active_ctx->terminal_emitted) {
        return;
    }
    ft_context_t *ctx = instance->active_ctx;
    ctx->terminal_emitted = true;
    ctx->reason_code = reason;
    ctx->detail = detail;
    if (terminal_event == ESP_FT_EVENT_FAILED) {
        ctx->state = TRANSFER_STATE_ERROR;
    }
    ft_timer_disarm(instance);
    if (ctx->file) {
        fclose(ctx->file);
        ctx->file = NULL;
    }
    if (ctx->role == ESP_FILE_TRANSFER_ROLE_RECEIVER && !ctx->target_committed) {
        ft_fs_remove(ctx->temp_path);
    }
    ft_hash_destroy(ctx->hash);
    ctx->hash = NULL;
    free(ctx->block_buffer);
    ctx->block_buffer = NULL;
    ft_emit_event(instance, terminal_event);
    free(ctx->source_path);
    free(ctx->temp_path);
    free(ctx->target_path);
    /* ctx is embedded in sessions[]; clear transfer state, keep the slot. */
    memset(ctx, 0, sizeof(*ctx));
    instance->active_ctx = NULL;

    ft_sess_t *def = ft_sess_find(instance, instance->gmp_link);
    if (def) {
        ft_sess_apply_caps_to_instance(instance, def);
    }
    ft_snapshot_update(instance);
}

void ft_abort_active(ft_instance_t *instance, uint16_t reason, esp_err_t detail,
                     bool notify_peer, bool cancelled)
{
    if (!instance || !instance->active_ctx) {
        return;
    }
    ft_timer_disarm(instance);
    instance->active_ctx->pending_valid = false;
    if (notify_peer) {
        ft_gmp_send_abort(instance, instance->active_ctx, reason);
    }
    ft_finish_transfer(instance, cancelled ? ESP_FT_EVENT_CANCELLED : ESP_FT_EVENT_FAILED,
                       reason, detail);
}

static bool timeout_current(ft_instance_t *instance, const ft_timeout_tuple_t *timeout)
{
    if (!instance->active_ctx) {
        return false;
    }
    bool current;
    portENTER_CRITICAL(&instance->timeout_lock);
    current = timeout->kind != FT_TIMER_NONE &&
              timeout->kind == instance->timer_kind &&
              timeout->generation == instance->timer_generation &&
              timeout->transfer_id == instance->timer_transfer_id &&
              timeout->transfer_id == instance->active_ctx->transfer_id;
    portEXIT_CRITICAL(&instance->timeout_lock);
    return current;
}

static void handle_timeout(ft_instance_t *instance, const ft_timeout_tuple_t *timeout)
{
    if (!timeout_current(instance, timeout)) {
        return;
    }
    if (instance->active_ctx->role == ESP_FILE_TRANSFER_ROLE_SENDER) {
        ft_sender_handle_timeout(instance, timeout->kind);
    } else {
        ft_receiver_handle_timeout(instance, timeout->kind);
    }
}

bool ft_process_pending_termination(ft_instance_t *instance)
{
    bool had_down = false;
    bool had_all = false;
    bool had_transport_error = false;
    esp_gmp_link_t downs[FT_MAX_SESSIONS];
    uint8_t down_count = 0;
    esp_err_t transport_error = ESP_OK;

    /* Snapshot link-down + transport-error flags together so a concurrent
     * transport_error path cannot be observed as a bare link-down. */
    portENTER_CRITICAL(&instance->pending_link_lock);
    had_down = atomic_exchange(&instance->link_down_pending, false);
    had_all = atomic_exchange(&instance->link_down_all_pending, false);
    had_transport_error = atomic_exchange(&instance->transport_error_pending, false);
    if (had_transport_error) {
        transport_error = atomic_load(&instance->transport_error);
    }
    if (had_down || had_all) {
        down_count = instance->pending_link_down_count;
        if (down_count > FT_MAX_SESSIONS) {
            down_count = FT_MAX_SESSIONS;
        }
        memcpy(downs, instance->pending_link_downs, down_count * sizeof(downs[0]));
        instance->pending_link_down_count = 0;
        memset(instance->pending_link_downs, 0, sizeof(instance->pending_link_downs));
    }
    portEXIT_CRITICAL(&instance->pending_link_lock);

    if (had_down || had_all) {
        esp_err_t detail = had_transport_error ? transport_error : ESP_ERR_INVALID_STATE;
        uint16_t reason = had_transport_error ? ESP_FT_REASON_DATA_SEND_FAILED
                          : ESP_FT_REASON_LINK_ERROR;

        if (had_all) {
            if (instance->active_ctx) {
                ft_abort_active(instance, reason, detail, had_transport_error, false);
            }
            esp_gmp_link_t all_links[FT_MAX_SESSIONS];
            uint8_t all_count = 0;
            if (instance->sessions_mutex) {
                xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);
                for (int i = 0; i < FT_MAX_SESSIONS; i++) {
                    if (instance->sessions[i].used && instance->sessions[i].link &&
                            all_count < FT_MAX_SESSIONS) {
                        all_links[all_count++] = instance->sessions[i].link;
                    }
                }
                xSemaphoreGive(instance->sessions_mutex);
            }
            for (uint8_t i = 0; i < all_count; i++) {
                ft_sess_cleanup_by_link(instance, all_links[i]);
            }
        } else {
            for (uint8_t i = 0; i < down_count; i++) {
                esp_gmp_link_t down_link = downs[i];
                if (instance->active_ctx && down_link &&
                        instance->active_ctx->link == down_link) {
                    ft_abort_active(instance, reason, detail, had_transport_error, false);
                }
                if (down_link) {
                    ft_sess_cleanup_by_link(instance, down_link);
                }
            }
        }
        return true;
    }
    if (had_transport_error) {
        /* Fallback if a transport error was posted without link_down_pending. */
        if (instance->active_ctx) {
            ft_abort_active(instance, ESP_FT_REASON_DATA_SEND_FAILED, transport_error, true, false);
        }
        return true;
    }
    if (atomic_exchange(&instance->user_abort_pending, false)) {
        if (instance->active_ctx) {
            ft_abort_active(instance, ESP_FT_REASON_ABORTED, ESP_OK, true, true);
            atomic_store(&instance->user_abort_completed, true);
            return true;
        }
    }
    return false;
}

static void handle_mtu_refresh(ft_instance_t *instance)
{
    esp_gmp_link_t links[FT_MAX_SESSIONS];
    uint8_t count = 0;
    portENTER_CRITICAL(&instance->pending_link_lock);
    count = instance->pending_mtu_count;
    if (count > FT_MAX_SESSIONS) {
        count = FT_MAX_SESSIONS;
    }
    memcpy(links, instance->pending_mtu_links, count * sizeof(links[0]));
    instance->pending_mtu_count = 0;
    memset(instance->pending_mtu_links, 0, sizeof(instance->pending_mtu_links));
    portEXIT_CRITICAL(&instance->pending_link_lock);

    if (!count || !instance->sessions_mutex) {
        return;
    }

    for (uint8_t i = 0; i < count; i++) {
        esp_gmp_link_t link = links[i];
        if (!link) {
            continue;
        }
        xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);
        ft_sess_t *sess = ft_sess_find(instance, link);
        if (sess) {
            esp_err_t err = ft_sess_refresh_capabilities(instance, sess);
            bool idle = !instance->active_ctx ||
                        instance->active_ctx->role == ESP_FILE_TRANSFER_ROLE_NONE ||
                        instance->active_ctx->link != link;
            if (err == ESP_OK && idle) {
                ft_sess_apply_caps_to_instance(instance, sess);
            }
        }
        xSemaphoreGive(instance->sessions_mutex);
    }
}

static void process_pending_flags(ft_instance_t *instance)
{
    /* Always drain timeout/MTU after termination — an early return here can
     * starve timeout_pending when it only exists as a queue-full fallback. */
    (void)ft_process_pending_termination(instance);
    if (atomic_exchange(&instance->timeout_pending, false)) {
        ft_timeout_tuple_t timeout;
        portENTER_CRITICAL(&instance->timeout_lock);
        timeout = instance->timeout_pending_tuple;
        instance->timeout_pending_tuple = (ft_timeout_tuple_t) {
            0
        };
        portEXIT_CRITICAL(&instance->timeout_lock);
        handle_timeout(instance, &timeout);
    }
    if (atomic_exchange(&instance->mtu_refresh_pending, false)) {
        handle_mtu_refresh(instance);
    }
}

static void handle_packet(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    if (!packet || !packet->link) {
        return;
    }

    /* Singleton worker: reject a new transfer start while another link is busy. */
    if (packet->op == ESP_GMP_OP_WRITE_REQ &&
            (packet->command_id == ESP_FT_CMD_TRANSFER_META ||
             packet->command_id == ESP_FT_CMD_DATA_BLOCK) &&
            instance->active_ctx &&
            instance->active_ctx->role != ESP_FILE_TRANSFER_ROLE_NONE &&
            instance->active_ctx->link &&
            instance->active_ctx->link != packet->link) {
        ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_BUSY, NULL, 0);
        return;
    }

    xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);
    ft_sess_t *sess = ft_sess_get_or_alloc(instance, packet->link);
    xSemaphoreGive(instance->sessions_mutex);

    if (!sess) {
        if (packet->op == ESP_GMP_OP_WRITE_REQ &&
                (packet->command_id == ESP_FT_CMD_TRANSFER_META ||
                 packet->command_id == ESP_FT_CMD_DATA_BLOCK)) {
            ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_BUSY, NULL, 0);
        }
        return;
    }

    ft_context_t *saved_ctx = instance->active_ctx;
    size_t saved_block_size = instance->block_size;
    size_t saved_effective = instance->effective_payload;

    instance->active_ctx = &sess->ctx;
    ft_sess_apply_caps_to_instance(instance, sess);

    if (packet->command_id == ESP_FT_CMD_ABORT && packet->op == ESP_GMP_OP_WRITE_REQ) {
        ft_handle_peer_abort(instance, packet);
    } else if (packet->command_id == ESP_FT_CMD_TRANSFER_META &&
               packet->op == ESP_GMP_OP_WRITE_REQ) {
        ft_receiver_handle_meta(instance, packet);
    } else if (packet->command_id == ESP_FT_CMD_DATA_BLOCK &&
               packet->op == ESP_GMP_OP_WRITE_REQ) {
        ft_receiver_handle_data(instance, packet);
    } else if (instance->active_ctx &&
               instance->active_ctx->role == ESP_FILE_TRANSFER_ROLE_SENDER) {
        ft_sender_handle_packet(instance, packet);
    } else if (packet->op == ESP_GMP_OP_WRITE_REQ &&
               (packet->command_id == ESP_FT_CMD_TRANSFER_META ||
                packet->command_id == ESP_FT_CMD_DATA_BLOCK)) {
        ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_BAD_STATE, NULL, 0);
    }

    if (sess->used) {
        sess->block_size = instance->block_size;
        sess->effective_payload = instance->effective_payload;
    }

    bool sess_busy = sess->used && sess->ctx.role != ESP_FILE_TRANSFER_ROLE_NONE;

    if (sess_busy) {
        /* Keep this session focused unless another transfer already owns active_ctx. */
        if (!saved_ctx || saved_ctx == &sess->ctx) {
            instance->active_ctx = &sess->ctx;
            ft_sess_apply_caps_to_instance(instance, sess);
        } else {
            instance->active_ctx = saved_ctx;
            instance->block_size = saved_block_size;
            instance->effective_payload = saved_effective;
        }
    } else if (saved_ctx && saved_ctx != &sess->ctx) {
        instance->active_ctx = saved_ctx;
        instance->block_size = saved_block_size;
        instance->effective_payload = saved_effective;
    } else {
        instance->active_ctx = NULL;
        ft_sess_t *def = ft_sess_find(instance, instance->gmp_link);
        if (def) {
            ft_sess_apply_caps_to_instance(instance, def);
        }
    }
}

static void free_event(ft_worker_event_t *event)
{
    if (event->type == FT_WORK_SEND_CMD) {
        free(event->data.send.src_path);
        free(event->data.send.remote_name);
        free(event->data.send.sha256);
        event->data.send.src_path = NULL;
        event->data.send.remote_name = NULL;
        event->data.send.sha256 = NULL;
    } else if (event->type == FT_WORK_GMP_PACKET) {
        free(event->data.packet.payload);
    }
}

static void complete_reply(ft_command_reply_t *reply, esp_err_t result)
{
    if (reply->result) {
        *reply->result = result;
    }
    if (reply->done) {
        xSemaphoreGive(reply->done);
    }
}

static bool receive_next_event(ft_instance_t *instance, ft_worker_event_t *event)
{
    if (xQueueReceive(instance->urgent_queue, event, 0) == pdTRUE) {
        return true;
    }
    return xQueueReceive(instance->event_queue, event, 0) == pdTRUE;
}

static void worker_task(void *arg)
{
    ft_instance_t *instance = arg;
    bool running = true;
    while (running) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        process_pending_flags(instance);
        ft_worker_event_t event;
        while (receive_next_event(instance, &event)) {
            switch (event.type) {
            case FT_WORK_SEND_CMD: {
                esp_err_t err;
                if (event.data.send.read_fn) {
                    esp_file_transfer_send_stream_param_t stream = {
                        .read_fn = event.data.send.read_fn,
                        .read_ctx = event.data.send.read_ctx,
                        .file_size = event.data.send.file_size,
                        .remote_name = event.data.send.remote_name,
                        .sha256 = event.data.send.sha256,
                        .link = event.data.send.link,
                    };
                    err = ft_sender_start_stream(instance, &stream);
                } else {
                    err = ft_sender_start(instance, event.data.send.src_path,
                                          event.data.send.remote_name);
                }
                complete_reply(&event.data.send.reply, err);
                if (err == ESP_OK) {
                    ft_sender_prepare(instance);
                }
                break;
            }
            case FT_WORK_ABORT_CMD:
                if (atomic_exchange(&instance->user_abort_completed, false)) {
                    complete_reply(&event.data.abort.reply, ESP_OK);
                } else if (instance->active_ctx) {
                    ft_abort_active(instance, ESP_FT_REASON_ABORTED, ESP_OK, true, true);
                    complete_reply(&event.data.abort.reply, ESP_OK);
                } else {
                    complete_reply(&event.data.abort.reply, ESP_ERR_INVALID_STATE);
                }
                break;
            case FT_WORK_GMP_PACKET:
                handle_packet(instance, &event.data.packet);
                break;
            case FT_WORK_TIMEOUT:
                handle_timeout(instance, &event.data.timeout);
                break;
            case FT_WORK_DEINIT:
                if (instance->active_ctx) {
                    ft_abort_active(instance, ESP_FT_REASON_ABORTED, ESP_OK, true, true);
                }
                ft_timer_disarm(instance);
                complete_reply(&event.data.deinit.reply, ESP_OK);
                running = false;
                break;
            default:
                break;
            }
            free_event(&event);
            process_pending_flags(instance);
            if (!running) {
                break;
            }
        }
    }
    instance->worker_task = NULL;
    vTaskDelete(NULL);
}

static void reset_instance_dynamic(ft_instance_t *instance)
{
    free(instance->recv_dir);
    instance->recv_dir = NULL;
    instance->gmp_link = NULL;
    instance->event_cb = NULL;
    instance->event_ctx = NULL;
    instance->profile_event_cb = NULL;
    instance->profile_event_ctx = NULL;
    instance->accept_cb = NULL;
    instance->accept_ctx = NULL;
    instance->initialized = false;
}

esp_err_t esp_file_transfer_init(const esp_file_transfer_config_t *config)
{
    ft_instance_t *instance = &s_ft;
    if (!config || !config->recv_dir || !config->gmp_link ||
            (!config->event_cb && !config->profile_event_cb)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (instance->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t effective = esp_gmp_max_payload_effective(config->gmp_link);
    if (effective < ESP_FT_MIN_EFFECTIVE_PAYLOAD) {
        return ESP_FT_ERR_CAPABILITY_MISMATCH;
    }
    size_t block_limit = effective - ESP_FT_DATA_REQ_HDR_LEN;
    if (block_limit > ESP_FT_DEFAULT_MAX_BLOCK_SIZE) {
        block_limit = ESP_FT_DEFAULT_MAX_BLOCK_SIZE;
    }
    size_t block_size = config->block_size ? config->block_size : block_limit;
    if (block_size == 0 || block_size > UINT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (block_size > block_limit) {
        return config->block_size ? ESP_ERR_INVALID_ARG : ESP_FT_ERR_CAPABILITY_MISMATCH;
    }
    esp_err_t err = ft_fs_prepare(config->recv_dir);
    if (err != ESP_OK) {
        return err;
    }

    memset(instance, 0, sizeof(*instance));
    instance->timeout_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    instance->recv_dir = strdup(config->recv_dir);
    if (!instance->recv_dir) {
        return ESP_ERR_NO_MEM;
    }
    instance->gmp_link = config->gmp_link;
    instance->max_file_size = config->max_file_size ? config->max_file_size :
                              ESP_FT_DEFAULT_MAX_FILE_SIZE;
    instance->configured_block_size = config->block_size;
    instance->block_size = block_size;
    instance->effective_payload = effective;
    instance->event_cb = config->event_cb;
    instance->event_ctx = config->event_ctx;
    instance->profile_event_cb = config->profile_event_cb;
    instance->profile_event_ctx = config->profile_event_ctx;
    instance->accept_cb = config->accept_cb;
    instance->accept_ctx = config->accept_ctx;
    instance->next_sequence = 1;
    instance->next_transfer_id = 1;
    atomic_init(&instance->accepting_events, false);
    atomic_init(&instance->user_abort_pending, false);
    atomic_init(&instance->user_abort_completed, false);
    atomic_init(&instance->link_down_pending, false);
    atomic_init(&instance->link_down_all_pending, false);
    atomic_init(&instance->transport_error_pending, false);
    atomic_init(&instance->timeout_pending, false);
    atomic_init(&instance->mtu_refresh_pending, false);
    atomic_init(&instance->timer_cb_inflight, 0);
    atomic_init(&instance->transport_error, ESP_OK);
    atomic_init(&instance->hook_ref_count, 0);
    instance->pending_link_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
    instance->pending_link_down_count = 0;
    instance->pending_mtu_count = 0;
    memset(instance->pending_link_downs, 0, sizeof(instance->pending_link_downs));
    memset(instance->pending_mtu_links, 0, sizeof(instance->pending_mtu_links));

    instance->snapshot_mutex = xSemaphoreCreateMutex();
    instance->abort_mutex = xSemaphoreCreateMutex();
    instance->event_queue_mutex = xSemaphoreCreateMutex();
    instance->event_queue = xQueueCreate(ESP_FT_EVENT_QUEUE_LEN, sizeof(ft_worker_event_t));
    instance->urgent_queue = xQueueCreate(ESP_FT_URGENT_QUEUE_LEN, sizeof(ft_worker_event_t));
    if (!instance->snapshot_mutex || !instance->abort_mutex || !instance->event_queue_mutex ||
            !instance->event_queue || !instance->urgent_queue) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    const esp_timer_create_args_t timer_args = {
        .callback = timeout_callback,
        .arg = instance,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ft_business",
    };
    err = esp_timer_create(&timer_args, &instance->timer);
    if (err != ESP_OK) {
        goto fail;
    }
    if (xTaskCreate(worker_task, "file_transfer", ESP_FT_WORKER_STACK_SIZE, instance,
                    ESP_FT_WORKER_PRIORITY, &instance->worker_task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }

    /* OTA-style per-link sessions; pre-allocate the config default link. */
    memset(instance->sessions, 0, sizeof(instance->sessions));
    instance->sessions_mutex = xSemaphoreCreateMutex();
    if (!instance->sessions_mutex) {
        err = ESP_ERR_NO_MEM;
        goto fail;
    }
    xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);
    ft_sess_t *default_sess = ft_sess_get_or_alloc(instance, config->gmp_link);
    if (default_sess) {
        default_sess->block_size = block_size;
        default_sess->effective_payload = effective;
    }
    xSemaphoreGive(instance->sessions_mutex);
    if (!default_sess) {
        err = ESP_FT_ERR_CAPABILITY_MISMATCH;
        goto fail;
    }

    instance->initialized = true;
    ft_snapshot_update(instance);
    ft_fs_cleanup_parts(instance->recv_dir);
    atomic_store(&instance->accepting_events, true);

    err = esp_gmp_register_handler(ESP_GMP_GRP_FILE_TRANSFER, esp_file_transfer_on_packet);
    if (err != ESP_OK) {
        (void)esp_file_transfer_deinit();
        return err;
    }
    s_link_sub = esp_gmp_link_event_subscribe(ft_link_event_handler, NULL);
    if (!s_link_sub) {
        esp_gmp_unregister_handler(ESP_GMP_GRP_FILE_TRANSFER, esp_file_transfer_on_packet);
        (void)esp_file_transfer_deinit();
        return ESP_ERR_NO_MEM;
    }
#if CONFIG_ESP_GMP_PROFILE_OS
    esp_gmp_os_register_capability(ESP_GMP_OS_CAP_FT_SUPPORTED);
#endif
    return ESP_OK;

fail:
    if (instance->worker_task) {
        vTaskDelete(instance->worker_task);
        instance->worker_task = NULL;
    }
    if (instance->sessions_mutex) {
        vSemaphoreDelete(instance->sessions_mutex);
        instance->sessions_mutex = NULL;
    }
    if (instance->timer) {
        esp_timer_delete(instance->timer);
        instance->timer = NULL;
    }
    if (instance->event_queue) {
        vQueueDelete(instance->event_queue);
        instance->event_queue = NULL;
    }
    if (instance->urgent_queue) {
        vQueueDelete(instance->urgent_queue);
        instance->urgent_queue = NULL;
    }
    if (instance->snapshot_mutex) {
        vSemaphoreDelete(instance->snapshot_mutex);
        instance->snapshot_mutex = NULL;
    }
    if (instance->abort_mutex) {
        vSemaphoreDelete(instance->abort_mutex);
        instance->abort_mutex = NULL;
    }
    if (instance->event_queue_mutex) {
        vSemaphoreDelete(instance->event_queue_mutex);
        instance->event_queue_mutex = NULL;
    }
    reset_instance_dynamic(instance);
    return err;
}

esp_err_t esp_file_transfer_deinit(void)
{
    ft_instance_t *instance = &s_ft;
    if (!instance->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskGetCurrentTaskHandle() == instance->worker_task) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate completion semaphore before irreversible unregister steps. */
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (!done) {
        return ESP_ERR_NO_MEM;
    }

#if CONFIG_ESP_GMP_PROFILE_OS
    esp_gmp_os_unregister_capability(ESP_GMP_OS_CAP_FT_SUPPORTED);
#endif
    if (s_link_sub) {
        esp_gmp_link_event_unsubscribe(s_link_sub);
        s_link_sub = NULL;
    }
    esp_gmp_unregister_handler(ESP_GMP_GRP_FILE_TRANSFER, esp_file_transfer_on_packet);

    atomic_store(&instance->accepting_events, false);
    while (atomic_load(&instance->hook_ref_count) != 0) {
        taskYIELD();
    }
    /* Serialize with concurrent abort (holds abort_mutex across worker reply). */
    if (instance->abort_mutex) {
        xSemaphoreTake(instance->abort_mutex, portMAX_DELAY);
    }
    esp_err_t result = ESP_FAIL;
    ft_worker_event_t event = {
        .type = FT_WORK_DEINIT,
        .data.deinit.reply = {
            .done = done,
            .result = &result,
        },
    };
    (void)ft_event_queue_send(instance, &event, portMAX_DELAY);
    ft_worker_wake(instance);
    xSemaphoreTake(done, portMAX_DELAY);
    vSemaphoreDelete(done);
    while (instance->worker_task) {
        taskYIELD();
    }
    esp_timer_delete(instance->timer);
    instance->timer = NULL;
    ft_worker_event_t pending;
    while (xQueueReceive(instance->event_queue, &pending, 0) == pdTRUE) {
        free_event(&pending);
        if (pending.type == FT_WORK_SEND_CMD) {
            complete_reply(&pending.data.send.reply, ESP_ERR_INVALID_STATE);
        } else if (pending.type == FT_WORK_ABORT_CMD) {
            complete_reply(&pending.data.abort.reply, ESP_ERR_INVALID_STATE);
        }
    }
    while (xQueueReceive(instance->urgent_queue, &pending, 0) == pdTRUE) {
        free_event(&pending);
    }
    vQueueDelete(instance->event_queue);
    vQueueDelete(instance->urgent_queue);
    instance->event_queue = NULL;
    instance->urgent_queue = NULL;

    /* Mark uninitialized before destroying sync objects so concurrent abort
     * sees INVALID_STATE instead of touching freed mutexes. */
    instance->initialized = false;

    if (instance->snapshot_mutex) {
        SemaphoreHandle_t snap = instance->snapshot_mutex;
        instance->snapshot_mutex = NULL;
        xSemaphoreTake(snap, portMAX_DELAY);
        xSemaphoreGive(snap);
        vSemaphoreDelete(snap);
    }
    if (instance->abort_mutex) {
        /* Release the deinit hold so a concurrent abort can exit, then wait
         * until we own the mutex alone before deleting it. */
        xSemaphoreGive(instance->abort_mutex);
        xSemaphoreTake(instance->abort_mutex, portMAX_DELAY);
        SemaphoreHandle_t abort_mu = instance->abort_mutex;
        instance->abort_mutex = NULL;
        xSemaphoreGive(abort_mu);
        vSemaphoreDelete(abort_mu);
    }
    if (instance->event_queue_mutex) {
        SemaphoreHandle_t qmu = instance->event_queue_mutex;
        instance->event_queue_mutex = NULL;
        vSemaphoreDelete(qmu);
    }

    if (instance->sessions_mutex) {
        xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);
        for (int i = 0; i < FT_MAX_SESSIONS; i++) {
            if (instance->sessions[i].used) {
                ft_sess_free(instance, &instance->sessions[i]);
            }
        }
        xSemaphoreGive(instance->sessions_mutex);
        vSemaphoreDelete(instance->sessions_mutex);
        instance->sessions_mutex = NULL;
    }

    reset_instance_dynamic(instance);
    return result;
}

esp_err_t esp_file_transfer_send(const esp_file_transfer_send_param_t *param)
{
    ft_instance_t *instance = &s_ft;
    if (!param || !param->src_path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!instance->initialized || !atomic_load(&instance->accepting_events)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskGetCurrentTaskHandle() == instance->worker_task) {
        return ESP_ERR_INVALID_STATE;
    }
    ft_worker_event_t event = {
        .type = FT_WORK_SEND_CMD,
    };
    event.data.send.src_path = strdup(param->src_path);
    event.data.send.remote_name = param->remote_name ? strdup(param->remote_name) : NULL;
    event.data.send.reply.done = xSemaphoreCreateBinary();
    esp_err_t result = ESP_FAIL;
    event.data.send.reply.result = &result;
    if (!event.data.send.src_path || (param->remote_name && !event.data.send.remote_name) ||
            !event.data.send.reply.done) {
        free(event.data.send.src_path);
        free(event.data.send.remote_name);
        if (event.data.send.reply.done) {
            vSemaphoreDelete(event.data.send.reply.done);
        }
        return ESP_ERR_NO_MEM;
    }
    if (ft_event_queue_send(instance, &event, 0) != pdTRUE) {
        free(event.data.send.src_path);
        free(event.data.send.remote_name);
        vSemaphoreDelete(event.data.send.reply.done);
        return ESP_FT_ERR_QUEUE_FULL;
    }
    ft_worker_wake(instance);
    xSemaphoreTake(event.data.send.reply.done, portMAX_DELAY);
    vSemaphoreDelete(event.data.send.reply.done);
    return result;
}

esp_err_t esp_file_transfer_send_stream(const esp_file_transfer_send_stream_param_t *param)
{
    ft_instance_t *instance = &s_ft;
    if (!param || !param->read_fn || !param->remote_name) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!instance->initialized || !atomic_load(&instance->accepting_events)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskGetCurrentTaskHandle() == instance->worker_task) {
        return ESP_ERR_INVALID_STATE;
    }
    ft_worker_event_t event = {
        .type = FT_WORK_SEND_CMD,
    };
    event.data.send.read_fn = param->read_fn;
    event.data.send.read_ctx = param->read_ctx;
    event.data.send.file_size = param->file_size;
    event.data.send.remote_name = strdup(param->remote_name);
    event.data.send.link = param->link;
    if (param->sha256) {
        event.data.send.sha256 = malloc(32);
        if (event.data.send.sha256) {
            memcpy(event.data.send.sha256, param->sha256, 32);
        }
    }
    event.data.send.reply.done = xSemaphoreCreateBinary();
    esp_err_t result = ESP_FAIL;
    event.data.send.reply.result = &result;
    if (!event.data.send.remote_name || (param->sha256 && !event.data.send.sha256) ||
            !event.data.send.reply.done) {
        free(event.data.send.remote_name);
        free(event.data.send.sha256);
        if (event.data.send.reply.done) {
            vSemaphoreDelete(event.data.send.reply.done);
        }
        return ESP_ERR_NO_MEM;
    }
    if (ft_event_queue_send(instance, &event, 0) != pdTRUE) {
        free(event.data.send.remote_name);
        free(event.data.send.sha256);
        vSemaphoreDelete(event.data.send.reply.done);
        return ESP_FT_ERR_QUEUE_FULL;
    }
    ft_worker_wake(instance);
    xSemaphoreTake(event.data.send.reply.done, portMAX_DELAY);
    vSemaphoreDelete(event.data.send.reply.done);
    return result;
}

esp_err_t esp_file_transfer_abort(void)
{
    ft_instance_t *instance = &s_ft;
    if (!instance->initialized || !atomic_load(&instance->accepting_events)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xTaskGetCurrentTaskHandle() == instance->worker_task) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!instance->abort_mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(instance->abort_mutex, portMAX_DELAY);
    /* Re-check after lock: deinit may have raced past the earlier gate. */
    if (!instance->initialized || !atomic_load(&instance->accepting_events) ||
            !instance->snapshot_mutex) {
        xSemaphoreGive(instance->abort_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    esp_file_transfer_status_t status;
    esp_err_t status_err = esp_file_transfer_get_status(&status);
    if (status_err != ESP_OK || !status.in_progress) {
        xSemaphoreGive(instance->abort_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (!done) {
        xSemaphoreGive(instance->abort_mutex);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = ESP_FAIL;
    ft_worker_event_t event = {
        .type = FT_WORK_ABORT_CMD,
        .data.abort.reply = {
            .done = done,
            .result = &result,
        },
    };
    atomic_store(&instance->user_abort_completed, false);
    atomic_store(&instance->user_abort_pending, true);
    if (ft_event_queue_send(instance, &event, portMAX_DELAY) != pdTRUE) {
        atomic_store(&instance->user_abort_pending, false);
        vSemaphoreDelete(done);
        xSemaphoreGive(instance->abort_mutex);
        return ESP_FT_ERR_QUEUE_FULL;
    }
    ft_worker_wake(instance);
    xSemaphoreTake(done, portMAX_DELAY);
    vSemaphoreDelete(done);
    xSemaphoreGive(instance->abort_mutex);
    return result;
}

esp_err_t esp_file_transfer_get_status(esp_file_transfer_status_t *status)
{
    ft_instance_t *instance = &s_ft;
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!instance->initialized || !instance->snapshot_mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(instance->snapshot_mutex, portMAX_DELAY);
    *status = instance->snapshot;
    xSemaphoreGive(instance->snapshot_mutex);
    return ESP_OK;
}
