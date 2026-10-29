/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_file_transfer_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "esp_file_transfer";

static esp_err_t send_meta_response(ft_instance_t *instance, const ft_gmp_packet_t *packet,
                                    uint32_t transfer_id, uint8_t status, uint16_t reason,
                                    uint32_t block_size)
{
    ft_meta_response_t response = {
        .transfer_id = transfer_id,
        .status = status,
        .reason_code = reason,
        .accepted_block_size = status == ESP_FT_WIRE_STATUS_OK ? block_size : 0,
    };
    uint8_t payload[ESP_FT_META_RSP_LEN];
    esp_err_t err = ft_protocol_encode_meta_response(&response, payload, sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }
    return ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_OK, payload, sizeof(payload));
}

static esp_err_t send_final(ft_instance_t *instance, ft_context_t *ctx,
                            uint8_t status, uint16_t reason)
{
    ft_final_confirm_t final = {
        .transfer_id = ctx->transfer_id,
        .status = status,
        .reason_code = reason,
        .received_size = ctx->bytes_transferred,
    };
    memcpy(final.received_sha256, ctx->computed_sha256, sizeof(final.received_sha256));
    if (ctx->saved_name[0]) {
        strcpy(final.saved_name, ctx->saved_name);
    }
    uint8_t payload[ESP_FT_FINAL_MAX_LEN];
    size_t payload_len;
    esp_err_t err = ft_protocol_encode_final_confirm(&final, payload, sizeof(payload),
                                                     &payload_len);
    if (err != ESP_OK) {
        return err;
    }
    return ft_gmp_send_request(instance, ctx, ESP_FT_CMD_FINAL_CONFIRM,
                               payload, payload_len, false);
}

static void receiver_progress(ft_instance_t *instance)
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

static void receiver_fail(ft_instance_t *instance, uint16_t reason, esp_err_t detail,
                          bool send_abort)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx) {
        return;
    }
    if (send_abort) {
        ft_abort_active(instance, reason, detail, true, false);
        return;
    }
    ft_finish_transfer(instance, ESP_FT_EVENT_FAILED, reason, detail);
}

static void finalize_receiver(ft_instance_t *instance)
{
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx) {
        return;
    }
    if (ft_process_pending_termination(instance) && !instance->active_ctx) {
        return;
    }
    ctx = instance->active_ctx;
    if (!ctx) {
        return;
    }
    ctx->state = TRANSFER_STATE_FINALIZING;
    if (ctx->file_size == 0 && !ctx->progress_reported) {
        receiver_progress(instance);
    }
    ft_timer_disarm(instance);
    ft_snapshot_update(instance);
    ft_emit_event(instance, ESP_FT_EVENT_VERIFYING);

    esp_err_t hash_err = ft_hash_finish(ctx->hash, ctx->computed_sha256);
    int io_errno = 0;
    if (ctx->file) {
        errno = 0;
        int flush_result = fflush(ctx->file);
        if (flush_result != 0) {
            io_errno = errno;
        }
        errno = 0;
        int close_result = fclose(ctx->file);
        if (close_result != 0 && io_errno == 0) {
            io_errno = errno;
        }
        ctx->file = NULL;
    }
    if (ft_process_pending_termination(instance) && !instance->active_ctx) {
        return;
    }
    if (hash_err != ESP_OK) {
        ft_fs_remove(ctx->temp_path);
        send_final(instance, ctx, ESP_FT_WIRE_STATUS_FAILED,
                   ESP_FT_REASON_INTERNAL_ERROR);
        receiver_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, hash_err, false);
        return;
    }
    if (io_errno != 0) {
        uint16_t reason = io_errno == ENOSPC ? ESP_FT_REASON_NO_SPACE :
                          ESP_FT_REASON_FILE_WRITE_FAILED;
        ft_fs_remove(ctx->temp_path);
        send_final(instance, ctx, ESP_FT_WIRE_STATUS_FAILED, reason);
        receiver_fail(instance, reason, ESP_FAIL, false);
        return;
    }
    if (memcmp(ctx->computed_sha256, ctx->expected_sha256, 32) != 0) {
        ft_fs_remove(ctx->temp_path);
        send_final(instance, ctx, ESP_FT_WIRE_STATUS_FAILED, ESP_FT_REASON_HASH_MISMATCH);
        receiver_fail(instance, ESP_FT_REASON_HASH_MISMATCH, ESP_ERR_INVALID_CRC, false);
        return;
    }

    if (ft_process_pending_termination(instance) && !instance->active_ctx) {
        return;
    }
    esp_err_t err = ft_fs_commit_temp(instance->recv_dir, ctx->temp_path, ctx->file_name,
                                      &ctx->target_path, ctx->saved_name);
    if (err != ESP_OK) {
        ft_fs_remove(ctx->temp_path);
        send_final(instance, ctx, ESP_FT_WIRE_STATUS_FAILED,
                   ESP_FT_REASON_FILE_WRITE_FAILED);
        receiver_fail(instance, ESP_FT_REASON_FILE_WRITE_FAILED, err, false);
        return;
    }
    ctx->target_committed = true;
    if (!ctx->progress_reported || ctx->last_reported_percent != 100) {
        ctx->bytes_transferred = ctx->file_size;
        receiver_progress(instance);
    }
    err = send_final(instance, ctx, ESP_FT_WIRE_STATUS_OK, ESP_FT_REASON_OK);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to send final success: %s", esp_err_to_name(err));
        /* File is committed locally, but the peer never got FINAL — do not
         * report COMPLETED. */
        ft_finish_transfer(instance, ESP_FT_EVENT_FAILED,
                           ESP_FT_REASON_DATA_SEND_FAILED, err);
        return;
    }
    ft_finish_transfer(instance, ESP_FT_EVENT_COMPLETED, ESP_FT_REASON_OK, ESP_OK);
}

static void reject_metadata(ft_instance_t *instance, const ft_gmp_packet_t *packet,
                            const ft_meta_request_t *metadata, uint16_t reason,
                            esp_err_t detail)
{
    send_meta_response(instance, packet, metadata->transfer_id,
                       ESP_FT_WIRE_STATUS_REJECTED, reason, 0);
    ft_emit_rejected_event(instance, metadata->file_name, metadata->file_size, reason, detail);
}

void ft_receiver_handle_meta(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    if (!instance || !packet) {
        return;
    }
    if (packet->status != ESP_GMP_STATUS_OK) {
        ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        return;
    }
    ft_meta_request_t metadata = { 0 };
    esp_err_t err = ft_protocol_decode_meta_request(packet->payload, packet->payload_len,
                                                    &metadata);
    if (err != ESP_OK) {
        uint16_t reason = err == ESP_ERR_NOT_SUPPORTED ? ESP_FT_REASON_CAPABILITY_MISMATCH :
                          err == ESP_ERR_INVALID_RESPONSE ? ESP_FT_REASON_INVALID_FILE_NAME :
                          ESP_FT_REASON_INVALID_MESSAGE;
        if (metadata.transfer_id != 0) {
            reject_metadata(instance, packet, &metadata, reason, err);
        } else {
            ft_gmp_send_response(instance, packet,
                                 err == ESP_ERR_NOT_SUPPORTED ? ESP_GMP_STATUS_NOT_SUPPORTED :
                                 ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
            ft_emit_rejected_event(instance, NULL, 0, reason, err);
        }
        return;
    }
    /* handle_packet focuses sessions[i].ctx into active_ctx before calling us. */
    if (!instance->active_ctx) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_INTERNAL_ERROR,
                        ESP_ERR_INVALID_STATE);
        return;
    }
    if (instance->active_ctx->role != ESP_FILE_TRANSFER_ROLE_NONE) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_BUSY, ESP_FT_ERR_BUSY);
        return;
    }
    err = ft_refresh_capabilities(instance);
    if (err != ESP_OK) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_CAPABILITY_MISMATCH,
                        err);
        return;
    }
    if (!ft_fs_name_valid(metadata.file_name)) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_INVALID_FILE_NAME,
                        ESP_FT_ERR_REMOTE_NAME_INVALID);
        return;
    }
    if (metadata.file_size > instance->max_file_size) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_FILE_TOO_LARGE,
                        ESP_FT_ERR_FILE_TOO_LARGE);
        return;
    }
    /* Negotiate block size: accept min(proposed, local max, payload-derived max). */
    uint32_t payload_max = 0;
    if (instance->effective_payload > ESP_FT_DATA_REQ_HDR_LEN) {
        payload_max = (uint32_t)(instance->effective_payload - ESP_FT_DATA_REQ_HDR_LEN);
    }
    uint32_t accepted_block_size = metadata.block_size;
    if (accepted_block_size > instance->block_size) {
        accepted_block_size = (uint32_t)instance->block_size;
    }
    if (accepted_block_size > payload_max) {
        accepted_block_size = payload_max;
    }
    if (accepted_block_size == 0) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_CAPABILITY_MISMATCH,
                        ESP_FT_ERR_CAPABILITY_MISMATCH);
        return;
    }
    uint64_t accepted_blocks = metadata.file_size == 0 ? 0 :
                               1 + (metadata.file_size - 1) / accepted_block_size;
    if (accepted_blocks > UINT32_MAX) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_FILE_TOO_LARGE,
                        ESP_FT_ERR_FILE_TOO_LARGE);
        return;
    }
    uint64_t free_bytes;
    err = ft_fs_get_free_bytes(instance->recv_dir, &free_bytes);
    if (err == ESP_OK && free_bytes < metadata.file_size) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_NO_SPACE, ESP_ERR_NO_MEM);
        return;
    }
    if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "free-space query failed: %s", esp_err_to_name(err));
    }

    if (instance->accept_cb &&
            !instance->accept_cb(metadata.file_name, metadata.file_size, metadata.sha256,
                                 instance->accept_ctx)) {
        reject_metadata(instance, packet, &metadata, ESP_FT_REASON_REJECTED_BY_APP,
                        ESP_ERR_INVALID_STATE);
        return;
    }

    ft_context_t *ctx = instance->active_ctx;
    memset(ctx, 0, sizeof(*ctx));
    err = ft_fs_create_temp(instance->recv_dir, metadata.transfer_id, metadata.file_name,
                            &ctx->temp_path, &ctx->file);
    if (err != ESP_OK) {
        if (ctx->file) {
            fclose(ctx->file);
        }
        ft_fs_remove(ctx->temp_path);
        free(ctx->temp_path);
        memset(ctx, 0, sizeof(*ctx));
        reject_metadata(instance, packet, &metadata,
                        err == ESP_ERR_NO_MEM ? ESP_FT_REASON_NO_SPACE :
                        ESP_FT_REASON_FILE_OPEN_FAILED, err);
        return;
    }
    err = ft_hash_create(&ctx->hash);
    if (err != ESP_OK) {
        fclose(ctx->file);
        ft_fs_remove(ctx->temp_path);
        free(ctx->temp_path);
        memset(ctx, 0, sizeof(*ctx));
        reject_metadata(instance, packet, &metadata,
                        err == ESP_ERR_NOT_SUPPORTED
                        ? ESP_FT_REASON_CAPABILITY_MISMATCH
                        : ESP_FT_REASON_INTERNAL_ERROR,
                        err);
        return;
    }

    ctx->role = ESP_FILE_TRANSFER_ROLE_RECEIVER;
    ctx->state = TRANSFER_STATE_WAIT_DATA_BLOCK;
    ctx->link = packet->link ? packet->link : instance->gmp_link;
    ctx->transfer_id = metadata.transfer_id;
    ctx->file_size = metadata.file_size;
    ctx->block_size = accepted_block_size;
    ctx->total_blocks = (uint32_t)accepted_blocks;
    memcpy(ctx->expected_sha256, metadata.sha256, 32);
    strcpy(ctx->file_name, metadata.file_name);
    instance->active_ctx = ctx;
    ft_snapshot_update(instance);
    ft_emit_event(instance, ESP_FT_EVENT_META_RECEIVED);

    err = send_meta_response(instance, packet, ctx->transfer_id, ESP_FT_WIRE_STATUS_OK,
                             ESP_FT_REASON_OK, accepted_block_size);
    if (err != ESP_OK) {
        receiver_fail(instance, ESP_FT_REASON_DATA_SEND_FAILED, err, false);
        return;
    }
    ctx->metadata_sent = true;
    ft_emit_event(instance, ESP_FT_EVENT_STARTED);
    if (ctx->total_blocks == 0) {
        finalize_receiver(instance);
    } else {
        err = ft_timer_arm(instance, FT_TIMER_DATA_RX, ESP_FT_DATA_RX_TIMEOUT_US);
        if (err != ESP_OK) {
            receiver_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, err, true);
        }
    }
}

void ft_receiver_handle_data(ft_instance_t *instance, const ft_gmp_packet_t *packet)
{
    if (!instance || !packet) {
        return;
    }
    ft_context_t *ctx = instance->active_ctx;
    if (!ctx || ctx->role != ESP_FILE_TRANSFER_ROLE_RECEIVER ||
            ctx->state != TRANSFER_STATE_WAIT_DATA_BLOCK) {
        ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_BAD_STATE, NULL, 0);
        return;
    }
    if (packet->payload_len >= sizeof(uint32_t)) {
        uint32_t transfer_id = ((uint32_t)packet->payload[0] << 24) |
                               ((uint32_t)packet->payload[1] << 16) |
                               ((uint32_t)packet->payload[2] << 8) |
                               packet->payload[3];
        if (transfer_id != ctx->transfer_id) {
            ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_NO_SESSION, NULL, 0);
            return;
        }
    }
    ft_data_request_t request = { 0 };
    esp_err_t err = ft_protocol_decode_data_request(packet->payload, packet->payload_len,
                                                    &request);
    if (err != ESP_OK) {
        if (packet->payload_len >= ESP_FT_DATA_REQ_HDR_LEN && request.transfer_id != 0) {
            ft_data_send_response(instance, packet, request.transfer_id, request.block_index,
                                  ESP_FT_WIRE_STATUS_FAILED, ESP_FT_REASON_INVALID_MESSAGE);
        } else {
            ft_gmp_send_response(instance, packet, ESP_GMP_STATUS_BAD_LENGTH, NULL, 0);
        }
        receiver_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, err, false);
        return;
    }
    uint64_t remaining = ctx->file_size - ctx->bytes_transferred;
    size_t expected = remaining > ctx->block_size ? ctx->block_size : (size_t)remaining;
    if (request.transfer_id != ctx->transfer_id ||
            request.block_index != ctx->current_block || request.data_len != expected) {
        ft_data_send_response(instance, packet, request.transfer_id, request.block_index,
                              ESP_FT_WIRE_STATUS_FAILED, ESP_FT_REASON_INVALID_MESSAGE);
        receiver_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_SIZE, false);
        return;
    }
    ft_timer_disarm(instance);
    errno = 0;
    size_t written = fwrite(request.data, 1, request.data_len, ctx->file);
    if (written != request.data_len) {
        uint16_t reason = errno == ENOSPC ? ESP_FT_REASON_NO_SPACE :
                          ESP_FT_REASON_FILE_WRITE_FAILED;
        ft_data_send_response(instance, packet, request.transfer_id, request.block_index,
                              ESP_FT_WIRE_STATUS_FAILED, reason);
        receiver_fail(instance, reason, ESP_FAIL, false);
        return;
    }
    err = ft_hash_update(ctx->hash, request.data, request.data_len);
    if (err != ESP_OK) {
        ft_data_send_response(instance, packet, request.transfer_id, request.block_index,
                              ESP_FT_WIRE_STATUS_FAILED, ESP_FT_REASON_INTERNAL_ERROR);
        receiver_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, err, false);
        return;
    }
    err = ft_data_send_response(instance, packet, request.transfer_id, request.block_index,
                                ESP_FT_WIRE_STATUS_OK, ESP_FT_REASON_OK);
    if (err != ESP_OK) {
        receiver_fail(instance, ESP_FT_REASON_DATA_SEND_FAILED, err, false);
        return;
    }
    ctx->bytes_transferred += request.data_len;
    ++ctx->current_block;
    receiver_progress(instance);
    if (ctx->current_block == ctx->total_blocks) {
        if (ctx->bytes_transferred != ctx->file_size) {
            receiver_fail(instance, ESP_FT_REASON_INVALID_MESSAGE, ESP_ERR_INVALID_SIZE, false);
        } else {
            finalize_receiver(instance);
        }
    } else {
        err = ft_timer_arm(instance, FT_TIMER_DATA_RX, ESP_FT_DATA_RX_TIMEOUT_US);
        if (err != ESP_OK) {
            receiver_fail(instance, ESP_FT_REASON_INTERNAL_ERROR, err, true);
        }
    }
}

void ft_receiver_handle_timeout(ft_instance_t *instance, ft_timer_kind_t kind)
{
    ft_context_t *ctx = instance ? instance->active_ctx : NULL;
    if (ctx && ctx->role == ESP_FILE_TRANSFER_ROLE_RECEIVER &&
            ctx->state == TRANSFER_STATE_WAIT_DATA_BLOCK && kind == FT_TIMER_DATA_RX) {
        receiver_fail(instance, ESP_FT_REASON_TIMEOUT, ESP_ERR_TIMEOUT, true);
    }
}
