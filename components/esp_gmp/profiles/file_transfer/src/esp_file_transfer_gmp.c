/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_file_transfer_internal.h"
#include "esp_gmp_ft_proto.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"

static const char *TAG = "esp_file_transfer";

static bool command_known(uint8_t command)
{
    return command == ESP_FT_CMD_TRANSFER_META || command == ESP_FT_CMD_DATA_BLOCK ||
           command == ESP_FT_CMD_FINAL_CONFIRM || command == ESP_FT_CMD_ABORT;
}

static void send_immediate_status(const esp_gmp_rx_t *pkt, uint8_t status)
{
    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = pkt->op == ESP_GMP_OP_READ_REQ ? ESP_GMP_OP_READ_RSP : ESP_GMP_OP_WRITE_RSP,
        .group_id = pkt->group_id,
        .sequence = pkt->sequence,
        .command_id = pkt->command_id,
        .flags = 0,
        .status = status,
    };
    esp_err_t err = esp_gmp_send(pkt->link, &tx, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "failed to send immediate GMP status: %s", esp_err_to_name(err));
    }
}

esp_err_t ft_gmp_send(ft_instance_t *instance, uint8_t op, uint16_t sequence,
                      uint8_t command, uint8_t status, const uint8_t *payload,
                      size_t payload_len)
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
    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = op,
        .group_id = ESP_GMP_GRP_FILE_TRANSFER,
        .sequence = sequence,
        .command_id = command,
        .flags = 0,
        .status = status,
    };
    return esp_gmp_send(link, &tx, payload, payload_len);
}

esp_err_t ft_gmp_send_request(ft_instance_t *instance, ft_context_t *ctx, uint8_t command,
                              const uint8_t *payload, size_t payload_len, bool expect_response)
{
    if (!instance || !ctx || ctx->pending_valid) {
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t sequence = ft_next_sequence(instance);
    esp_err_t err = ft_gmp_send(instance, ESP_GMP_OP_WRITE_REQ, sequence, command,
                                ESP_GMP_STATUS_OK, payload, payload_len);
    if (err == ESP_OK && expect_response) {
        ctx->pending_valid = true;
        ctx->pending_sequence = sequence;
        ctx->pending_command = command;
    }
    return err;
}

esp_err_t ft_gmp_send_response(ft_instance_t *instance, const ft_gmp_packet_t *request,
                               uint8_t gmp_status, const uint8_t *payload, size_t payload_len)
{
    if (!instance || !request || request->op != ESP_GMP_OP_WRITE_REQ || !request->link) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_gmp_tx_params_t tx = {
        .ver = ESP_GMP_VER,
        .op = ESP_GMP_OP_WRITE_RSP,
        .group_id = ESP_GMP_GRP_FILE_TRANSFER,
        .sequence = request->sequence,
        .command_id = request->command_id,
        .flags = 0,
        .status = gmp_status,
    };
    return esp_gmp_send(request->link, &tx, payload, payload_len);
}

void ft_gmp_send_abort(ft_instance_t *instance, ft_context_t *ctx, uint16_t reason)
{
    if (!instance || !ctx || !ctx->metadata_sent || reason == ESP_FT_REASON_OK) {
        return;
    }
    uint8_t payload[ESP_FT_ABORT_LEN];
    ft_abort_t abort_msg = {
        .transfer_id = ctx->transfer_id,
        .reason_code = reason,
    };
    if (ft_protocol_encode_abort(&abort_msg, payload, sizeof(payload)) == ESP_OK) {
        esp_err_t err = ft_gmp_send(instance, ESP_GMP_OP_WRITE_REQ,
                                    ft_next_sequence(instance), ESP_FT_CMD_ABORT,
                                    ESP_GMP_STATUS_OK, payload, sizeof(payload));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "failed to send abort: %s", esp_err_to_name(err));
        }
    }
}

bool ft_gmp_pending_matches(const ft_context_t *ctx, const ft_gmp_packet_t *packet,
                            uint8_t command)
{
    return ctx && packet && ctx->pending_valid &&
           packet->link == ctx->link && packet->op == ESP_GMP_OP_WRITE_RSP &&
           packet->sequence == ctx->pending_sequence &&
           packet->command_id == command && ctx->pending_command == command;
}

bool esp_file_transfer_on_packet(const esp_gmp_rx_t *pkt)
{
    ft_instance_t *instance = ft_instance_get();
    if (!pkt || !ft_async_enter(instance)) {
        return false;
    }

    if (pkt->group_id != ESP_GMP_GRP_FILE_TRANSFER) {
        ft_async_exit(instance);
        return false;
    }

    if (!command_known(pkt->command_id)) {
        if (pkt->op == ESP_GMP_OP_WRITE_REQ || pkt->op == ESP_GMP_OP_READ_REQ) {
            send_immediate_status(pkt, ESP_GMP_STATUS_UNKNOWN_COMMAND);
        }
        ft_async_exit(instance);
        return false;
    }

    if (pkt->ver != ESP_GMP_VER || pkt->flags != 0) {
        if (pkt->op == ESP_GMP_OP_WRITE_REQ || pkt->op == ESP_GMP_OP_READ_REQ) {
            send_immediate_status(pkt, ESP_GMP_STATUS_NOT_SUPPORTED);
        }
        ft_async_exit(instance);
        return false;
    }
    if (pkt->payload_len != 0 && !pkt->payload) {
        if (pkt->op == ESP_GMP_OP_WRITE_REQ || pkt->op == ESP_GMP_OP_READ_REQ) {
            send_immediate_status(pkt, ESP_GMP_STATUS_BAD_LENGTH);
        }
        ft_async_exit(instance);
        return false;
    }
    if (pkt->op == ESP_GMP_OP_READ_REQ || pkt->op == ESP_GMP_OP_READ_RSP ||
            ((pkt->command_id == ESP_FT_CMD_FINAL_CONFIRM ||
              pkt->command_id == ESP_FT_CMD_ABORT) &&
             pkt->op != ESP_GMP_OP_WRITE_REQ)) {
        if (pkt->op == ESP_GMP_OP_READ_REQ) {
            send_immediate_status(pkt, ESP_GMP_STATUS_NOT_SUPPORTED);
        }
        ft_async_exit(instance);
        return false;
    }

    ft_worker_event_t event = {
        .type = FT_WORK_GMP_PACKET,
        .data.packet = {
            .link = pkt->link,
            .ver = pkt->ver,
            .op = pkt->op,
            .group_id = pkt->group_id,
            .sequence = pkt->sequence,
            .command_id = pkt->command_id,
            .flags = pkt->flags,
            .status = pkt->status,
            .payload_len = pkt->payload_len,
        },
    };
    if (pkt->payload_len) {
        event.data.packet.payload = malloc(pkt->payload_len);
        if (!event.data.packet.payload) {
            if (pkt->op == ESP_GMP_OP_WRITE_REQ &&
                    (pkt->command_id == ESP_FT_CMD_TRANSFER_META ||
                     pkt->command_id == ESP_FT_CMD_DATA_BLOCK)) {
                send_immediate_status(pkt, ESP_GMP_STATUS_INTERNAL);
            }
            ft_async_exit(instance);
            return false;
        }
        memcpy(event.data.packet.payload, pkt->payload, pkt->payload_len);
    }

    bool urgent = pkt->op == ESP_GMP_OP_WRITE_REQ &&
                  (pkt->command_id == ESP_FT_CMD_FINAL_CONFIRM ||
                   pkt->command_id == ESP_FT_CMD_ABORT);

    BaseType_t queued = pdFALSE;
    if (urgent) {
        queued = xQueueSend(instance->urgent_queue, &event, 0);
        if (queued != pdTRUE) {
            queued = ft_event_queue_send(instance, &event, 0);
        }
        /* Prefer room by dropping only GMP/SEND work — never ABORT/DEINIT/TIMEOUT. */
        if (queued != pdTRUE) {
            ft_worker_event_t saved[ESP_FT_EVENT_QUEUE_LEN];
            int saved_count = 0;
            bool evicted = false;
            ft_worker_event_t item;

            ft_event_queue_lock(instance);
            while (xQueueReceive(instance->event_queue, &item, 0) == pdTRUE) {
                if (!evicted && (item.type == FT_WORK_GMP_PACKET ||
                                 item.type == FT_WORK_SEND_CMD)) {
                    ft_discard_worker_event(instance, &item, ESP_FT_ERR_QUEUE_FULL);
                    evicted = true;
                    ESP_LOGW(TAG, "evicted queued work for urgent cmd 0x%02x",
                             pkt->command_id);
                    continue;
                }
                if (saved_count < (int)ESP_FT_EVENT_QUEUE_LEN) {
                    saved[saved_count++] = item;
                } else {
                    ft_discard_worker_event(instance, &item, ESP_FT_ERR_QUEUE_FULL);
                }
            }
            for (int i = 0; i < saved_count; i++) {
                if (xQueueSend(instance->event_queue, &saved[i], 0) != pdTRUE) {
                    /* Should not happen under lock; complete waiters if it does. */
                    ft_discard_worker_event(instance, &saved[i], ESP_FT_ERR_QUEUE_FULL);
                }
            }
            queued = xQueueSend(instance->urgent_queue, &event, 0);
            if (queued != pdTRUE) {
                queued = xQueueSend(instance->event_queue, &event, 0);
            }
            ft_event_queue_unlock(instance);
        }
    } else {
        queued = ft_event_queue_send(instance, &event, 0);
    }

    if (queued != pdTRUE) {
        free(event.data.packet.payload);
        if (pkt->op == ESP_GMP_OP_WRITE_REQ &&
                (pkt->command_id == ESP_FT_CMD_TRANSFER_META ||
                 pkt->command_id == ESP_FT_CMD_DATA_BLOCK ||
                 pkt->command_id == ESP_FT_CMD_FINAL_CONFIRM ||
                 pkt->command_id == ESP_FT_CMD_ABORT)) {
            send_immediate_status(pkt, ESP_GMP_STATUS_BUSY);
        }
        if (urgent) {
            ESP_LOGE(TAG, "failed to queue urgent command 0x%02x", pkt->command_id);
        }
        ft_async_exit(instance);
        return false;
    }
    ft_worker_wake(instance);
    ft_async_exit(instance);
    return false;
}

void esp_file_transfer_on_link_down(esp_gmp_link_t link)
{
    ft_instance_t *instance = ft_instance_get();
    if (!ft_async_enter(instance)) {
        return;
    }

    /* Always defer to the worker. Never free sessions here — active_ctx may be
     * temporarily switched to another link while handling a packet. */
    ft_pending_link_down_push(instance, link);
    ft_worker_wake(instance);

    ft_async_exit(instance);
}

void esp_file_transfer_on_transport_error(esp_gmp_link_t link, esp_err_t error)
{
    ft_instance_t *instance = ft_instance_get();
    if (!ft_async_enter(instance)) {
        return;
    }

    /* Publish payload, then both pending flags under the same critical section. */
    atomic_store(&instance->transport_error, error);
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
            atomic_store(&instance->link_down_all_pending, true);
        }
        if (instance->pending_link_down_count < FT_MAX_SESSIONS) {
            instance->pending_link_downs[instance->pending_link_down_count++] = link;
        } else {
            atomic_store(&instance->link_down_all_pending, true);
        }
    }
    atomic_store(&instance->transport_error_pending, true);
    atomic_store(&instance->link_down_pending, true);
    portEXIT_CRITICAL(&instance->pending_link_lock);
    ft_worker_wake(instance);

    ft_async_exit(instance);
}
