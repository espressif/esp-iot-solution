/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_file_transfer_internal.h"

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "esp_file_transfer";

#ifndef CONFIG_ESP_GMP_FT_DATA_PIPE_DEPTH
#define CONFIG_ESP_GMP_FT_DATA_PIPE_DEPTH 2
#endif

static void sender_fail(ft_instance_t *instance, uint16_t reason, esp_err_t detail,
                        bool notify_peer)
{
    ft_abort_active(instance, reason, detail, notify_peer, false);
}

static void sender_progress(ft_instance_t *instance)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx) {
        return;
    }
    uint8_t percent = ctx->file_size == 0 ? 100 :
                      (uint8_t)((ctx->bytes_transferred * 100) / ctx->file_size);
    if (!ctx->progress_reported || percent == 100 ||
            percent >= (uint8_t)(ctx->last_reported_percent + 5)) {
        ctx->progress_reported = true;
        ctx->last_reported_percent = percent;
        ft_emit_event(instance, ESP_FT_EVENT_PROGRESS);
    }
}

static size_t sender_pipe_depth_limit(ft_instance_t *instance)
{
    size_t cfg = (size_t)CONFIG_ESP_GMP_FT_DATA_PIPE_DEPTH;
    if (cfg < 1) {
        cfg = 1;
    }
    if (cfg > (size_t)FT_DATA_PIPE_MAX) {
        cfg = (size_t)FT_DATA_PIPE_MAX;
    }
    esp_gmp_link_t link = instance->gmp_link;
    if (instance->active_ctx && instance->active_ctx->link) {
        link = instance->active_ctx->link;
    }
    size_t recommended = esp_gmp_recommended_pipeline_depth(link);
    if (recommended < 1) {
        recommended = 1;
    }
    return cfg < recommended ? cfg : recommended;
}

static esp_err_t sender_read(ft_context_t *ctx, uint64_t offset, uint8_t *buf, size_t len,
                             size_t *out_len)
{
    if (!ctx || !buf || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ctx->read_fn) {
        size_t got = 0;
        esp_err_t err = ctx->read_fn(ctx->read_ctx, (size_t)offset, buf, len, &got);
        if (err != ESP_OK) {
            return err;
        }
        *out_len = got;
        return ESP_OK;
    }
    if (!ctx->file) {
        return ESP_ERR_INVALID_STATE;
    }
    if (fseek(ctx->file, (long)offset, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    *out_len = fread(buf, 1, len, ctx->file);
    if (*out_len != len && ferror(ctx->file)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void handle_final(ft_instance_t *instance, const ft_final_confirm_t *final)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx || final->transfer_id != ctx->transfer_id) {
        return;
    }
    ft_timer_disarm(instance);
    if (final->status != ESP_FT_WIRE_STATUS_OK) {
        sender_fail(instance, final->reason_code, ESP_FAIL, false);
        return;
    }
    if (final->received_size != ctx->file_size) {
        sender_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_SIZE, true);
        return;
    }
    if (memcmp(final->received_sha256, ctx->expected_sha256, 32) != 0) {
        sender_fail(instance, ESP_FT_REASON_HASH_MISMATCH, ESP_ERR_INVALID_CRC, true);
        return;
    }
    memcpy(ctx->saved_name, final->saved_name, sizeof(ctx->saved_name));
    if (!ctx->progress_reported || ctx->last_reported_percent != 100) {
        ctx->bytes_transferred = ctx->file_size;
        sender_progress(instance);
    }
    ft_finish_transfer(instance, ESP_FT_EVENT_COMPLETED, ESP_FT_REASON_OK, ESP_OK);
}

static void use_early_final(ft_instance_t *instance)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx || !ctx->early_final_valid) {
        return;
    }
    ft_final_confirm_t final = ctx->early_final;
    ctx->early_final_valid = false;
    handle_final(instance, &final);
}

static void enter_wait_final(ft_instance_t *instance)
{
    ft_context_t *ctx = instance->active_ctx;
    ctx->state = TRANSFER_STATE_WAIT_FINAL_CONFIRM;
    ft_snapshot_update(instance);
    if (ctx->early_final_valid) {
        use_early_final(instance);
        return;
    }
    esp_err_t err = ft_timer_arm(instance, FT_TIMER_FINAL, ESP_FT_FINAL_TIMEOUT_US);
    if (err != ESP_OK) {
        sender_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, err, true);
    }
}

static bool sender_all_blocks_acked(const ft_context_t *ctx)
{
    return ctx->next_send_block >= ctx->total_blocks &&
           ft_data_pipe_count(&ctx->data_pipe) == 0;
}

/**
 * Push as many DATA blocks as depth / TX headroom allow.
 * Wire format unchanged; receiver still sees in-order blocks (Flux/GMP preserve order).
 */
static void fill_data_pipeline(ft_instance_t *instance)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx || ctx->role != ESP_FILE_TRANSFER_ROLE_SENDER) {
        return;
    }
    /* Enter SENDING_DATA before the first TX attempt so empty-pipe backpressure
     * retries are recognized by ft_sender_handle_timeout. */
    if (ctx->state != TRANSFER_STATE_SENDING_DATA) {
        ctx->state = TRANSFER_STATE_SENDING_DATA;
        ft_snapshot_update(instance);
    }

    size_t depth = sender_pipe_depth_limit(instance);
    while (ctx->next_send_block < ctx->total_blocks &&
            (size_t)ft_data_pipe_count(&ctx->data_pipe) < depth) {
        if (atomic_load(&instance->user_abort_pending) ||
                atomic_load(&instance->link_down_pending) ||
                atomic_load(&instance->transport_error_pending)) {
            return;
        }

        uint64_t offset = (uint64_t)ctx->next_send_block * ctx->block_size;
        uint64_t remaining = ctx->file_size - offset;
        size_t expected = remaining > ctx->block_size ? ctx->block_size : (size_t)remaining;
        uint8_t *data = ctx->block_buffer + ESP_FT_DATA_REQ_HDR_LEN;
        size_t count = 0;
        esp_err_t read_err = sender_read(ctx, offset, data, expected, &count);
        if (read_err != ESP_OK || count != expected) {
            sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED,
                        read_err != ESP_OK ? read_err : ESP_ERR_INVALID_SIZE, true);
            return;
        }

        esp_err_t err = ft_data_send_request(instance, ctx, ctx->next_send_block, count);
        if (err == ESP_ERR_ESP_GMP_TX_QUEUE_FULL || err == ESP_ERR_NO_MEM) {
            /* Backpressure: keep already-queued inflight; retry after an ACK.
             * Path-based source rewinds the FILE*; stream read_fn is offset-based. */
            if (!ctx->read_fn && ctx->file &&
                    fseek(ctx->file, (long)offset, SEEK_SET) != 0) {
                sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED, ESP_FAIL, true);
                return;
            }
            ESP_LOGD(TAG, "DATA pipeline paused (backpressure=%s, inflight=%d)",
                     esp_err_to_name(err), ft_data_pipe_count(&ctx->data_pipe));
            if (ft_data_pipe_count(&ctx->data_pipe) == 0 &&
                    ctx->next_send_block < ctx->total_blocks) {
                /* Empty pipe: no ACK will arrive — schedule a short retry. */
                (void)ft_timer_arm(instance, FT_TIMER_BLOCK_RSP, ESP_FT_BACKPRESSURE_RETRY_US);
            }
            break;
        }
        if (err != ESP_OK) {
            sender_fail(instance, ESP_FT_REASON_DATA_SEND_FAILED, err, true);
            return;
        }

        ctx->next_send_block++;
        ft_snapshot_update(instance);
        esp_err_t timer_err = ft_timer_arm(instance, FT_TIMER_BLOCK_RSP,
                                           ESP_FT_BLOCK_RSP_TIMEOUT_US);
        if (timer_err != ESP_OK) {
            sender_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, timer_err, true);
            return;
        }
        esp_gmp_poll();
    }

    if (sender_all_blocks_acked(ctx)) {
        enter_wait_final(instance);
    }
}

static esp_err_t sender_start_common(ft_instance_t *instance, esp_gmp_link_t send_link,
                                     const char *file_name, uint64_t file_size,
                                     FILE *file, const char *src_path,
                                     esp_file_transfer_read_fn_t read_fn, void *read_ctx,
                                     const uint8_t *sha256)
{
    esp_gmp_link_t link = send_link ? send_link : instance->gmp_link;
    if (!link) {
        return ESP_ERR_INVALID_STATE;
    }

    if (instance->active_ctx &&
            instance->active_ctx->role != ESP_FILE_TRANSFER_ROLE_NONE &&
            instance->active_ctx->link &&
            instance->active_ctx->link != link) {
        return ESP_FT_ERR_BUSY;
    }

    xSemaphoreTake(instance->sessions_mutex, portMAX_DELAY);
    ft_sess_t *sess = ft_sess_get_or_alloc(instance, link);
    xSemaphoreGive(instance->sessions_mutex);
    if (!sess) {
        return ESP_ERR_NO_MEM;
    }
    if (sess->ctx.role != ESP_FILE_TRANSFER_ROLE_NONE) {
        return ESP_FT_ERR_BUSY;
    }

    esp_err_t err = ft_sess_refresh_capabilities(instance, sess);
    if (err != ESP_OK) {
        return err;
    }
    size_t block_size = sess->block_size;
    if (file_size > instance->max_file_size) {
        return ESP_FT_ERR_FILE_TOO_LARGE;
    }
    uint64_t blocks = file_size == 0 ? 0 : 1 + (file_size - 1) / block_size;
    if (blocks > UINT32_MAX) {
        return ESP_FT_ERR_FILE_TOO_LARGE;
    }

    ft_context_t *ctx = &sess->ctx;
    memset(ctx, 0, sizeof(*ctx));

    if (src_path) {
        ctx->source_path = strdup(src_path);
        if (!ctx->source_path) {
            return ESP_ERR_NO_MEM;
        }
    }
    ctx->block_buffer = malloc(block_size + ESP_FT_DATA_REQ_HDR_LEN);
    if (!ctx->block_buffer) {
        free(ctx->source_path);
        ctx->source_path = NULL;
        return ESP_ERR_NO_MEM;
    }
    if (!sha256) {
        err = ft_hash_create(&ctx->hash);
        if (err != ESP_OK) {
            free(ctx->source_path);
            free(ctx->block_buffer);
            ctx->source_path = NULL;
            ctx->block_buffer = NULL;
            return err;
        }
    }

    ctx->role = ESP_FILE_TRANSFER_ROLE_SENDER;
    ctx->state = TRANSFER_STATE_PREPARING;
    ctx->link = link;
    ctx->transfer_id = ft_next_transfer_id(instance);
    ctx->file_size = file_size;
    ctx->block_size = (uint32_t)block_size;
    ctx->total_blocks = (uint32_t)blocks;
    ctx->next_send_block = 0;
    ctx->file = file;
    ctx->read_fn = read_fn;
    ctx->read_ctx = read_ctx;
    ctx->block_buffer_size = block_size;
    ft_data_pipe_reset(&ctx->data_pipe);
    strcpy(ctx->file_name, file_name);
    if (sha256) {
        memcpy(ctx->expected_sha256, sha256, 32);
        ctx->sha256_precomputed = true;
    }

    ft_sess_apply_caps_to_instance(instance, sess);
    instance->active_ctx = ctx;
    ft_snapshot_update(instance);
    ft_emit_event(instance, ESP_FT_EVENT_STARTED);
    return ESP_OK;
}

esp_err_t ft_sender_start(ft_instance_t *instance, const char *src_path, const char *remote_name)
{
    if (!instance || !src_path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (instance->active_ctx) {
        return ESP_FT_ERR_BUSY;
    }
    const char *file_name = remote_name ? remote_name : ft_fs_basename(src_path);
    if (!ft_fs_name_valid(file_name)) {
        return ESP_FT_ERR_REMOTE_NAME_INVALID;
    }
    uint64_t file_size;
    esp_err_t err = ft_fs_source_info(src_path, &file_size);
    if (err != ESP_OK) {
        return err;
    }
    FILE *file = NULL;
    err = ft_fs_open_source(src_path, &file);
    if (err != ESP_OK) {
        return err;
    }
    err = sender_start_common(instance, instance->gmp_link, file_name, file_size,
                              file, src_path, NULL, NULL, NULL);
    if (err != ESP_OK) {
        fclose(file);
    }
    return err;
}

esp_err_t ft_sender_start_stream(ft_instance_t *instance,
                                 const esp_file_transfer_send_stream_param_t *param)
{
    if (!instance || !param || !param->read_fn || !param->remote_name) {
        return ESP_ERR_INVALID_ARG;
    }
    if (instance->active_ctx) {
        return ESP_FT_ERR_BUSY;
    }
    if (!ft_fs_name_valid(param->remote_name)) {
        return ESP_FT_ERR_REMOTE_NAME_INVALID;
    }
    esp_gmp_link_t send_link = param->link ? param->link : instance->gmp_link;
    if (!send_link) {
        return ESP_ERR_INVALID_STATE;
    }
    return sender_start_common(instance, send_link, param->remote_name, param->file_size,
                               NULL, NULL, param->read_fn, param->read_ctx, param->sha256);
}

void ft_sender_prepare(ft_instance_t *instance)
{
    ft_context_t *ctx = instance ? instance->active_ctx : NULL;
    if (!ctx || ctx->role != ESP_FILE_TRANSFER_ROLE_SENDER ||
            ctx->state != TRANSFER_STATE_PREPARING) {
        return;
    }
    if (!ctx->sha256_precomputed) {
        if (ctx->read_fn) {
            uint64_t offset = 0;
            while (offset < ctx->file_size) {
                if (atomic_load(&instance->user_abort_pending) ||
                        atomic_load(&instance->link_down_pending) ||
                        atomic_load(&instance->transport_error_pending)) {
                    return;
                }
                uint64_t remaining = ctx->file_size - offset;
                size_t want = remaining > ctx->block_buffer_size ?
                              ctx->block_buffer_size : (size_t)remaining;
                uint8_t *data = ctx->block_buffer + ESP_FT_DATA_REQ_HDR_LEN;
                size_t count = 0;
                esp_err_t err = sender_read(ctx, offset, data, want, &count);
                if (err != ESP_OK || count != want) {
                    sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED,
                                err != ESP_OK ? err : ESP_ERR_INVALID_SIZE, false);
                    return;
                }
                if (ft_hash_update(ctx->hash, data, count) != ESP_OK) {
                    sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED, ESP_FAIL, false);
                    return;
                }
                offset += count;
            }
            if (ft_hash_finish(ctx->hash, ctx->expected_sha256) != ESP_OK) {
                sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED, ESP_FAIL, false);
                return;
            }
        } else {
            while (!feof(ctx->file)) {
                if (atomic_load(&instance->user_abort_pending) ||
                        atomic_load(&instance->link_down_pending) ||
                        atomic_load(&instance->transport_error_pending)) {
                    return;
                }
                uint8_t *data = ctx->block_buffer + ESP_FT_DATA_REQ_HDR_LEN;
                size_t count = fread(data, 1, ctx->block_buffer_size, ctx->file);
                if (count && ft_hash_update(ctx->hash, data, count) != ESP_OK) {
                    sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED, ESP_FAIL, false);
                    return;
                }
                if (count < ctx->block_buffer_size && ferror(ctx->file)) {
                    sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED, ESP_FAIL, false);
                    return;
                }
            }
            if (ft_hash_finish(ctx->hash, ctx->expected_sha256) != ESP_OK) {
                sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED, ESP_FAIL, false);
                return;
            }
            uint64_t current_size;
            if (ft_fs_source_info(ctx->source_path, &current_size) != ESP_OK ||
                    current_size != ctx->file_size || fseek(ctx->file, 0, SEEK_SET) != 0) {
                sender_fail(instance, ESP_FT_REASON_FILE_READ_FAILED, ESP_ERR_INVALID_SIZE, false);
                return;
            }
        }
    }

    ft_meta_request_t metadata = {
        .transfer_id = ctx->transfer_id,
        .file_size = ctx->file_size,
        .block_size = ctx->block_size,
        .total_blocks = ctx->total_blocks,
    };
    memcpy(metadata.sha256, ctx->expected_sha256, 32);
    strcpy(metadata.file_name, ctx->file_name);
    uint8_t payload[ESP_FT_META_REQ_MAX_LEN];
    size_t payload_len;
    esp_err_t err = ft_protocol_encode_meta_request(&metadata, payload, sizeof(payload),
                                                    &payload_len);
    if (err != ESP_OK) {
        sender_fail(instance, ESP_FT_REASON_DATA_SEND_FAILED, err, false);
        return;
    }
    if (ft_process_pending_termination(instance)) {
        return;
    }
    err = ESP_FAIL;
    for (uint8_t tx_try = 0; tx_try <= ESP_FT_META_TX_FULL_MAX_RETRIES; tx_try++) {
        if (ft_process_pending_termination(instance)) {
            return;
        }
        err = ft_gmp_send_request(instance, ctx, ESP_FT_CMD_TRANSFER_META,
                                  payload, payload_len, true);
        if (err == ESP_OK) {
            break;
        }
        if (err != ESP_ERR_ESP_GMP_TX_QUEUE_FULL && err != ESP_ERR_NO_MEM) {
            break;
        }
        ESP_LOGD(TAG, "META send backpressure (%s), retry %u",
                 esp_err_to_name(err), (unsigned)(tx_try + 1));
        esp_gmp_poll();
        vTaskDelay(pdMS_TO_TICKS(10U * (tx_try + 1U)));
    }
    if (err != ESP_OK) {
        sender_fail(instance, ESP_FT_REASON_DATA_SEND_FAILED, err, false);
        return;
    }
    ctx->metadata_sent = true;
    ctx->state = TRANSFER_STATE_WAIT_META_RSP;
    ft_snapshot_update(instance);
    ft_emit_event(instance, ESP_FT_EVENT_META_SENT);
    err = ft_timer_arm(instance, FT_TIMER_META_RSP, ESP_FT_META_RSP_TIMEOUT_US);
    if (err != ESP_OK) {
        sender_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, err, true);
    }
}

static void handle_meta_response(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ft_gmp_pending_matches(ctx, packet, ESP_FT_CMD_TRANSFER_META)) {
        return;
    }
    ft_timer_disarm(instance);
    ctx->pending_valid = false;
    if (packet->status != ESP_GMP_STATUS_OK) {
        uint16_t reason = packet->status == ESP_GMP_STATUS_NOT_SUPPORTED
                          ? ESP_FT_REASON_CAPABILITY_MISMATCH
                          : ESP_FT_REASON_DATA_SEND_FAILED;
        sender_fail(instance, reason, ESP_FAIL, true);
        return;
    }
    ft_meta_response_t response;
    if (ft_protocol_decode_meta_response(packet->payload, packet->payload_len, &response) != ESP_OK ||
            response.transfer_id != ctx->transfer_id) {
        sender_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_RESPONSE, true);
        return;
    }
    if (response.status == ESP_FT_WIRE_STATUS_REJECTED) {
        ctx->reason_code = response.reason_code;
        ft_emit_event(instance, ESP_FT_EVENT_PEER_REJECTED);
        sender_fail(instance, response.reason_code, ESP_FAIL, false);
        return;
    }
    if (response.accepted_block_size == 0 ||
            response.accepted_block_size > ctx->block_size) {
        sender_fail(instance, ESP_FT_REASON_CAPABILITY_MISMATCH,
                    ESP_ERR_INVALID_RESPONSE, true);
        return;
    }
    if (response.accepted_block_size < ctx->block_size) {
        ctx->block_size = response.accepted_block_size;
        uint64_t blocks = ctx->file_size == 0 ? 0 :
                          1 + (ctx->file_size - 1) / ctx->block_size;
        if (blocks > UINT32_MAX) {
            sender_fail(instance, ESP_FT_REASON_FILE_TOO_LARGE,
                        ESP_FT_ERR_FILE_TOO_LARGE, true);
            return;
        }
        ctx->total_blocks = (uint32_t)blocks;
        /* block_buffer was sized to the proposed (larger) block; still sufficient. */
    }
    ft_emit_event(instance, ESP_FT_EVENT_PEER_ACCEPTED);
    if (ctx->total_blocks == 0) {
        enter_wait_final(instance);
    } else {
        fill_data_pipeline(instance);
    }
}

static esp_err_t resend_data_block(ft_instance_t *instance, ft_context_t *ctx,
                                   uint32_t block_index, uint32_t data_len)
{
    uint64_t offset = (uint64_t)block_index * ctx->block_size;
    uint8_t *data = ctx->block_buffer + ESP_FT_DATA_REQ_HDR_LEN;
    size_t count = 0;
    esp_err_t err = sender_read(ctx, offset, data, data_len, &count);
    if (err != ESP_OK || count != data_len) {
        return err != ESP_OK ? err : ESP_ERR_INVALID_SIZE;
    }
    /* Restore file cursor to next unsent block (path-based source only). */
    if (!ctx->read_fn && ctx->file) {
        uint64_t resume = (uint64_t)ctx->next_send_block * ctx->block_size;
        if (fseek(ctx->file, (long)resume, SEEK_SET) != 0) {
            return ESP_FAIL;
        }
    }
    return ft_data_send_request(instance, ctx, block_index, data_len);
}

static void handle_data_response(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx || packet->op != ESP_GMP_OP_WRITE_RSP ||
            packet->command_id != ESP_FT_CMD_DATA_BLOCK ||
            packet->link != ctx->link) {
        return;
    }

    int slot = ft_data_pipe_find_seq(&ctx->data_pipe, packet->sequence);
    if (slot < 0) {
        return;
    }

    ft_data_pipe_slot_t done = ctx->data_pipe.slots[slot];
    ft_data_pipe_clear(&ctx->data_pipe, slot);

    if (packet->status == ESP_GMP_STATUS_BUSY) {
        ESP_LOGW(TAG, "DATA block %" PRIu32 " BUSY — retry", done.block_index);
        esp_err_t err = resend_data_block(instance, ctx, done.block_index, done.data_len);
        if (err != ESP_OK) {
            sender_fail(instance, ESP_FT_REASON_DATA_SEND_FAILED, err, true);
            return;
        }
        esp_err_t timer_err = ft_timer_arm(instance, FT_TIMER_BLOCK_RSP,
                                           ESP_FT_BLOCK_RSP_TIMEOUT_US);
        if (timer_err != ESP_OK) {
            sender_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, timer_err, true);
        }
        return;
    }

    if (packet->status != ESP_GMP_STATUS_OK) {
        sender_fail(instance, ESP_FT_REASON_DATA_SEND_FAILED, ESP_FAIL, true);
        return;
    }

    ft_data_response_t response;
    if (ft_protocol_decode_data_response(packet->payload, packet->payload_len, &response) != ESP_OK ||
            response.transfer_id != ctx->transfer_id ||
            response.block_index != done.block_index) {
        sender_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_RESPONSE, true);
        return;
    }
    if (response.status != ESP_FT_WIRE_STATUS_OK) {
        sender_fail(instance, response.reason_code, ESP_FAIL, true);
        return;
    }

    ctx->bytes_transferred += done.data_len;
    if (ctx->bytes_transferred > ctx->file_size) {
        ctx->bytes_transferred = ctx->file_size;
    }
    sender_progress(instance);

    if (ft_data_pipe_count(&ctx->data_pipe) > 0) {
        esp_err_t timer_err = ft_timer_arm(instance, FT_TIMER_BLOCK_RSP,
                                           ESP_FT_BLOCK_RSP_TIMEOUT_US);
        if (timer_err != ESP_OK) {
            sender_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, timer_err, true);
            return;
        }
    } else {
        ft_timer_disarm(instance);
    }

    if (sender_all_blocks_acked(ctx)) {
        enter_wait_final(instance);
    } else {
        fill_data_pipeline(instance);
    }
}

static void handle_final_packet(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx) {
        return;
    }
    if (packet->payload_len < sizeof(uint32_t)) {
        return;
    }
    uint32_t transfer_id = ((uint32_t)packet->payload[0] << 24) |
                           ((uint32_t)packet->payload[1] << 16) |
                           ((uint32_t)packet->payload[2] << 8) |
                           packet->payload[3];
    if (transfer_id != ctx->transfer_id) {
        return;
    }
    if (packet->status != ESP_GMP_STATUS_OK) {
        sender_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_RESPONSE, true);
        return;
    }
    ft_final_confirm_t final;
    if (ft_protocol_decode_final_confirm(packet->payload, packet->payload_len, &final) != ESP_OK ||
            final.transfer_id != ctx->transfer_id) {
        sender_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_RESPONSE, true);
        return;
    }
    if (ctx->state == TRANSFER_STATE_WAIT_FINAL_CONFIRM && !ctx->pending_valid &&
            ft_data_pipe_count(&ctx->data_pipe) == 0) {
        handle_final(instance, &final);
    } else if ((ctx->state == TRANSFER_STATE_WAIT_META_RSP && ctx->file_size == 0) ||
               (ctx->state == TRANSFER_STATE_SENDING_DATA &&
                ctx->next_send_block >= ctx->total_blocks)) {
        if (!ctx->early_final_valid) {
            ctx->early_final = final;
            ctx->early_final_valid = true;
        }
    } else if (!ctx->early_final_valid) {
        sender_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_STATE, true);
    }
}

void ft_sender_handle_packet(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    if (!instance || !instance->active_ctx || !packet) {
        return;
    }
    if (packet->command_id == ESP_FT_CMD_TRANSFER_META &&
            packet->op == ESP_GMP_OP_WRITE_RSP) {
        handle_meta_response(instance, packet);
    } else if (packet->command_id == ESP_FT_CMD_DATA_BLOCK &&
               packet->op == ESP_GMP_OP_WRITE_RSP) {
        handle_data_response(instance, packet);
    } else if (packet->command_id == ESP_FT_CMD_FINAL_CONFIRM &&
               packet->op == ESP_GMP_OP_WRITE_REQ) {
        handle_final_packet(instance, packet);
    }
}

void ft_sender_handle_timeout(ft_instance_t *instance, ft_timer_kind_t kind)
{
    ft_context_t *ctx = instance ? instance->active_ctx : NULL;
    if (!ctx || ctx->role != ESP_FILE_TRANSFER_ROLE_SENDER) {
        return;
    }
    /* Empty-pipe BLOCK_RSP timeout is a backpressure retry, not a peer timeout. */
    if (kind == FT_TIMER_BLOCK_RSP && ctx->state == TRANSFER_STATE_SENDING_DATA &&
            ft_data_pipe_count(&ctx->data_pipe) == 0 &&
            ctx->next_send_block < ctx->total_blocks) {
        fill_data_pipeline(instance);
        return;
    }
    bool expected = (kind == FT_TIMER_META_RSP && ctx->state == TRANSFER_STATE_WAIT_META_RSP) ||
                    (kind == FT_TIMER_BLOCK_RSP && ctx->state == TRANSFER_STATE_SENDING_DATA &&
                     ft_data_pipe_count(&ctx->data_pipe) > 0) ||
                    (kind == FT_TIMER_FINAL && ctx->state == TRANSFER_STATE_WAIT_FINAL_CONFIRM);
    if (expected) {
        sender_fail(instance, ESP_FT_REASON_TIMEOUT, ESP_ERR_TIMEOUT, true);
    }
}

void ft_handle_peer_abort(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    if (!instance || !instance->active_ctx || !packet) {
        return;
    }
    ft_abort_t abort_msg;
    if (ft_protocol_decode_abort(packet->payload, packet->payload_len, &abort_msg) != ESP_OK ||
            abort_msg.transfer_id != instance->active_ctx->transfer_id) {
        return;
    }
    ft_abort_active(instance, abort_msg.reason_code, ESP_OK, false, true);
}
