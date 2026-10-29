/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @file esp_flux_session_priv.h
 * @brief Private flux_session layout — not part of the public API.
 *
 * Only esp_flux sources (and rare privileged tests) should include this header.
 * Application code must use the opaque flux_session_t handle and public APIs.
 */

#include "esp_flux_transport.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sys/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

SLIST_HEAD(flux_session_list, flux_session);

/**
 * @brief Fragment receive state (reassembly tracking)
 */
typedef struct {
    bool recv_active;
    uint8_t stream_id;
    uint16_t total_fragments;
    uint16_t received_fragments;
    uint32_t total_size;
    uint32_t received_size;
    uint8_t *reassembly_buffer;
    uint32_t reassembly_buffer_size;
    uint8_t fragment_bitmap[FLUX_SACK_BITMAP_SIZE];
    uint32_t start_time;
    uint32_t last_fragment_time;
    uint16_t last_continuous_seq;
    uint32_t last_ack_time;
    uint32_t last_continuous_time;
    bool pending_ack;
    uint8_t window_size;
    uint8_t window_threshold_percent;
    uint16_t last_ack_missing_count;
    uint16_t acked_fragments;
    uint16_t first_fragment_size;
} flux_fragment_recv_state_t;

/**
 * @brief Fragment send state (transmission tracking)
 */
typedef struct {
    bool send_active;
    uint8_t stream_id;
    bool completed;
    uint8_t max_retries;
    esp_err_t complete_status;
    uint16_t total_fragments;
    uint16_t sent_fragments;
    uint32_t total_size;
    uint32_t sent_size;
    const uint8_t *source_data;
    uint32_t source_size;
    uint16_t current_fragment;
    uint8_t window_size;
    uint8_t window_threshold_percent;
    uint16_t in_flight;
    bool *fragment_acked;
    uint32_t start_time;
    uint32_t last_send_time;
    uint32_t *fragment_send_time;
    uint8_t *fragment_retry_count;
    uint32_t retransmit_timeout_ms;
} flux_fragment_send_state_t;

/**
 * @brief Reliable-transfer state for one logical link (internal).
 */
struct flux_session {
    SLIST_ENTRY(flux_session) next;
    uint8_t next_stream_id;
    uint32_t id;
    uint16_t mtu_size;
    uint16_t max_fragment_data_size;
    uint16_t max_start_data_size;
    const flux_transport_ops_t *ops;
    void *transport_ctx;
    flux_callbacks_t callbacks;
    SemaphoreHandle_t state_mutex;
    bool destroying;
    uint16_t callback_pending;
    uint16_t callback_inflight;
    bool last_completed_recv_valid;
    uint8_t last_completed_recv_stream_id;
    flux_fragment_recv_state_t fragment_recv[FLUX_MAX_CONCURRENT_RECVS];
    flux_fragment_send_state_t fragment_send[FLUX_MAX_CONCURRENT_SENDS];
};

/** Internal helpers used only by esp_flux_transport.c */
flux_fragment_send_state_t *flux_session_send_alloc(flux_session_t *session);
flux_fragment_send_state_t *flux_session_send_find(flux_session_t *session, uint8_t stream_id);

#ifdef __cplusplus
}
#endif
