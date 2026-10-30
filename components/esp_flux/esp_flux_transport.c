/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_flux_transport.h"
#include "esp_flux_session_priv.h"
#include "sys/queue.h"

/* ────────────────────── Internal Packet Structures ────────────────────── */
typedef struct {
    uint8_t packet_head : 3;  // Packet header identifier: 0x1-single, 0x2-fragment, 0x3-ACK, 0x4-start
    uint8_t stream_id : 3;    // Stream ID (demultiplexes concurrent transfers, 0-15)
    uint8_t protocol_ver : 2; // Protocol version: 0x1-1.0, 0x2-1.1
    uint8_t reserved;         // Reserved for future use
    uint8_t seq_num;          // Packet sequence number (for fragment packets, this is fragment sequence number)
    uint8_t window_size;      // Window size
    uint8_t window_threshold_percent; // Window occupancy threshold (percentage, only for fragment start packet)
    uint16_t size;            // Data size
    uint16_t total_fragments; // Total fragment count (only for fragment start packet)
    uint32_t total_size;      // Total data size (only for fragment start packet)
    uint8_t data[0];          // Variable length data
} __attribute__((packed)) flux_start_packet_t;

typedef struct {
    uint8_t packet_head : 3;  // Packet header identifier: 0x1-single, 0x2-fragment, 0x3-ACK, 0x4-start
    uint8_t stream_id : 3;    // Stream ID (demultiplexes concurrent transfers, 0-15)
    uint8_t protocol_ver : 2; // Protocol version: 0x1-1.0, 0x2-1.1
    uint8_t seq_num;          // Packet sequence number (for fragment packets, this is fragment sequence number)
    uint16_t size;            // Data size
    uint8_t data[0];          // Variable length data
} __attribute__((packed)) flux_fragment_packet_t;

typedef struct {
    uint8_t packet_head : 3;  // Packet header identifier: 0x1-single, 0x2-fragment, 0x3-ACK, 0x4-start
    uint8_t stream_id : 3;    // Stream ID (echoes the transfer being acknowledged, 0-15)
    uint8_t protocol_ver : 2; // Protocol version: 0x1-1.0, 0x2-1.1
    uint8_t ack_num;          // Acknowledgment number
    uint16_t size;            // Data size
    uint8_t data[0];          // Variable length data
} __attribute__((packed)) flux_ack_packet_t;

// Queue message structure: contains conn_handle and packet pointer
typedef struct {
    flux_session_t *session;
    void *fragment_packet;  // Can be flux_start_packet_t, flux_fragment_packet_t, or flux_ack_packet_t
    uint16_t packet_len;    // Actual bytes fed from transport (defensive parser bound)
} __attribute__((packed)) flux_session_queue_msg_t;

#define SIZE_OF_START_WO_DATA       (sizeof(flux_start_packet_t))
#define SIZE_OF_FRAGMENT_WO_DATA    (sizeof(flux_fragment_packet_t))
#define SIZE_OF_ACK_WO_DATA         (sizeof(flux_ack_packet_t))

static const char *TAG = "esp_flux";

static uint32_t g_session_id_counter = 0;
static TaskHandle_t g_flux_session_task_handle = NULL;
static QueueHandle_t g_flux_session_queue = NULL;
static SemaphoreHandle_t g_flux_sessions_mutex = NULL;
static struct flux_session_list g_flux_sessions_list = SLIST_HEAD_INITIALIZER(g_flux_sessions_list);

static inline void flux_session_state_lock(flux_session_t *session)
{
    if (session && session->state_mutex) {
        xSemaphoreTake(session->state_mutex, portMAX_DELAY);
    }
}

static inline void flux_session_state_unlock(flux_session_t *session)
{
    if (session && session->state_mutex) {
        xSemaphoreGive(session->state_mutex);
    }
}

// static bool flux_session_callback_try_enter(flux_session_t *session)
// {
//     if (!session) {
//         return false;
//     }
//     flux_session_state_lock(session);
//     if (session->destroying) {
//         flux_session_state_unlock(session);
//         return false;
//     }
//     session->callback_inflight++;
//     flux_session_state_unlock(session);
//     return true;
// }

static bool flux_session_callback_begin_reserved(flux_session_t *session)
{
    if (!session) {
        return false;
    }
    bool entered = false;
    flux_session_state_lock(session);
    if (session->callback_pending > 0) {
        session->callback_pending--;
    }
    if (!session->destroying) {
        session->callback_inflight++;
        entered = true;
    }
    flux_session_state_unlock(session);
    return entered;
}

static void flux_session_callback_leave(flux_session_t *session)
{
    if (!session) {
        return;
    }
    flux_session_state_lock(session);
    if (session->callback_inflight > 0) {
        session->callback_inflight--;
    }
    flux_session_state_unlock(session);
}

static bool flux_session_is_active(flux_session_t *session)
{
    flux_session_t *it = NULL;
    SLIST_FOREACH(it, &g_flux_sessions_list, next) {
        if (it == session) {
            return true;
        }
    }
    return false;
}

// Find the active send slot stamped with the given stream_id (e.g. to route an incoming SACK).
flux_fragment_send_state_t *flux_session_send_find(flux_session_t *session, uint8_t stream_id)
{
    for (int i = 0; i < FLUX_MAX_CONCURRENT_SENDS; i++) {
        if (session->fragment_send[i].send_active && session->fragment_send[i].stream_id == stream_id) {
            return &session->fragment_send[i];
        }
    }
    return NULL;
}

// Allocate a free send slot (neither active nor pending a completion callback). Returns NULL if all busy.
flux_fragment_send_state_t *flux_session_send_alloc(flux_session_t *session)
{
    for (int i = 0; i < FLUX_MAX_CONCURRENT_SENDS; i++) {
        if (!session->fragment_send[i].send_active && !session->fragment_send[i].completed) {
            return &session->fragment_send[i];
        }
    }
    return NULL;
}

// Pick the next stream_id for a new transfer, avoiding collision with any currently busy send slot.
static uint8_t flux_session_alloc_stream_id(flux_session_t *session)
{
    for (int attempt = 0; attempt < 16; attempt++) {
        uint8_t candidate = session->next_stream_id & FLUX_STREAM_ID_MASK;
        session->next_stream_id = (session->next_stream_id + 1) & FLUX_STREAM_ID_MASK;

        bool in_use = false;
        for (int i = 0; i < FLUX_MAX_CONCURRENT_SENDS; i++) {
            if ((session->fragment_send[i].send_active || session->fragment_send[i].completed) &&
                    session->fragment_send[i].stream_id == candidate) {
                in_use = true;
                break;
            }
        }
        if (!in_use) {
            return candidate;
        }
    }
    // Fallback (should not happen with only 2 concurrent slots)
    return session->next_stream_id & FLUX_STREAM_ID_MASK;
}

// Circular age of stream_id relative to the rolling allocator (larger = allocated earlier).
static uint8_t flux_session_stream_circular_age(flux_session_t *session, uint8_t stream_id)
{
    uint8_t newest = (session->next_stream_id - 1) & FLUX_STREAM_ID_MASK;
    return (newest - stream_id) & FLUX_STREAM_ID_MASK;
}

static bool flux_session_send_all_dispatched(const flux_fragment_send_state_t *send_state)
{
    return send_state->current_fragment >= send_state->total_fragments;
}

static bool flux_session_send_blocked_by_older(flux_session_t *session,
                                               const flux_fragment_send_state_t *self)
{
    for (int i = 0; i < FLUX_MAX_CONCURRENT_SENDS; i++) {
        const flux_fragment_send_state_t *other = &session->fragment_send[i];
        if (!other->send_active || other == self) {
            continue;
        }
        if (flux_session_stream_circular_age(session, other->stream_id) >
                flux_session_stream_circular_age(session, self->stream_id) &&
                !flux_session_send_all_dispatched(other)) {
            return true;
        }
    }
    return false;
}

// Find the active receive slot reassembling the given stream_id.
static flux_fragment_recv_state_t *flux_session_recv_find(flux_session_t *session, uint8_t stream_id)
{
    for (int i = 0; i < FLUX_MAX_CONCURRENT_RECVS; i++) {
        if (session->fragment_recv[i].recv_active && session->fragment_recv[i].stream_id == stream_id) {
            return &session->fragment_recv[i];
        }
    }
    return NULL;
}

// Find the receive slot for the given stream_id, or allocate a free one (used on START). Returns NULL if full.
static flux_fragment_recv_state_t *flux_session_recv_find_or_alloc(flux_session_t *session, uint8_t stream_id)
{
    flux_fragment_recv_state_t *recv_state = flux_session_recv_find(session, stream_id);
    if (recv_state) {
        return recv_state;
    }
    for (int i = 0; i < FLUX_MAX_CONCURRENT_RECVS; i++) {
        if (!session->fragment_recv[i].recv_active) {
            session->fragment_recv[i].stream_id = stream_id;
            return &session->fragment_recv[i];
        }
    }
    return NULL;
}

// Send packet to queue for asynchronous processing (optional use)
static esp_err_t flux_session_queue_packet(flux_session_t *session, void *packet, uint16_t packet_len)
{
    if (!session || !packet || !g_flux_session_queue || packet_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    flux_session_queue_msg_t msg = {
        .session = session,
        .fragment_packet = packet,
        .packet_len = packet_len,
    };

    if (xQueueSend(g_flux_session_queue, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "Failed to queue packet for session=%p, queue full", session);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/* ────────────────────── Bitmap Helpers ────────────────────── */
// Check if fragment has been received
static bool fragment_is_received(const flux_fragment_recv_state_t *recv_state, uint16_t fragment_idx)
{
    if (fragment_idx >= recv_state->total_fragments) {
        return false;
    }

    uint8_t byte_idx = fragment_idx / 8;
    uint8_t bit_idx = fragment_idx % 8;
    if (byte_idx >= sizeof(recv_state->fragment_bitmap)) {
        return false;
    }
    return (recv_state->fragment_bitmap[byte_idx] & (1 << bit_idx)) != 0;
}

// Mark fragment as received
static void fragment_mark_received(flux_fragment_recv_state_t *recv_state, uint16_t fragment_idx)
{
    if (fragment_idx >= recv_state->total_fragments) {
        return;
    }
    uint8_t byte_idx = fragment_idx / 8;
    uint8_t bit_idx = fragment_idx % 8;
    if (byte_idx >= sizeof(recv_state->fragment_bitmap)) {
        return;
    }
    recv_state->fragment_bitmap[byte_idx] |= (1 << bit_idx);
}

// Calculate last continuously-received sequence number starting from fragment 0.
// Returns UINT16_MAX when fragment 0 has not been received yet (sentinel for "no continuous prefix").
static uint16_t fragment_get_last_continuous_seq(const flux_fragment_recv_state_t *recv_state)
{
    if (!fragment_is_received(recv_state, 0)) {
        return UINT16_MAX; // Fragment 0 not yet received — no continuous prefix exists
    }

    uint16_t last_continuous = 0;
    for (uint16_t i = 1; i < recv_state->total_fragments; i++) {
        if (fragment_is_received(recv_state, i)) {
            last_continuous = i;
        } else {
            break;
        }
    }
    return last_continuous;
}

// Calculate maximum sequence number received by receiver (regardless of continuity)
static uint16_t fragment_get_max_received_seq(const flux_fragment_recv_state_t *recv_state)
{
    uint16_t max_by_bitmap = sizeof(recv_state->fragment_bitmap) * 8;
    uint16_t upper = (recv_state->total_fragments < max_by_bitmap) ? recv_state->total_fragments : max_by_bitmap;
    if (upper == 0) {
        return 0;
    }
    // Search backwards to find maximum received sequence number
    for (int16_t i = upper - 1; i >= 0; i--) {
        if (fragment_is_received(recv_state, i)) {
            return i;
        }
    }

    // If no received fragments found, return 0
    return 0;
}

// Check if fragment has been acknowledged
static bool fragment_send_is_acked(const flux_fragment_send_state_t *send_state, uint16_t fragment_idx)
{
    if (!send_state->fragment_acked || fragment_idx >= send_state->total_fragments) {
        return false;
    }

    return send_state->fragment_acked[fragment_idx];
}

/* ────────────────────── Time Helper ────────────────────── */
static inline uint32_t flux_get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ────────────────────── SACK Bitmap Generation ────────────────────── */
// Generate SACK bitmap: based on maximum sequence number, 0 means received, 1 means lost
static esp_err_t flux_generate_sack_bitmap(flux_fragment_recv_state_t *recv_state,
                                           uint16_t max_seq, uint8_t *sack_bitmap, uint16_t *sack_count)
{
    if (!recv_state || !sack_bitmap || !sack_count) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t missing_count = 0;
    memset(sack_bitmap, 0, FLUX_SACK_BITMAP_SIZE);

    // Iterate through all fragments from 0 to max_seq, mark lost fragments
    // Bitmap only covers range from 0 to max_seq
    uint16_t bitmap_range = max_seq + 1;
    if (bitmap_range > recv_state->total_fragments) {
        bitmap_range = recv_state->total_fragments;
    }

    for (uint16_t i = 0; i < bitmap_range; i++) {
        if (!fragment_is_received(recv_state, i)) {
            uint8_t byte_idx = i / 8;
            uint8_t bit_idx = i % 8;
            if (byte_idx < FLUX_SACK_BITMAP_SIZE) {
                sack_bitmap[byte_idx] |= (1 << bit_idx);
                missing_count++;
            }
        }
    }

    *sack_count = missing_count;
    return ESP_OK;
}

/* ────────────────────── Send Helpers ────────────────────── */
static esp_err_t flux_send_via_transport(flux_session_t *session, const uint8_t *data, uint16_t size)
{
    if (!session || !session->ops || !session->ops->send) {
        return ESP_ERR_INVALID_ARG;
    }

    int retry = 0;
    esp_err_t ret;
    while (retry < FLUX_MAX_RETRIES) {
        ret = session->ops->send(session->transport_ctx, data, size);
        if (ret == ESP_OK) {
            return ESP_OK;
        }

        if (ret == ESP_ERR_NO_MEM) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        retry++;
    }
    ESP_LOGE(TAG, "Failed to send data after %d retries: %d", FLUX_MAX_RETRIES, ret);
    return ret;
}

/* ────────────────────── Send SACK ────────────────────── */
// Send SACK packet for a specific receive stream
static esp_err_t flux_send_sack(flux_session_t *session, flux_fragment_recv_state_t *recv_state,
                                uint16_t ack_num, bool include_sack)
{
    if (!session || !recv_state) {
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate SACK data size
    uint16_t sack_data_size = 0;
    uint8_t sack_bitmap[FLUX_SACK_BITMAP_SIZE] = {0};
    uint16_t sack_count = 0;

    if (include_sack && recv_state->recv_active) {
        // Use maximum sequence number in ACK to generate bitmap
        esp_err_t err = flux_generate_sack_bitmap(recv_state, ack_num, sack_bitmap, &sack_count);
        if (err == ESP_OK && sack_count > 0) {
            sack_data_size = FLUX_SACK_BITMAP_SIZE;
        }
    }

    // Allocate SACK packet (including SACK data)
    uint16_t packet_size = SIZE_OF_ACK_WO_DATA + sack_data_size;
    flux_ack_packet_t *sack_packet = calloc(1, packet_size);
    if (!sack_packet) {
        ESP_LOGE(TAG, "Failed to allocate SACK packet");
        return ESP_ERR_NO_MEM;
    }

    // Set packet type: SACK (stamp the stream so the sender can route it)
    sack_packet->packet_head = FLUX_PACKET_TYPE_SACK;
    sack_packet->stream_id = recv_state->stream_id;
    sack_packet->ack_num = ack_num;
    sack_packet->size = sack_data_size;

    // Copy SACK bitmap
    if (sack_data_size > 0) {
        memcpy(sack_packet->data, sack_bitmap, FLUX_SACK_BITMAP_SIZE);
    }

    // Calculate number of acknowledged fragments (for window occupancy calculation)
    // In bitmap, 0 means received (acknowledged), 1 means lost
    uint16_t acked_count = 0;
    if (include_sack && recv_state->recv_active && sack_data_size > 0) {
        // Has SACK bitmap: count number of 0s in bitmap (acknowledged fragments)
        uint16_t bitmap_range = ack_num + 1;
        if (bitmap_range > recv_state->total_fragments) {
            bitmap_range = recv_state->total_fragments;
        }
        for (uint16_t i = 0; i < bitmap_range; i++) {
            uint8_t byte_idx = i / 8;
            uint8_t bit_idx = i % 8;
            // In bitmap, 0 means received (acknowledged)
            if ((sack_bitmap[byte_idx] & (1 << bit_idx)) == 0) {
                acked_count++;
            }
        }
    } else if (recv_state->recv_active) {
        // No SACK bitmap: acknowledge all fragments <= ack_num
        acked_count = (ack_num + 1 < recv_state->total_fragments) ? (ack_num + 1) : recv_state->total_fragments;
    }

    // Batch send optimization: reduce log output frequency, only output detailed logs at VERBOSE level
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, sack_packet, packet_size, ESP_LOG_VERBOSE);
    // ESP_LOGI(TAG, "Sending SACK: conn_handle=%d, attr_handle=%d, attr_len=%d, ack_num=%d",
    //          session->conn_handle, session->write_val_handle, packet_size, ack_num);

    esp_err_t ret = flux_send_via_transport(session, (uint8_t *)sack_packet, packet_size);
    // Free SACK packet
    free(sack_packet);
    sack_packet = NULL;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send SACK: ret=%d", ret);
        return ESP_FAIL;
    }

    // Update number of acknowledged fragments (after successful send)
    if (ret == ESP_OK && recv_state->recv_active) {
        // Update to number of fragments acknowledged in this ACK
        recv_state->acked_fragments = acked_count;
        // ESP_LOGD(TAG, "SACK sent: stream=%d, ack_num=%d, acked_fragments=%d, sack_count=%d (role=%s)",
        //          recv_state->stream_id, ack_num, acked_count, sack_count,
        //          session->role == GATT_SESSION_ROLE_MASTER ? "MASTER" : "SLAVE");
    }

    return ret;
}

/* ────────────────────── Send Fragment Packet ────────────────────── */
// Send single fragment for the given send stream
static esp_err_t flux_send_fragment_packet(flux_session_t *session, flux_fragment_send_state_t *send_state,
                                           uint16_t fragment_idx, const uint8_t *data, uint16_t data_size)
{
    if (!session || !send_state || !data || data_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    void *packet = NULL;
    uint16_t packet_size = 0;

    if (fragment_idx == 0) {
        // First fragment: send start packet
        // START packet header is larger, need to check if total size exceeds MTU
        if (data_size > session->max_start_data_size) {
            ESP_LOGW(TAG, "Data size too large for START packet: %d > %d", data_size, session->max_start_data_size);
            return ESP_ERR_INVALID_SIZE;
        }
        packet_size = SIZE_OF_START_WO_DATA + data_size;
        flux_start_packet_t *start_packet = calloc(1, packet_size);
        if (!start_packet) {
            ESP_LOGE(TAG, "Failed to allocate START packet buffer");
            return ESP_ERR_NO_MEM;
        }
        start_packet->packet_head = FLUX_PACKET_TYPE_START;
        start_packet->stream_id = send_state->stream_id;
        start_packet->seq_num = fragment_idx;
        start_packet->window_size = send_state->window_size;
        start_packet->window_threshold_percent = send_state->window_threshold_percent;
        start_packet->total_fragments = send_state->total_fragments;
        start_packet->total_size = send_state->total_size;
        start_packet->size = data_size;
        // Copy data
        memcpy(start_packet->data, data, data_size);
        packet = start_packet;
    } else {
        // Normal fragment: send fragment packet
        // Calculate actual available data size (MTU - packet header size)
        if (data_size > session->max_fragment_data_size) {
            ESP_LOGW(TAG, "Data size too large for max fragment data size: %d > %d", data_size, session->max_fragment_data_size);
            return ESP_ERR_INVALID_SIZE;
        }
        packet_size = SIZE_OF_FRAGMENT_WO_DATA + data_size;
        flux_fragment_packet_t *fragment_packet = calloc(1, packet_size);
        if (!fragment_packet) {
            ESP_LOGE(TAG, "Failed to allocate fragment packet buffer");
            return ESP_ERR_NO_MEM;
        }
        fragment_packet->packet_head = FLUX_PACKET_TYPE_FRAGMENT;
        fragment_packet->stream_id = send_state->stream_id;
        fragment_packet->seq_num = fragment_idx;
        fragment_packet->size = data_size;
        // Copy data
        memcpy(fragment_packet->data, data, data_size);
        packet = fragment_packet;
    }

    // Batch send optimization: reduce log output frequency, only output detailed logs at VERBOSE level
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, packet, packet_size, ESP_LOG_VERBOSE);
    ESP_LOGD(TAG, "Sending stream=%d fragment_idx=%d, length=%d", send_state->stream_id, fragment_idx, packet_size);

    esp_err_t ret = flux_send_via_transport(session, (uint8_t *)packet, packet_size);
    // Free packet
    free(packet);
    packet = NULL;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "send_fragment_packet: Failed to send fragment %d: ret=%d", fragment_idx, ret);
        if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGI(TAG, "Not enough MBUFs available");
            return ESP_ERR_NO_MEM;
        }
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sent stream=%d fragment %d/%d, size=%d, total_size=%" PRIu32, send_state->stream_id, fragment_idx + 1,
             send_state->total_fragments, data_size, send_state->total_size);

    // Record send timestamp (for retransmission tracking)
    if (send_state->fragment_send_time && fragment_idx < send_state->total_fragments) {
        send_state->fragment_send_time[fragment_idx] = esp_timer_get_time() / 1000; // Convert to milliseconds
    }

    return ret;
}

/* ────────────────────── Mark Fragment Acked ────────────────────── */
// Return the index of the first fragment that has NOT been acknowledged yet.
// This is the correct lower bound for retransmit scans: using sent_fragments (an ACK
// count, not an index) would skip fragments when ACKs arrive out of order.
// Example: fragments 0, 2, 3 ACKed but fragment 1 missing → sent_fragments=3 would
// start the scan at i=3 and miss fragment 1; this function returns 1.
static uint16_t fragment_send_get_left_edge(const flux_fragment_send_state_t *send_state)
{
    if (!send_state->fragment_acked) {
        return 0;
    }

    for (uint16_t i = 0; i < send_state->total_fragments; i++) {
        if (!send_state->fragment_acked[i]) {
            return i;
        }
    }

    return send_state->total_fragments; // All acknowledged
}

// Mark fragment as acknowledged
static void fragment_send_mark_acked(flux_session_t *session, flux_fragment_send_state_t *send_state, uint16_t fragment_idx)
{
    if (send_state->fragment_acked && fragment_idx < send_state->total_fragments) {
        if (!send_state->fragment_acked[fragment_idx]) {
            send_state->fragment_acked[fragment_idx] = true;
            send_state->sent_fragments++;
            // Decrease in_flight (acknowledged fragments no longer occupy window)
            if (send_state->in_flight > 0) {
                send_state->in_flight--;
            }
            // Update sent_size: calculate data size of this fragment
            // First fragment uses START packet (can carry less data), subsequent fragments use normal fragment packets
            uint32_t offset;
            uint16_t fragment_data_size;
            if (fragment_idx == 0) {
                offset = 0;
                fragment_data_size = (send_state->total_size > session->max_start_data_size) ?
                                     session->max_start_data_size : send_state->total_size;
            } else {
                offset = session->max_start_data_size + (fragment_idx - 1) * session->max_fragment_data_size;
                uint32_t remaining = send_state->total_size - offset;
                fragment_data_size = (remaining > session->max_fragment_data_size) ?
                                     session->max_fragment_data_size : remaining;
            }
            send_state->sent_size += fragment_data_size;

            if (send_state->sent_fragments >= send_state->total_fragments) {
                // All fragments have been sent and acknowledged
                send_state->send_active = false;

                uint32_t elapsed = (esp_timer_get_time() / 1000) - send_state->start_time;
                ESP_LOGW(TAG, "Fragment send completed (stream=%d): %lu bytes in %d fragments, elapsed: %lu ms",
                         send_state->stream_id, send_state->total_size, send_state->total_fragments, elapsed);

                // Defer completion callback to the transmit task's safe point to avoid
                // re-entering the SACK handler / clearing state mid-processing.
                send_state->completed = true;
                send_state->complete_status = ESP_OK;
            }
        }
    }
}

/* ────────────────────── Send Continue (window + retransmit) ────────────────────── */
static void flux_calc_fragment_offset(flux_session_t *session, uint16_t fragment_idx,
                                      uint32_t total_size, uint32_t *offset, uint16_t *data_size)
{
    if (fragment_idx == 0) {
        *offset = 0;
        *data_size = (total_size > session->max_start_data_size) ? session->max_start_data_size : total_size;
    } else {
        *offset = session->max_start_data_size + (fragment_idx - 1) * session->max_fragment_data_size;
        uint32_t remaining = total_size - *offset;
        *data_size = (remaining > session->max_fragment_data_size) ? session->max_fragment_data_size : remaining;
    }
}

// Terminate a send transfer immediately with an error.
// Sets the deferred-completion flag so the transmit task loop fires the callback safely.
static void flux_send_state_abort(flux_fragment_send_state_t *send_state, esp_err_t status)
{
    send_state->send_active = false;
    send_state->completed   = true;
    send_state->complete_status = status;
}

static esp_err_t flux_send_continue(flux_session_t *session, flux_fragment_send_state_t *send_state)
{
    if (!session || !send_state || !send_state->send_active) {
        return ESP_ERR_INVALID_STATE;
    }

    // If all fragments have been sent, check if all are acknowledged
    if (send_state->current_fragment >= send_state->total_fragments && send_state->sent_fragments < send_state->total_fragments) {
        // Wait for more ACKs
        return ESP_OK;
    }

    // Window control: decide whether to limit sending based on window_size
    uint32_t batch_start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    uint16_t batch_sent_count = 0;
    uint32_t batch_sent_size = 0;
    uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds

    // Check window limit: if window_size != 0xFF, need to check in_flight
    bool window_limited = (send_state->window_size != 0xFF);
    uint16_t available_window = 0;
    if (window_limited) {
        // Calculate available window: window size - number of sent but unacknowledged fragments
        if (send_state->in_flight >= send_state->window_size) {
            // Window full, but check if we need to retransmit timeout fragments
            // Retransmission doesn't increase in_flight (already counted)
            bool has_timeout_retransmit = false;
            if (send_state->fragment_send_time && send_state->fragment_retry_count) {
                for (uint16_t i = fragment_send_get_left_edge(send_state); i < send_state->current_fragment; i++) {
                    if (!fragment_send_is_acked(send_state, i)) {
                        uint32_t send_time = send_state->fragment_send_time[i];
                        if (send_time > 0 && current_time > send_time && (current_time - send_time) > send_state->retransmit_timeout_ms) {
                            if (send_state->fragment_retry_count[i] < send_state->max_retries) {
                                has_timeout_retransmit = true;
                                break;
                            }
                        }
                    }
                }
            }
            if (!has_timeout_retransmit) {
                // Window full and no timeout retransmit needed, wait for ACK
                ESP_LOGD(TAG, "Window full: in_flight=%d, window_size=%d, waiting for ACK",
                         send_state->in_flight, send_state->window_size);
                return ESP_OK;
            }
        }
        available_window = send_state->window_size - send_state->in_flight;
        ESP_LOGD(TAG, "available_window=%d", available_window);
    }

    // First, handle timeout retransmissions (before sending new fragments)
    // Check all sent but unacknowledged fragments for timeout retransmission
    if (send_state->fragment_send_time && send_state->fragment_retry_count) {
        for (uint16_t i = fragment_send_get_left_edge(send_state); i < send_state->current_fragment; i++) {
            if (!fragment_send_is_acked(send_state, i)) {
                uint32_t send_time = send_state->fragment_send_time[i];
                if (send_time > 0 && current_time > send_time && (current_time - send_time) > send_state->retransmit_timeout_ms) {
                    // Check retransmission count
                    if (send_state->fragment_retry_count[i] < send_state->max_retries) {
                        ESP_LOGD(TAG, "Retransmitting timeout fragment %d (retry %d, timeout %lu ms)",
                                 i, send_state->fragment_retry_count[i], current_time - send_time);

                        // Calculate offset and data size for retransmission
                        uint32_t offset;
                        uint16_t fragment_data_size;
                        flux_calc_fragment_offset(session, i, send_state->total_size, &offset, &fragment_data_size);

                        esp_err_t err = flux_send_fragment_packet(session, send_state, i, send_state->source_data + offset, fragment_data_size);
                        if (err == ESP_OK) {
                            // Update retransmission tracking
                            send_state->fragment_send_time[i] = current_time;
                            send_state->fragment_retry_count[i]++;
                            // Note: in_flight should not be incremented here as it's already counted
                            batch_sent_count++;
                            batch_sent_size += fragment_data_size;
                        } else {
                            ESP_LOGW(TAG, "Failed to retransmit fragment %d: %d", i, err);
                        }
                    } else {
                        // Retry limit reached — this fragment can never be delivered.
                        // Abort the entire transfer immediately so the send slot is released
                        // and the caller receives an error callback without waiting for the
                        // 10-second global timeout.
                        ESP_LOGE(TAG, "Fragment %d exceeded max retries (%d), aborting transfer (stream=%d)",
                                 i, send_state->max_retries, send_state->stream_id);
                        flux_send_state_abort(send_state, ESP_ERR_TIMEOUT);
                        return ESP_FAIL;
                    }
                }
            }
        }
    }

    // Send new fragments: decide send count based on window limit
    while (send_state->current_fragment < send_state->total_fragments) {
        // Window limit check: if limited and window size reached, stop sending
        if (window_limited && send_state->in_flight >= send_state->window_size) {
            ESP_LOGD(TAG, "Window limit reached: in_flight=%d, window_size=%d",
                     send_state->in_flight, send_state->window_size);
            break;
        } else if (batch_sent_count >= FLUX_MAX_BATCH_SENT_COUNT) {
            ESP_LOGD(TAG, "Batch sent count reached %d, breaking", FLUX_MAX_BATCH_SENT_COUNT);
            break;
        }

        uint16_t fragment_idx = send_state->current_fragment;

        // Check if acknowledged (avoid resending acknowledged fragments)
        if (fragment_send_is_acked(send_state, fragment_idx)) {
            send_state->current_fragment++;
            continue;
        }

        // Check if already sent (avoid duplicate sending)
        bool already_sent = false;
        if (send_state->fragment_send_time && fragment_idx < send_state->total_fragments) {
            already_sent = (send_state->fragment_send_time[fragment_idx] > 0);
        }

        // If already sent but not acknowledged, skip (wait for retransmission mechanism to handle)
        if (already_sent) {
            ESP_LOGD(TAG, "Fragment %d already sent, skipping (waiting for ACK or retransmit)", fragment_idx);
            send_state->current_fragment++;
            continue;
        }

        // Calculate offset and data size: first fragment uses START packet, subsequent fragments use normal fragment packets
        uint32_t offset;
        uint16_t fragment_data_size;
        flux_calc_fragment_offset(session, fragment_idx, send_state->total_size, &offset, &fragment_data_size);

        esp_err_t err = flux_send_fragment_packet(session, send_state, fragment_idx, send_state->source_data + offset, fragment_data_size);
        if (err == ESP_OK) {
            // Update fragment send state
            uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
            if (send_state->fragment_send_time) {
                send_state->fragment_send_time[fragment_idx] = current_time;
            }
            if (send_state->fragment_retry_count) {
                send_state->fragment_retry_count[fragment_idx] = 0;  // First send, retry count is 0
            }

            // Update in_flight (number of sent but unacknowledged fragments)
            send_state->in_flight++;
            send_state->current_fragment++;
            batch_sent_count++;
            batch_sent_size += fragment_data_size;
        } else {
            ESP_LOGI(TAG, "send_continue: Failed to send fragment %d", fragment_idx);
            // Send failed, retry later (on next call to this API)
            break;
        }
    }

    // Update state and callback uniformly after batch send completion
    if (batch_sent_count > 0) {
        send_state->last_send_time = batch_start_time;

        ESP_LOGD(TAG, "Batch sent %d fragments (%" PRIu32 " bytes), total: %" PRIu32 "/%" PRIu32 " bytes",
                 batch_sent_count, batch_sent_size, send_state->sent_size, send_state->total_size);
    }

    return ESP_OK;
}

/* ────────────────────── Handle SACK (sender side) ────────────────────── */
static esp_err_t flux_handle_sack(flux_session_t *session, flux_fragment_send_state_t *send_state,
                                  uint16_t ack_num, const uint8_t *sack_bitmap)
{
    if (!session || !send_state || !send_state->send_active) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds

    // 1. Acknowledge fragments based on maximum sequence number in ACK
    // ack_num is the maximum sequence number received by receiver (regardless of continuity)
    // First acknowledge all received fragments (0 in bitmap means received)
    uint16_t acked_count = 0;

    if (sack_bitmap) {
        // Has SACK bitmap: acknowledge fragments based on bitmap
        // Bitmap covers range from 0 to ack_num, 0 means received, 1 means lost
        uint16_t bitmap_range = ack_num + 1;
        if (bitmap_range > send_state->total_fragments) {
            bitmap_range = send_state->total_fragments;
        }

        for (uint16_t i = 0; i < bitmap_range; i++) {
            uint8_t byte_idx = i / 8;
            uint8_t bit_idx = i % 8;

            // 0 in bitmap means received, need to acknowledge
            if ((sack_bitmap[byte_idx] & (1 << bit_idx)) == 0) {
                if (!fragment_send_is_acked(send_state, i)) {
                    fragment_send_mark_acked(session, send_state, i);
                    acked_count++;
                }
            }
        }
    } else {
        // No SACK bitmap: acknowledge all fragments <= ack_num (traditional ACK)
        for (uint16_t i = 0; i <= ack_num && i < send_state->total_fragments; i++) {
            if (!fragment_send_is_acked(send_state, i)) {
                fragment_send_mark_acked(session, send_state, i);
                acked_count++;
            }
        }
    }

    // 2. Process SACK bitmap: retransmit lost fragments
    // Bitmap only covers range from 0 to ack_num (maximum sequence number)
    uint16_t retransmit_count = 0;
    if (sack_bitmap) {
        // Bitmap range: 0 to ack_num
        uint16_t bitmap_range = ack_num + 1;
        if (bitmap_range > send_state->total_fragments) {
            bitmap_range = send_state->total_fragments;
        }

        for (uint16_t i = 0; i < bitmap_range; i++) {
            uint8_t byte_idx = i / 8;
            uint8_t bit_idx = i % 8;

            // In the bitmap, 1 means lost, needs retransmission
            if ((sack_bitmap[byte_idx] & (1 << bit_idx)) != 0) {
                // Check if already acknowledged
                if (!fragment_send_is_acked(send_state, i)) {
                    // Check retransmission count
                    if (send_state->fragment_retry_count && send_state->fragment_retry_count[i] >= send_state->max_retries) {
                        // Retry limit reached — abort the entire transfer immediately.
                        // Continuing to skip this fragment would leave the transfer stuck
                        // forever; aborting now releases the slot and notifies the caller.
                        ESP_LOGE(TAG, "Fragment %d exceeded max retries (%d) via SACK, aborting transfer (stream=%d)",
                                 i, send_state->max_retries, send_state->stream_id);
                        flux_send_state_abort(send_state, ESP_ERR_TIMEOUT);
                        return ESP_FAIL;
                    }

                    // Retransmit lost fragment
                    // Calculate offset and data size: first fragment uses START packet, subsequent fragments use normal fragment packets
                    uint32_t offset;
                    uint16_t fragment_data_size;
                    flux_calc_fragment_offset(session, i, send_state->total_size, &offset, &fragment_data_size);

                    esp_err_t err = flux_send_fragment_packet(session, send_state, i, send_state->source_data + offset, fragment_data_size);
                    if (err == ESP_OK) {
                        // Update retransmission tracking
                        if (send_state->fragment_send_time) {
                            send_state->fragment_send_time[i] = current_time;
                        }
                        if (send_state->fragment_retry_count) {
                            send_state->fragment_retry_count[i]++;
                        }
                        retransmit_count++;
                        ESP_LOGD(TAG, "Retransmitting fragment %d (retry %d)",
                                 i, send_state->fragment_retry_count ? send_state->fragment_retry_count[i] : 0);
                    }
                }
            }
        }
    }

    if (acked_count > 0 || retransmit_count > 0) {
        ESP_LOGI(TAG, "SACK received: ack_num=%d, confirmed %d fragments, retransmitted %d fragments, progress: %d/%d",
                 ack_num, acked_count, retransmit_count, send_state->sent_fragments, send_state->total_fragments);

    }

    // 3. Continue sending new fragments
    return flux_send_continue(session, send_state);
}

// Free and zero a single receive slot (internal; does not touch other slots)
static void flux_recv_state_reset(flux_fragment_recv_state_t *recv_state)
{
    if (!recv_state) {
        return;
    }
    if (recv_state->reassembly_buffer) {
        free(recv_state->reassembly_buffer);
        recv_state->reassembly_buffer = NULL;
        recv_state->reassembly_buffer_size = 0;
    }
    memset(recv_state, 0, sizeof(*recv_state));
}

// Free a send slot's internal tracking arrays and zero the slot.
// Caller-owned source_data is never freed here.
static void flux_send_state_clear(flux_fragment_send_state_t *send_state)
{
    if (!send_state) {
        return;
    }
    if (send_state->fragment_acked) {
        free(send_state->fragment_acked);
        send_state->fragment_acked = NULL;
    }
    if (send_state->fragment_send_time) {
        free(send_state->fragment_send_time);
        send_state->fragment_send_time = NULL;
    }
    if (send_state->fragment_retry_count) {
        free(send_state->fragment_retry_count);
        send_state->fragment_retry_count = NULL;
    }
    memset(send_state, 0, sizeof(*send_state));
}

/* ────────────────────── Handle Received Fragments ────────────────────── */
static esp_err_t flux_fragment_recv_handle(flux_session_t *session, flux_fragment_recv_state_t *recv_state,
                                           void *packet_ptr, uint16_t packet_len, uint8_t **owned_buf,
                                           uint32_t *owned_size, uint8_t *owned_stream_id,
                                           uint32_t *progress_transferred, uint32_t *progress_total)
{
    if (!session || !recv_state || !packet_ptr || packet_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (owned_buf) {
        *owned_buf = NULL;
    }
    if (owned_size) {
        *owned_size = 0;
    }
    if (owned_stream_id) {
        *owned_stream_id = FLUX_STREAM_ID_INVALID;
    }
    if (progress_transferred) {
        *progress_transferred = 0;
    }
    if (progress_total) {
        *progress_total = 0;
    }

    // Determine which structure to use based on packet type
    uint8_t packet_type = ((uint8_t *)packet_ptr)[0] & FLUX_PACKET_TYPE_MASK;
    uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds

    // Process fragment start packet
    if (packet_type == FLUX_PACKET_TYPE_START) {
        const flux_start_packet_t *packet = (const flux_start_packet_t *)packet_ptr;
        if (packet_len < SIZE_OF_START_WO_DATA) {
            ESP_LOGW(TAG, "START packet too short: len=%u", packet_len);
            return ESP_ERR_INVALID_SIZE;
        }
        uint16_t payload_len = packet_len - SIZE_OF_START_WO_DATA;
        if (packet->size > payload_len) {
            ESP_LOGW(TAG, "START packet size invalid: declared=%u actual=%u", packet->size, payload_len);
            return ESP_ERR_INVALID_SIZE;
        }
        ESP_LOGI(TAG, "Fragment session started (stream=%d)", packet->stream_id);
        // If reassembly already in progress for this stream, clean up first (restart same stream)
        uint8_t stream_id = packet->stream_id;
        if (recv_state->recv_active) {
            ESP_LOGW(TAG, "New fragment session started, resetting previous one (stream=%d)", stream_id);
            flux_recv_state_reset(recv_state);
        }

        // Initialize new fragment reassembly
        recv_state->recv_active = true;
        recv_state->stream_id = stream_id;
        recv_state->total_fragments = packet->total_fragments;
        recv_state->total_size = packet->total_size;
        if (recv_state->total_fragments == 0 || recv_state->total_fragments > FLUX_MAX_FRAGMENTS ||
                recv_state->total_size == 0 || packet->size > recv_state->total_size) {
            ESP_LOGW(TAG, "Invalid START fields: total_fragments=%u total_size=%" PRIu32 " size=%u",
                     recv_state->total_fragments, recv_state->total_size, packet->size);
            flux_recv_state_reset(recv_state);
            return ESP_ERR_INVALID_SIZE;
        }
        recv_state->received_fragments = 0;
        recv_state->received_size = 0;
        recv_state->start_time = current_time;
        recv_state->last_fragment_time = current_time;
        // Initialize ACK related fields.
        // UINT16_MAX is the sentinel meaning "no continuous prefix yet" (fragment 0 not received).
        recv_state->last_continuous_seq = UINT16_MAX;
        recv_state->last_ack_time = current_time;
        recv_state->last_continuous_time = current_time;
        recv_state->pending_ack = false;
        recv_state->window_size = packet->window_size;  // Save window size
        // Read window occupancy threshold from START packet structure field
        recv_state->window_threshold_percent = packet->window_threshold_percent;
        // Validate threshold range: 0-100, default 50
        if (recv_state->window_threshold_percent > 100) {
            recv_state->window_threshold_percent = 50;  // Default value
        }
        if (recv_state->window_threshold_percent == 0) {
            recv_state->window_threshold_percent = 50;  // Default value
        }
        recv_state->last_ack_missing_count = 0;  // Initialize lost packet count
        recv_state->acked_fragments = 0;  // Initialize acknowledged fragment count
        recv_state->first_fragment_size = 0;  // Initialize first fragment size (will be set when processing START packet data)
        // Allocate reassembly buffer
        if (recv_state->total_size > FLUX_MAX_REASSEMBLY_SIZE) {
            ESP_LOGE(TAG, "Fragment size too large: %" PRIu32 " > %d", recv_state->total_size, FLUX_MAX_REASSEMBLY_SIZE);
            flux_recv_state_reset(recv_state);
            return ESP_ERR_INVALID_SIZE;
        }

        recv_state->reassembly_buffer = calloc(1, recv_state->total_size);
        if (!recv_state->reassembly_buffer) {
            ESP_LOGE(TAG, "Failed to allocate reassembly buffer: %lu bytes", recv_state->total_size);
            flux_recv_state_reset(recv_state);
            return ESP_ERR_NO_MEM;
        }
        recv_state->reassembly_buffer_size = recv_state->total_size;

        // Zero bitmap
        memset(recv_state->fragment_bitmap, 0, sizeof(recv_state->fragment_bitmap));

        uint16_t fragment_idx = 0;
        ESP_LOGI(TAG, "Fragment session %d/%d started: total_size=%" PRIu32,
                 fragment_idx + 1, recv_state->total_fragments, recv_state->total_size);

        // Start packet may also contain data (first fragment)
        if (packet->size > 0) {
            // Save first fragment size (for calculating offset of subsequent fragments)
            recv_state->first_fragment_size = packet->size;

            if (fragment_idx < recv_state->total_fragments && packet->size <= recv_state->total_size) {
                uint32_t offset = 0;  // First fragment offset is always 0
                // Copy data directly
                memcpy(recv_state->reassembly_buffer + offset, packet->data, packet->size);

                if (!fragment_is_received(recv_state, fragment_idx)) {
                    fragment_mark_received(recv_state, fragment_idx);
                    recv_state->received_fragments++;
                    recv_state->received_size += packet->size;
                    // First fragment, update continuous sequence number
                    recv_state->last_continuous_seq = 0;
                    recv_state->last_continuous_time = current_time;
                    recv_state->pending_ack = false;  // Don't send ACK immediately, wait for packet loss detection or reception completion
                }
            }
        }

        // When total_fragments == 1 the START packet carries all the data — check completion here
        // because the FRAGMENT path (which normally fires the callback) will never be reached.
        if (recv_state->received_fragments >= recv_state->total_fragments) {
            session->last_completed_recv_valid = true;
            session->last_completed_recv_stream_id = recv_state->stream_id;
            ESP_LOGW(TAG, "Single-fragment transfer complete (stream=%d): %" PRIu32 " bytes",
                     recv_state->stream_id, recv_state->received_size);

            uint16_t max_seq = fragment_get_max_received_seq(recv_state);
            esp_err_t err = flux_send_sack(session, recv_state, max_seq, true);
            if (err == ESP_OK) {
                recv_state->last_ack_time = current_time;
                recv_state->last_ack_missing_count = 0;
                recv_state->pending_ack = false;
            }

            if (session->callbacks.data_received_cb) {
                uint8_t *cb_data = recv_state->reassembly_buffer;
                uint32_t cb_size = recv_state->received_size;
                uint8_t  sid     = recv_state->stream_id;
                recv_state->reassembly_buffer      = NULL;
                recv_state->reassembly_buffer_size = 0;
                flux_recv_state_reset(recv_state);
                if (owned_buf) {
                    *owned_buf = cb_data;
                } else {
                    free(cb_data);
                }
                if (owned_size) {
                    *owned_size = cb_size;
                }
                if (owned_stream_id) {
                    *owned_stream_id = sid;
                }
            } else {
                ESP_LOGW(TAG, "Dropping received stream=%d because data_received_cb is NULL", recv_state->stream_id);
                flux_recv_state_reset(recv_state);
            }
        } else {
            if (owned_stream_id) {
                *owned_stream_id = recv_state->stream_id;
            }
            if (progress_transferred) {
                *progress_transferred = recv_state->received_size;
            }
            if (progress_total) {
                *progress_total = recv_state->total_size;
            }
        }

        return ESP_OK;
    }

    // Process normal fragment packet
    if (packet_type == FLUX_PACKET_TYPE_FRAGMENT) {
        const flux_fragment_packet_t *packet = (const flux_fragment_packet_t *)packet_ptr;
        if (packet_len < SIZE_OF_FRAGMENT_WO_DATA) {
            ESP_LOGW(TAG, "FRAGMENT packet too short: len=%u", packet_len);
            return ESP_ERR_INVALID_SIZE;
        }
        uint16_t payload_len = packet_len - SIZE_OF_FRAGMENT_WO_DATA;
        if (packet->size > payload_len) {
            ESP_LOGW(TAG, "FRAGMENT packet size invalid: declared=%u actual=%u", packet->size, payload_len);
            return ESP_ERR_INVALID_SIZE;
        }
        if (!recv_state->recv_active) {
            ESP_LOGW(TAG, "Received fragment without start packet, ignoring");
            return ESP_ERR_INVALID_STATE;
        }
        if (!recv_state->reassembly_buffer) {
            ESP_LOGW(TAG, "Receive slot missing buffer, resetting stream=%d", recv_state->stream_id);
            flux_recv_state_reset(recv_state);
            return ESP_ERR_INVALID_STATE;
        }

        // Check timeout
        if (current_time > recv_state->last_fragment_time && (current_time - recv_state->last_fragment_time) > FLUX_FRAGMENT_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Fragment receive timeout, resetting");
            flux_recv_state_reset(recv_state);
            return ESP_ERR_TIMEOUT;
        }

        uint16_t fragment_idx = packet->seq_num;

        // Validate fragment index
        if (fragment_idx >= recv_state->total_fragments) {
            ESP_LOGW(TAG, "Fragment index out of range: %d >= %d (total_size=%" PRIu32 "), ignoring - may be from different session",
                     fragment_idx, recv_state->total_fragments, recv_state->total_size);
            return ESP_OK; // Ignore packets not belonging to current session
        }

        // Calculate data offset: first fragment uses START packet, subsequent fragments use normal fragment packets
        uint32_t offset;
        if (fragment_idx == 0) {
            offset = 0;  // First fragment offset is always 0
        } else {
            // Subsequent fragment offset = first fragment size + (fragment_idx - 1) * maximum data size of normal fragment packet
            offset = recv_state->first_fragment_size + (fragment_idx - 1) * session->max_fragment_data_size;
        }

        // Validate if offset is within reasonable range
        if (offset >= recv_state->total_size) {
            ESP_LOGW(TAG, "Fragment offset out of range: offset=%" PRIu32 " >= total_size=%" PRIu32 ", ignoring - may be from different session",
                     offset, recv_state->total_size);
            return ESP_OK; // Ignore packets not belonging to current session
        }

        // Last fragment may be smaller than fragment_data_size
        uint32_t remaining = recv_state->total_size - offset;
        uint32_t copy_size = (packet->size < remaining) ? packet->size : remaining;

        if (offset + copy_size > recv_state->total_size) {
            ESP_LOGW(TAG, "Fragment data overflow: offset=%" PRIu32 ", size=%" PRIu32 ", total=%" PRIu32 ", ignoring - may be from different session",
                     offset, copy_size, recv_state->total_size);
            return ESP_OK; // Ignore packets not belonging to current session
        }

        // Check if duplicate reception
        if (fragment_is_received(recv_state, fragment_idx)) {
            ESP_LOGD(TAG, "Fragment %d already received, ignoring", fragment_idx);
            return ESP_OK;
        }

        // Copy data to reassembly buffer
        memcpy(recv_state->reassembly_buffer + offset, packet->data, copy_size);

        // Mark fragment as received
        fragment_mark_received(recv_state, fragment_idx);
        recv_state->received_fragments++;
        recv_state->received_size += copy_size;
        recv_state->last_fragment_time = current_time;

        // Update continuous sequence number after marking this fragment received.
        // last_continuous_seq == UINT16_MAX is the sentinel meaning "no continuous prefix yet".
        if (recv_state->last_continuous_seq == UINT16_MAX) {
            // No continuous prefix established yet. Recalculate from scratch — fragment 0
            // may have just arrived (either this packet or an earlier one that was out of order).
            uint16_t new_continuous = fragment_get_last_continuous_seq(recv_state);
            if (new_continuous != UINT16_MAX) {
                recv_state->last_continuous_seq = new_continuous;
                recv_state->last_continuous_time = current_time;
            }
        } else {
            uint16_t expected_next = recv_state->last_continuous_seq + 1;
            if (fragment_idx == expected_next) {
                // Received the next in-order fragment — advance the left edge directly.
                recv_state->last_continuous_seq = fragment_idx;
                recv_state->last_continuous_time = current_time;
            } else if (fragment_idx < expected_next) {
                // fragment_idx <= last_continuous_seq: late duplicate or gap-filling arrival.
                // Recalculate in case this fragment completes a previously-broken run.
                uint16_t new_continuous = fragment_get_last_continuous_seq(recv_state);
                if (new_continuous != UINT16_MAX && new_continuous > recv_state->last_continuous_seq) {
                    recv_state->last_continuous_seq = new_continuous;
                    recv_state->last_continuous_time = current_time;
                }
            }
            // fragment_idx > expected_next: out-of-order arrival, continuous prefix unchanged.
        }

        ESP_LOGI(TAG, "Fragment %d/%d received: %" PRIu32 "/%" PRIu32 " bytes (%.1f%%), last_continuous=%d",
                 fragment_idx + 1, recv_state->total_fragments, recv_state->received_size, recv_state->total_size,
                 (recv_state->received_size * 100.0f) / recv_state->total_size,
                 recv_state->last_continuous_seq == UINT16_MAX ? -1 : (int)recv_state->last_continuous_seq);

        // Check if packet loss: only send ACK when packet loss recv_state changes and time interval is satisfied
        uint16_t max_seq = fragment_get_max_received_seq(recv_state);
        uint16_t missing_count = 0;
        for (uint16_t i = 0; i <= max_seq && i < recv_state->total_fragments; i++) {
            if (!fragment_is_received(recv_state, i)) {
                missing_count++;
            }
        }

        // Check window occupancy: if window occupancy reaches window_threshold_percent, send SACK immediately
        bool window_percentage_trigger = false;
        if (recv_state->window_size != 0xFF && recv_state->window_size > 0) {
            // Calculate window occupancy: number of received but unacknowledged fragments / window size
            // Number of unacknowledged fragments = number of received fragments - number of acknowledged fragments
            uint16_t unacked_fragments = 0;
            if (recv_state->received_fragments > recv_state->acked_fragments) {
                unacked_fragments = recv_state->received_fragments - recv_state->acked_fragments;
            }
            uint16_t window_usage = (unacked_fragments * 100) / recv_state->window_size;
            if (window_usage >= recv_state->window_threshold_percent) {
                window_percentage_trigger = true;
                ESP_LOGD(TAG, "Window usage >= %d%%: received_fragments=%d, acked_fragments=%d, unacked=%d, window_size=%d, usage=%d%%",
                         recv_state->window_threshold_percent, recv_state->received_fragments, recv_state->acked_fragments, unacked_fragments, recv_state->window_size, window_usage);
            }
        }

        // Condition 1: packet loss exists
        // Condition 2: packet loss count changes (avoid sending duplicate ACKs)
        // Condition 3: at least 50ms since last ACK (avoid ACK storm)
        // Condition 4: window occupancy reaches window_threshold_percent (send immediately, regardless of time interval)
        uint32_t time_since_last_ack = current_time - recv_state->last_ack_time;
        bool missing_changed = (missing_count != recv_state->last_ack_missing_count);
        bool interval_ok = (time_since_last_ack >= FLUX_ACK_MIN_INTERVAL_MS);

        // Send SACK immediately when window occupancy reaches window_threshold_percent (regardless of time interval)
        if (window_percentage_trigger) {
            esp_err_t err = flux_send_sack(session, recv_state, max_seq, true);
            if (err == ESP_OK) {
                recv_state->last_ack_time = current_time;
                recv_state->last_ack_missing_count = missing_count;
                ESP_LOGD(TAG, "SACK sent immediately (window >= window_threshold_percent%%): max_seq=%d, missing_count=%d",
                         max_seq, missing_count);
            }
        } else if (missing_count > 0 && missing_changed && interval_ok) {
            esp_err_t err = flux_send_sack(session, recv_state, max_seq, true);
            if (err == ESP_OK) {
                uint16_t prev_missing_count = recv_state->last_ack_missing_count;
                recv_state->last_ack_time = current_time;
                recv_state->last_ack_missing_count = missing_count;
                ESP_LOGD(TAG, "SACK sent: max_seq=%d, missing_count=%d (changed from %d)",
                         max_seq, missing_count, prev_missing_count);
            }
        } else if (missing_count > 0) {
            ESP_LOGD(TAG, "SACK skipped: missing_count=%d, changed=%d, interval=%" PRIu32 " ms",
                     missing_count, missing_changed, time_since_last_ack);
        }

        // Check if all fragments have been received
        if (recv_state->received_fragments >= recv_state->total_fragments) {
            session->last_completed_recv_valid = true;
            session->last_completed_recv_stream_id = recv_state->stream_id;
            ESP_LOGW(TAG, "All fragments received: %d/%d, total size: %" PRIu32 " bytes",
                     recv_state->received_fragments, recv_state->total_fragments, recv_state->received_size);

            // All fragments received, send final ACK immediately (acknowledging all fragments, including SACK info)
            // Unconditionally send final ACK to ensure sender knows all data has been successfully received
            // Use max sequence number (all fragments received, max sequence number is the last fragment index)
            uint16_t max_seq = fragment_get_max_received_seq(recv_state);
            esp_err_t err = flux_send_sack(session, recv_state, max_seq, true);
            if (err == ESP_OK) {
                recv_state->last_ack_time = current_time;
                recv_state->last_ack_missing_count = 0;  // All fragments received, packet loss count is 0
                recv_state->pending_ack = false;
                ESP_LOGI(TAG, "Final SACK sent immediately: max_seq=%d (all %d fragments confirmed)",
                         max_seq, recv_state->total_fragments);
            } else {
                ESP_LOGW(TAG, "Failed to send final SACK: %d", err);
            }

            // Transfer buffer ownership to the application before resetting the slot.
            // NULL out reassembly_buffer first so flux_recv_state_reset does not free it —
            // the callback recipient is now responsible for calling free(data).
            if (session->callbacks.data_received_cb) {
                uint8_t *cb_data = recv_state->reassembly_buffer;
                uint32_t cb_size = recv_state->received_size;
                uint8_t  stream_id = recv_state->stream_id;
                recv_state->reassembly_buffer      = NULL;
                recv_state->reassembly_buffer_size = 0;
                flux_recv_state_reset(recv_state);
                if (owned_buf) {
                    *owned_buf = cb_data;
                } else {
                    free(cb_data);
                }
                if (owned_size) {
                    *owned_size = cb_size;
                }
                if (owned_stream_id) {
                    *owned_stream_id = stream_id;
                }
            } else {
                ESP_LOGW(TAG, "Dropping received stream=%d because data_received_cb is NULL", recv_state->stream_id);
                flux_recv_state_reset(recv_state);
            }
        } else {
            if (owned_stream_id) {
                *owned_stream_id = recv_state->stream_id;
            }
            if (progress_transferred) {
                *progress_transferred = recv_state->received_size;
            }
            if (progress_total) {
                *progress_total = recv_state->total_size;
            }
        }

        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

void flux_transmit_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Flux session transmit task running");
    flux_session_queue_msg_t msg = {0};

    while (true) {
        // 1. Process receive queue
        // 1. Based on received packet type: divide into ACK, single packet, fragment data
        // 1. Process ACK packet: update flow control window, mark packet as acknowledged
        // 2. Process single packet data: directly pass to upper layer callback
        // 3. Process fragment data: reassemble, after reassembly complete pass to upper layer callback
        // 2. Send data packets based on current window
        memset(&msg, 0, sizeof(msg));
        if (xQueueReceive(g_flux_session_queue, &msg, pdMS_TO_TICKS(10)) == pdPASS) {
            // Find corresponding session by conn_handle
            flux_session_t *session = msg.session;
            uint16_t packet_len = msg.packet_len;
            uint8_t *cb_data = NULL;
            uint32_t cb_size = 0;
            uint8_t cb_stream_id = FLUX_STREAM_ID_INVALID;
            uint32_t cb_progress_transferred = 0;
            uint32_t cb_progress_total = 0;
            if (!session) {
                ESP_LOGW(TAG, "No session found for session=%p, dropping packet", session);
                if (msg.fragment_packet) {
                    free(msg.fragment_packet);
                }
                continue;
            }

            if (!msg.fragment_packet || packet_len < 1) {
                ESP_LOGW(TAG, "Received NULL packet for session=%p", session);
                if (msg.fragment_packet) {
                    free(msg.fragment_packet);
                }
                continue;
            }

            if (g_flux_sessions_mutex) {
                xSemaphoreTake(g_flux_sessions_mutex, portMAX_DELAY);
            }
            if (!flux_session_is_active(session)) {
                if (g_flux_sessions_mutex) {
                    xSemaphoreGive(g_flux_sessions_mutex);
                }
                free(msg.fragment_packet);
                continue;
            }
            flux_session_state_lock(session);
            if (g_flux_sessions_mutex) {
                xSemaphoreGive(g_flux_sessions_mutex);
            }

            // Process based on packet type: read packet type first (first byte of all packet types is packet_head)
            uint8_t packet_type = ((uint8_t *)msg.fragment_packet)[0] & FLUX_PACKET_TYPE_MASK;

            switch (packet_type) {
            case FLUX_PACKET_TYPE_SINGLE: {
                flux_fragment_packet_t *single_packet = (flux_fragment_packet_t *)msg.fragment_packet;
                if (packet_len < SIZE_OF_FRAGMENT_WO_DATA) {
                    ESP_LOGW(TAG, "SINGLE packet too short: len=%u", packet_len);
                    free(msg.fragment_packet);
                    break;
                }
                uint16_t payload_len = packet_len - SIZE_OF_FRAGMENT_WO_DATA;
                if (single_packet->size > payload_len) {
                    ESP_LOGW(TAG, "SINGLE packet size invalid: declared=%u actual=%u", single_packet->size, payload_len);
                    free(msg.fragment_packet);
                    break;
                }
                // Single packet data, callback directly (carries its own stream_id from the header nibble)
                if (session->callbacks.data_received_cb) {
                    uint8_t *owned_buf = malloc(single_packet->size);
                    if (!owned_buf) {
                        ESP_LOGE(TAG, "Failed to allocate copy buffer for SINGLE packet");
                    } else {
                        memcpy(owned_buf, single_packet->data, single_packet->size);
                        cb_data = owned_buf;
                        cb_size = single_packet->size;
                        cb_stream_id = single_packet->stream_id;
                    }
                }
                free(msg.fragment_packet);
                break;
            }
            case FLUX_PACKET_TYPE_START: {
                flux_start_packet_t *start_packet = (flux_start_packet_t *)msg.fragment_packet;
                if (packet_len < SIZE_OF_START_WO_DATA) {
                    ESP_LOGW(TAG, "START packet too short: len=%u", packet_len);
                    free(msg.fragment_packet);
                    break;
                }
                // START packet: resolve (or allocate) the receive slot for this stream
                flux_fragment_recv_state_t *recv_state = flux_session_recv_find_or_alloc(session, start_packet->stream_id);
                if (!recv_state) {
                    ESP_LOGW(TAG, "No free receive slot for stream=%d, dropping START", start_packet->stream_id);
                } else {
                    session->last_completed_recv_valid = false;
                    esp_err_t err = flux_fragment_recv_handle(session, recv_state, start_packet, packet_len,
                                                              &cb_data, &cb_size, &cb_stream_id,
                                                              &cb_progress_transferred, &cb_progress_total);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to process START fragment (stream=%d): %d", start_packet->stream_id, err);
                    }
                }
                free(msg.fragment_packet);
                break;
            }
            case FLUX_PACKET_TYPE_FRAGMENT: {
                flux_fragment_packet_t *frag_packet = (flux_fragment_packet_t *)msg.fragment_packet;
                if (packet_len < SIZE_OF_FRAGMENT_WO_DATA) {
                    ESP_LOGW(TAG, "FRAGMENT packet too short: len=%u", packet_len);
                    free(msg.fragment_packet);
                    break;
                }
                // Fragment data packet: route to the receive slot reassembling this stream
                flux_fragment_recv_state_t *recv_state = flux_session_recv_find(session, frag_packet->stream_id);
                if (!recv_state) {
                    ESP_LOGW(TAG, "Fragment for unknown/inactive stream=%d, ignoring", frag_packet->stream_id);
                } else {
                    esp_err_t err = flux_fragment_recv_handle(session, recv_state, frag_packet, packet_len,
                                                              &cb_data, &cb_size, &cb_stream_id,
                                                              &cb_progress_transferred, &cb_progress_total);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to process fragment (stream=%d): %d", frag_packet->stream_id, err);
                    }
                }
                free(msg.fragment_packet);
                break;
            }
            case FLUX_PACKET_TYPE_SACK: {
                flux_ack_packet_t *ack_packet = (flux_ack_packet_t *)msg.fragment_packet;
                if (packet_len < SIZE_OF_ACK_WO_DATA) {
                    ESP_LOGW(TAG, "SACK packet too short: len=%u", packet_len);
                    free(msg.fragment_packet);
                    break;
                }
                uint16_t payload_len = packet_len - SIZE_OF_ACK_WO_DATA;
                if (ack_packet->size > payload_len) {
                    ESP_LOGW(TAG, "SACK packet size invalid: declared=%u actual=%u", ack_packet->size, payload_len);
                    free(msg.fragment_packet);
                    break;
                }
                // SACK packet: route to the send slot for this stream
                flux_fragment_send_state_t *send_state = flux_session_send_find(session, ack_packet->stream_id);
                if (send_state) {
                    uint8_t *sack_bitmap = NULL;
                    if (ack_packet->size >= FLUX_SACK_BITMAP_SIZE) {
                        sack_bitmap = ack_packet->data;  // SACK bitmap
                        ESP_LOGI(TAG, "SACK received: stream=%d, ack_num=%d, sack_size=%d", ack_packet->stream_id, ack_packet->ack_num, ack_packet->size);
                    } else {
                        ESP_LOGI(TAG, "ACK(SACK don't include bitmap) received: stream=%d, ack_num=%d", ack_packet->stream_id, ack_packet->ack_num);
                    }

                    flux_handle_sack(session, send_state, ack_packet->ack_num, sack_bitmap);
                } else {
                    // Send completed or unknown stream, ignore duplicate SACK
                    ESP_LOGD(TAG, "SACK received but no active send for stream=%d: ack_num=%d (ignored)", ack_packet->stream_id, ack_packet->ack_num);
                }
                free(msg.fragment_packet);
                break;
            }

            default:
                ESP_LOGW(TAG, "Unknown packet type: 0x%02X", packet_type);
                free(msg.fragment_packet);
                break;
            }
            bool reserve_data_cb = false;
            bool reserve_progress_cb = false;
            if (cb_data && session->callbacks.data_received_cb && !session->destroying) {
                session->callback_pending++;
                reserve_data_cb = true;
            }
            if (cb_progress_total > 0 && session->callbacks.progress_cb && !session->destroying) {
                session->callback_pending++;
                reserve_progress_cb = true;
            }
            flux_session_state_unlock(session);

            if (cb_data) {
                if (reserve_data_cb && flux_session_callback_begin_reserved(session)) {
                    session->callbacks.data_received_cb(session, cb_stream_id, cb_data, cb_size);
                    flux_session_callback_leave(session);
                } else {
                    free(cb_data);
                }
            }

            if (reserve_progress_cb && flux_session_callback_begin_reserved(session)) {
                session->callbacks.progress_cb(session, cb_stream_id, cb_progress_transferred, cb_progress_total);
                flux_session_callback_leave(session);
            }
        }

        // If queue still has messages, continue processing immediately
        if (uxQueueMessagesWaiting(g_flux_session_queue) != 0) {
            continue;
        }

        // Check fragment receive and send timeout, and ACK send timing - iterate through all sessions
        typedef struct {
            flux_session_t *session;
            uint8_t stream_id;
            esp_err_t status;
            const uint8_t *data;
            uint32_t size;
        } flux_send_complete_event_t;
        typedef struct {
            flux_session_t *session;
            esp_err_t err;
        } flux_error_event_t;
        typedef struct {
            flux_session_t *session;
            uint8_t stream_id;
            uint32_t transferred;
            uint32_t total;
        } flux_progress_event_t;
        flux_send_complete_event_t send_events[16];
        flux_error_event_t error_events[16];
        flux_progress_event_t progress_events[32];
        int send_event_count = 0;
        int error_event_count = 0;
        int progress_event_count = 0;
        flux_session_t *session;
        uint32_t current_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        if (g_flux_sessions_mutex) {
            xSemaphoreTake(g_flux_sessions_mutex, portMAX_DELAY);
        }
        SLIST_FOREACH(session, &g_flux_sessions_list, next) {
            flux_session_state_lock(session);
            // Check receive timeout and ACK send timing across all receive slots
            for (int ri = 0; ri < FLUX_MAX_CONCURRENT_RECVS; ri++) {
                flux_fragment_recv_state_t *recv_state = &session->fragment_recv[ri];
                if (!recv_state->recv_active) {
                    continue;
                }

                // Check receive timeout
                if (current_time > recv_state->last_fragment_time && (current_time - recv_state->last_fragment_time) > FLUX_FRAGMENT_TIMEOUT_MS) {
                    // ESP_LOGW(TAG, "Fragment receive timeout for conn_handle=%d stream=%d, resetting", session->conn_handle, recv_state->stream_id);
                    flux_recv_state_reset(recv_state);

                    if (session->callbacks.error_cb &&
                            !session->destroying &&
                            error_event_count < (int)(sizeof(error_events) / sizeof(error_events[0]))) {
                        session->callback_pending++;
                        error_events[error_event_count].session = session;
                        error_events[error_event_count].err = ESP_ERR_TIMEOUT;
                        error_event_count++;
                    }
                    continue;
                }

                // Check ACK send timing: only send ACK when packet loss state changes and time interval is satisfied
                // Check if packet loss exists (in range from 0 to max_seq)
                uint16_t max_seq = fragment_get_max_received_seq(recv_state);
                uint16_t missing_count = 0;
                for (uint16_t i = 0; i <= max_seq && i < recv_state->total_fragments; i++) {
                    if (!fragment_is_received(recv_state, i)) {
                        missing_count++;
                    }
                }

                // Condition 1: packet loss exists
                // Condition 2: packet loss count changes (avoid sending duplicate ACKs)
                // Condition 3: at least 50ms since last ACK (avoid ACK storm)
                uint32_t time_since_last_ack = current_time - recv_state->last_ack_time;
                bool missing_changed = (missing_count != recv_state->last_ack_missing_count);
                bool interval_ok = (time_since_last_ack >= FLUX_ACK_MIN_INTERVAL_MS);

                if (missing_count > 0 && missing_changed && interval_ok) {
                    esp_err_t err = flux_send_sack(session, recv_state, max_seq, true);
                    if (err == ESP_OK) {
                        uint16_t prev_missing_count = recv_state->last_ack_missing_count;
                        recv_state->last_ack_time = current_time;
                        recv_state->last_ack_missing_count = missing_count;
                        recv_state->pending_ack = false;
                        ESP_LOGD(TAG, "SACK sent: stream=%d, max_seq=%d, missing_count=%d (changed from %d)",
                                 recv_state->stream_id, max_seq, missing_count, prev_missing_count);
                    }
                } else if (missing_count > 0) {
                    ESP_LOGD(TAG, "SACK skipped: missing_count=%d, changed=%d, interval=%" PRIu32 " ms",
                             missing_count, missing_changed, time_since_last_ack);
                }
            }

            // Drive each send slot: timeouts, proactive sending, and deferred completion callbacks
            for (int si = 0; si < FLUX_MAX_CONCURRENT_SENDS; si++) {
                flux_fragment_send_state_t *send_state = &session->fragment_send[si];

                // Fire deferred completion callback for a fully-acknowledged transfer.
                // Clear the slot BEFORE the callback so the callback may start a new transfer here.
                if (send_state->completed) {
                    const uint8_t *data = send_state->source_data;
                    uint32_t size = send_state->total_size;
                    esp_err_t status = send_state->complete_status;
                    uint8_t stream_id = send_state->stream_id;
                    flux_send_state_clear(send_state);
                    if (session->callbacks.session_complete_cb &&
                            !session->destroying &&
                            send_event_count < (int)(sizeof(send_events) / sizeof(send_events[0]))) {
                        session->callback_pending++;
                        send_events[send_event_count].session = session;
                        send_events[send_event_count].stream_id = stream_id;
                        send_events[send_event_count].status = status;
                        send_events[send_event_count].data = data;
                        send_events[send_event_count].size = size;
                        send_event_count++;
                    }
                    continue;
                }

                if (!send_state->send_active) {
                    continue;
                }

                if (session->callbacks.progress_cb &&
                        !session->destroying &&
                        send_state->total_size > 0 &&
                        progress_event_count < (int)(sizeof(progress_events) / sizeof(progress_events[0]))) {
                    session->callback_pending++;
                    progress_events[progress_event_count].session = session;
                    progress_events[progress_event_count].stream_id = send_state->stream_id;
                    progress_events[progress_event_count].transferred = send_state->sent_size;
                    progress_events[progress_event_count].total = send_state->total_size;
                    progress_event_count++;
                }

                // Check send timeout. Two independent conditions — either alone is sufficient:
                // 1. Absolute deadline: transfer has been running for more than
                //    FLUX_FRAGMENT_TIMEOUT_MS since start. This prevents a congested
                //    link from refreshing last_send_time indefinitely while never making
                //    progress — without this guard the timeout could never fire when the
                //    sender keeps retransmitting but no ACKs come back.
                bool absolute_timeout = (current_time > send_state->start_time &&
                                         (current_time - send_state->start_time) > FLUX_FRAGMENT_TIMEOUT_MS);
                if (absolute_timeout) {
                    ESP_LOGW(TAG, "Send timeout (stream=%d, absolute=%d): elapsed=%" PRIu32 " ms",
                             send_state->stream_id, absolute_timeout, current_time - send_state->start_time);
                    const uint8_t *data = send_state->source_data;
                    uint32_t size = send_state->total_size;
                    uint8_t stream_id = send_state->stream_id;
                    flux_send_state_clear(send_state);
                    if (session->callbacks.session_complete_cb &&
                            !session->destroying &&
                            send_event_count < (int)(sizeof(send_events) / sizeof(send_events[0]))) {
                        session->callback_pending++;
                        send_events[send_event_count].session = session;
                        send_events[send_event_count].stream_id = stream_id;
                        send_events[send_event_count].status = ESP_ERR_TIMEOUT;
                        send_events[send_event_count].data = data;
                        send_events[send_event_count].size = size;
                        send_event_count++;
                    } else if (session->callbacks.error_cb &&
                               !session->destroying &&
                               error_event_count < (int)(sizeof(error_events) / sizeof(error_events[0]))) {
                        session->callback_pending++;
                        error_events[error_event_count].session = session;
                        error_events[error_event_count].err = ESP_ERR_TIMEOUT;
                        error_event_count++;
                    }
                }
            }

            // Phase C: send_continue in circular stream_id order (oldest stream first)
            typedef struct {
                int slot_idx;
                uint8_t age;
            } send_slot_order_t;

            send_slot_order_t order[FLUX_MAX_CONCURRENT_SENDS];
            int order_count = 0;

            for (int si = 0; si < FLUX_MAX_CONCURRENT_SENDS; si++) {
                flux_fragment_send_state_t *send_state = &session->fragment_send[si];
                if (!send_state->send_active) {
                    continue;
                }
                order[order_count].slot_idx = si;
                order[order_count].age = flux_session_stream_circular_age(session, send_state->stream_id);
                order_count++;
            }

            for (int i = 0; i < order_count; i++) {
                for (int j = i + 1; j < order_count; j++) {
                    if (order[j].age > order[i].age) {
                        send_slot_order_t tmp = order[i];
                        order[i] = order[j];
                        order[j] = tmp;
                    }
                }
            }

            for (int oi = 0; oi < order_count; oi++) {
                flux_fragment_send_state_t *send_state = &session->fragment_send[order[oi].slot_idx];

                if (!send_state->send_active) {
                    continue;
                }

                if (flux_session_send_blocked_by_older(session, send_state)) {
                    continue;
                }

                // SACK mechanism: actively check and continue sending, don't rely on SACK trigger
                // gatt session fragment send continue will handle both new fragments and timeout retransmissions
                // Check if there are timed-out fragments needing retransmission
                bool need_retransmit = false;
                if (send_state->fragment_send_time && send_state->fragment_retry_count) {
                    // Only check fragments that have been sent but not yet acknowledged.
                    // Compute unacked_left inline: the first fragment not yet ACKed.
                    // Using sent_fragments as the loop start is wrong when ACKs arrive out
                    // of order (e.g. 0,2,3 ACKed but 1 missing → sent_fragments=3 skips 1).
                    for (uint16_t i = fragment_send_get_left_edge(send_state); i < send_state->current_fragment; i++) {
                        if (!fragment_send_is_acked(send_state, i)) {
                            uint32_t send_time = send_state->fragment_send_time[i];
                            if (send_time > 0 && current_time > send_time && (current_time - send_time) > send_state->retransmit_timeout_ms) {
                                // Check retransmission count
                                if (send_state->fragment_retry_count[i] < send_state->max_retries) {
                                    need_retransmit = true;
                                    break;
                                }
                            }
                        }
                    }
                }

                // Actively continue sending (will handle both new fragments and timeout retransmissions)
                if (send_state->current_fragment < send_state->total_fragments || need_retransmit) {
                    flux_send_continue(session, send_state);
                }
            }
            flux_session_state_unlock(session);
        }
        if (g_flux_sessions_mutex) {
            xSemaphoreGive(g_flux_sessions_mutex);
        }
        for (int i = 0; i < send_event_count; i++) {
            flux_send_complete_event_t *event = &send_events[i];
            if (event->session->callbacks.session_complete_cb &&
                    flux_session_callback_begin_reserved(event->session)) {
                event->session->callbacks.session_complete_cb(
                                             event->session, event->stream_id, event->status, event->data, event->size);
                flux_session_callback_leave(event->session);
            }
        }
        for (int i = 0; i < error_event_count; i++) {
            flux_error_event_t *event = &error_events[i];
            if (event->session->callbacks.error_cb &&
                    flux_session_callback_begin_reserved(event->session)) {
                event->session->callbacks.error_cb(event->session, event->err);
                flux_session_callback_leave(event->session);
            }
        }
        for (int i = 0; i < progress_event_count; i++) {
            flux_progress_event_t *event = &progress_events[i];
            if (event->session->callbacks.progress_cb &&
                    flux_session_callback_begin_reserved(event->session)) {
                event->session->callbacks.progress_cb(event->session, event->stream_id, event->transferred, event->total);
                flux_session_callback_leave(event->session);
            }
        }
    }
}

/* ────────────────────── Public: Feed Data ────────────────────── */
esp_err_t flux_session_feed_data(flux_session_t *session, const uint8_t *data, uint16_t data_size)
{
    if (!session || !data || data_size < 1) {
        return ESP_ERR_INVALID_ARG;
    }

    void *frag_packet = calloc(1, data_size);
    if (!frag_packet) {
        ESP_LOGE(TAG, "Failed to allocate fragment packet buffer");
        return ESP_ERR_NO_MEM;
    }
    memcpy(frag_packet, data, data_size);

    // Send packet to queue for asynchronous processing
    esp_err_t err = flux_session_queue_packet(session, frag_packet, data_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to queue packet: %d", err);
        free(frag_packet);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* ────────────────────── Public: Session Lifecycle ────────────────────── */
static void flux_update_mtu_sizes(flux_session_t *session)
{
    uint16_t min_mtu = SIZE_OF_START_WO_DATA + FLUX_ATT_HEADER_SIZE + 1; // Minimum MTU to fit at least 1 byte
    if (session->mtu_size < min_mtu) {
        ESP_LOGW(TAG, "MTU %d too small (min %d), deferring size calculation", session->mtu_size, min_mtu);
        session->max_fragment_data_size = 0;
        session->max_start_data_size = 0;
        return;
    }
    session->max_fragment_data_size = session->mtu_size - SIZE_OF_FRAGMENT_WO_DATA - FLUX_ATT_HEADER_SIZE;
    session->max_start_data_size = session->mtu_size - SIZE_OF_START_WO_DATA - FLUX_ATT_HEADER_SIZE;
}

flux_session_t *flux_session_create(const flux_transport_ops_t *ops, void *transport_ctx,
                                    const flux_callbacks_t *callbacks, uint16_t mtu)
{
    bool created_queue = false;
    bool created_task = false;
    if (!ops || !ops->send) {
        ESP_LOGE(TAG, "Transport ops with send() is required");
        return NULL;
    }

    // Create the sessions-list mutex on the first call (before any list access).
    if (g_flux_sessions_mutex == NULL) {
        g_flux_sessions_mutex = xSemaphoreCreateMutex();
        if (g_flux_sessions_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create sessions list mutex");
            return NULL;
        }
    }

    flux_session_t *session = calloc(1, sizeof(flux_session_t));
    if (!session) {
        ESP_LOGE(TAG, "Failed to allocate session");
        return NULL;
    }

    // Initialize concurrent fragment receive/send slots
    memset(session->fragment_recv, 0, sizeof(session->fragment_recv));
    memset(session->fragment_send, 0, sizeof(session->fragment_send));
    session->next_stream_id = 0;            // Rolling stream_id allocator
    session->id = g_session_id_counter++;
    session->ops = ops;
    session->transport_ctx = transport_ctx;
    session->mtu_size = mtu;
    session->state_mutex = xSemaphoreCreateMutex();
    if (!session->state_mutex) {
        ESP_LOGE(TAG, "Failed to create session state mutex");
        goto err_ret;
    }
    flux_update_mtu_sizes(session);

    if (g_flux_session_task_handle == NULL) {
        // Create queue, pass messages containing conn_handle and packet pointer
        g_flux_session_queue = xQueueCreate(10, sizeof(flux_session_queue_msg_t));
        if (g_flux_session_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create g_flux_session_queue");
            goto err_ret;
        }
        created_queue = true;

        xTaskCreate(flux_transmit_task, "flux session transmit", 4096, NULL, 8, &g_flux_session_task_handle);
        if (g_flux_session_task_handle == NULL) {
            ESP_LOGE(TAG, "Failed to create g_flux_session_task_handle");
            goto err_ret;
        }
        created_task = true;
    }

    if (callbacks) {
        memcpy(&session->callbacks, callbacks, sizeof(flux_callbacks_t));
    }

    // Add to global session list (insert at list head)
    xSemaphoreTake(g_flux_sessions_mutex, portMAX_DELAY);
    SLIST_INSERT_HEAD(&g_flux_sessions_list, session, next);
    xSemaphoreGive(g_flux_sessions_mutex);

    ESP_LOGI(TAG, "Session %" PRIu32 " created, mtu=%d, max_start=%d, max_frag=%d",
             session->id, mtu, session->max_start_data_size, session->max_fragment_data_size);
    return session;

err_ret:
    if (session && session->state_mutex) {
        vSemaphoreDelete(session->state_mutex);
        session->state_mutex = NULL;
    }
    if (created_task && g_flux_session_task_handle) {
        vTaskDelete(g_flux_session_task_handle);
        g_flux_session_task_handle = NULL;
    }
    if (created_queue && g_flux_session_queue) {
        vQueueDelete(g_flux_session_queue);
        g_flux_session_queue = NULL;
    }
    free(session);
    return NULL;
}

esp_err_t flux_session_destroy(flux_session_t *session)
{
    if (!session) {
        return ESP_ERR_INVALID_ARG;
    }

    // Destroy from session callback running on tx task would self-deadlock.
    flux_session_state_lock(session);
    if (xTaskGetCurrentTaskHandle() == g_flux_session_task_handle && session->callback_inflight > 0) {
        flux_session_state_unlock(session);
        return ESP_ERR_INVALID_STATE;
    }
    flux_session_state_unlock(session);

    // Remove from global session list
    flux_session_t *prev = NULL;
    flux_session_t *cur = SLIST_FIRST(&g_flux_sessions_list);
    bool found = false;
    bool last_session = false;
    typedef struct {
        uint8_t stream_id;
        const uint8_t *data;
        uint32_t size;
    } dropped_send_t;
    dropped_send_t dropped_sends[FLUX_MAX_CONCURRENT_SENDS] = {0};
    int dropped_send_count = 0;

    xSemaphoreTake(g_flux_sessions_mutex, portMAX_DELAY);
    flux_session_state_lock(session);
    session->destroying = true;

    // Iterate through the list to find the node to delete
    while (cur != NULL) {
        if (cur == session) {
            // Found the node to delete
            if (prev == NULL) {
                // Is the first element
                SLIST_REMOVE_HEAD(&g_flux_sessions_list, next);
            } else {
                // Not the first element
                SLIST_REMOVE_AFTER(prev, next);
            }
            found = true;
            break;
        }
        prev = cur;
        cur = SLIST_NEXT(cur, next);
    }

    // If session not in list, log warning (but doesn't affect destroy process)
    if (!found) {
        ESP_LOGW(TAG, "Session not found in list, may have been already removed");
    }

    last_session = SLIST_EMPTY(&g_flux_sessions_list);

    // Clean up all receive slots while session list is locked to avoid
    // racing with flux_transmit_task's list iteration and slot access.
    for (int i = 0; i < FLUX_MAX_CONCURRENT_RECVS; i++) {
        flux_recv_state_reset(&session->fragment_recv[i]);
    }
    // Clean up all send slots. NOTE: any in-progress transfer's source buffer is
    // caller-owned (zero-copy); report dropped transfers once the slot is no
    // longer accessible so adapters can release their source buffers.
    for (int i = 0; i < FLUX_MAX_CONCURRENT_SENDS; i++) {
        flux_fragment_send_state_t *send_state = &session->fragment_send[i];
        if ((send_state->send_active || send_state->completed) &&
                send_state->source_data &&
                dropped_send_count < (int)(sizeof(dropped_sends) / sizeof(dropped_sends[0]))) {
            dropped_sends[dropped_send_count].stream_id = send_state->stream_id;
            dropped_sends[dropped_send_count].data = send_state->source_data;
            dropped_sends[dropped_send_count].size = send_state->source_size;
            dropped_send_count++;
        }
        flux_send_state_clear(&session->fragment_send[i]);
    }

    // Remove any queued packets still pointing at this session before it is freed.
    if (g_flux_session_queue) {
        UBaseType_t queued = uxQueueMessagesWaiting(g_flux_session_queue);
        for (UBaseType_t i = 0; i < queued; i++) {
            flux_session_queue_msg_t msg = {0};
            if (xQueueReceive(g_flux_session_queue, &msg, 0) != pdPASS) {
                break;
            }
            if (msg.session == session) {
                free(msg.fragment_packet);
                continue;
            }
            if (xQueueSend(g_flux_session_queue, &msg, 0) != pdPASS) {
                free(msg.fragment_packet);
            }
        }
    }

    xSemaphoreGive(g_flux_sessions_mutex);
    flux_session_state_unlock(session);

    for (int i = 0; i < dropped_send_count; i++) {
        if (session->callbacks.session_complete_cb) {
            session->callbacks.session_complete_cb(session, dropped_sends[i].stream_id,
                                                   ESP_ERR_INVALID_STATE,
                                                   dropped_sends[i].data,
                                                   dropped_sends[i].size);
        }
    }

    while (true) {
        uint16_t callback_pending = 0;
        uint16_t callback_inflight = 0;
        flux_session_state_lock(session);
        callback_pending = session->callback_pending;
        callback_inflight = session->callback_inflight;
        flux_session_state_unlock(session);
        if (callback_pending == 0 && callback_inflight == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // Unregister GAP listener if no sessions left
    if (last_session) {
        if (g_flux_session_task_handle) {
            vTaskDelete(g_flux_session_task_handle);
            g_flux_session_task_handle = NULL;
        }

        if (g_flux_session_queue) {
            if (uxQueueMessagesWaiting(g_flux_session_queue) != 0) {
                flux_session_queue_msg_t msg;
                while (xQueueReceive(g_flux_session_queue, &msg, 0) == pdPASS) {
                    free(msg.fragment_packet);
                    msg.fragment_packet = NULL;
                    msg.session = NULL;
                }
            }
            vQueueDelete(g_flux_session_queue);
            g_flux_session_queue = NULL;
        }
    }

    ESP_LOGI(TAG, "Session %" PRIu32 " destroyed", session->id);
    if (session->state_mutex) {
        vSemaphoreDelete(session->state_mutex);
        session->state_mutex = NULL;
    }
    free(session);
    return ESP_OK;
}

esp_err_t flux_session_set_mtu(flux_session_t *session, uint16_t mtu)
{
    if (!session || mtu == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    flux_session_state_lock(session);
    session->mtu_size = mtu;
    flux_update_mtu_sizes(session);
    flux_session_state_unlock(session);
    return ESP_OK;
}

uint16_t flux_session_get_mtu(const flux_session_t *session)
{
    return session ? session->mtu_size : 0;
}

uint32_t flux_session_get_id(const flux_session_t *session)
{
    return session ? session->id : 0;
}

void *flux_session_get_user_arg(const flux_session_t *session)
{
    return session ? session->callbacks.arg : NULL;
}

size_t flux_session_max_message_bytes(const flux_session_t *session)
{
    if (!session || session->max_start_data_size == 0) {
        return 0;
    }
    size_t result = (size_t)session->max_start_data_size +
                    (size_t)(FLUX_MAX_FRAGMENTS - 1) * (size_t)session->max_fragment_data_size;
    /* Receiver rejects total_size > FLUX_MAX_REASSEMBLY_SIZE; never advertise more. */
    return (result > FLUX_MAX_REASSEMBLY_SIZE) ? FLUX_MAX_REASSEMBLY_SIZE : result;
}

/* ────────────────────── Public: Send ────────────────────── */
// Initialize a free send slot for a new fragmented transfer. Assumes args validated and slot free.
static esp_err_t flux_session_fragment_send_start(flux_session_t *session, flux_fragment_send_state_t *send_state,
                                                  uint8_t stream_id, const uint8_t *data, uint32_t size, uint8_t window_size, uint8_t window_threshold_percent)
{
    // size==0 is rejected upstream; no other size restriction here — small payloads
    // fit entirely in the START packet (total_fragments==1, zero FRAGMENT packets).

    // Calculate required number of fragments.
    // Fragment 0 is always a START packet carrying up to max_start_data_size bytes.
    // Any remainder is split across FRAGMENT packets of max_fragment_data_size bytes each.
    uint16_t total_fragments;
    if (size <= session->max_start_data_size) {
        // All data fits in the START packet — single-fragment reliable transfer.
        total_fragments = 1;
    } else {
        uint32_t remaining_size = size - session->max_start_data_size;
        uint16_t remaining_fragments = (remaining_size + session->max_fragment_data_size - 1) / session->max_fragment_data_size;
        total_fragments = 1 + remaining_fragments;
    }

    ESP_LOGI(TAG, "max_start_data_size=%d, max_fragment_data_size=%d, total_fragments=%d",
             session->max_start_data_size, session->max_fragment_data_size, total_fragments);

    if (total_fragments > FLUX_MAX_FRAGMENTS) {
        ESP_LOGE(TAG, "Data too large: requires %d fragments > %d", total_fragments, FLUX_MAX_FRAGMENTS);
        return ESP_ERR_INVALID_SIZE;
    }

    // Handle window size: 0xFF means no window limit, other values indicate window size
    if (window_size == 0xFF) {
        // No window limit, can send continuously
        // window_size remains 0xFF, no adjustment needed
    } else {
        // Limit window size
        if (window_size == 0) {
            window_size = 6;  // Default window size
        }
        if (window_size > total_fragments) {
            window_size = total_fragments;
        }
    }

    // Initialize send state
    memset(send_state, 0, sizeof(flux_fragment_send_state_t));

    send_state->send_active = true;
    send_state->stream_id = stream_id;
    send_state->total_fragments = total_fragments;
    send_state->total_size = size;
    send_state->source_data = data;
    send_state->source_size = size;
    send_state->window_size = window_size;
    send_state->window_threshold_percent = window_threshold_percent;  // Save window occupancy threshold
    send_state->current_fragment = 0;
    send_state->in_flight = 0;  // Initialize number of sent but unacknowledged fragments to 0
    send_state->start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    send_state->last_send_time = send_state->start_time;

    // Allocate acknowledgment state array
    send_state->fragment_acked = calloc(total_fragments, sizeof(bool));
    if (!send_state->fragment_acked) {
        ESP_LOGE(TAG, "Failed to allocate ack array");
        flux_send_state_clear(send_state);
        return ESP_ERR_NO_MEM;
    }

    // Allocate retransmission tracking array
    send_state->fragment_send_time = calloc(total_fragments, sizeof(uint32_t));
    send_state->fragment_retry_count = calloc(total_fragments, sizeof(uint8_t));
    if (!send_state->fragment_send_time || !send_state->fragment_retry_count) {
        ESP_LOGE(TAG, "Failed to allocate retransmit tracking arrays, reset send state");
        flux_send_state_clear(send_state);
        return ESP_ERR_NO_MEM;
    }

    // Initialize retransmission parameters
    send_state->retransmit_timeout_ms = FLUX_RETRANSMIT_TIMEOUT_MS;
    send_state->max_retries = FLUX_MAX_RETRANSMIT_RETRIES;

    ESP_LOGI(TAG, "Starting fragment send: stream=%d, total_size=%" PRIu32 ", fragments=%d, window_size=%d",
             send_state->stream_id, send_state->total_size, send_state->total_fragments, send_state->window_size);

    return ESP_OK;
}

esp_err_t flux_session_send(flux_session_t *session, const uint8_t *data,
                            uint32_t size, uint8_t window_size, uint8_t window_threshold_percent)
{
    if (!session || !data || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    flux_session_state_lock(session);
    if (session->max_start_data_size == 0 || session->max_fragment_data_size == 0) {
        ESP_LOGE(TAG, "Cannot send: MTU not set (call flux_session_set_mtu first)");
        flux_session_state_unlock(session);
        return ESP_ERR_INVALID_STATE;
    }

    // Validate window occupancy threshold: 0-100, default 50
    if (window_threshold_percent > 100 || window_threshold_percent == 0) {
        window_threshold_percent = 50;  // Default value
    }

    // All data — small or large — goes through the reliable fragment engine so that
    // every transfer gets SACK acknowledgment and retransmission.
    // Small payloads (size <= max_start_data_size) become a 1-fragment transfer
    // (START only, no FRAGMENT packets).  Medium payloads that fit in START+1
    // become a 2-fragment transfer, etc.

    // Allocate a free send slot; reject when both concurrent slots are busy (the "C" case).
    flux_fragment_send_state_t *send_state = flux_session_send_alloc(session);
    if (!send_state) {
        ESP_LOGW(TAG, "No free send slot (max %d concurrent transfers in flight)", FLUX_MAX_CONCURRENT_SENDS);
        flux_session_state_unlock(session);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t stream_id = flux_session_alloc_stream_id(session);
    esp_err_t err = flux_session_fragment_send_start(session, send_state, stream_id, data, size, window_size, window_threshold_percent);
    flux_session_state_unlock(session);
    return err;
}

/* ────────────────────── Public: State Queries ────────────────────── */
bool flux_session_send_idle(flux_session_t *session)
{
    if (!session) {
        return false;
    }
    flux_session_state_lock(session);
    // Idle (a new transfer can be started) if at least one send slot is free.
    if (flux_session_send_alloc(session) != NULL) {
        flux_session_state_unlock(session);
        return true;
    }
    flux_session_state_unlock(session);
    return false;
}

bool flux_session_send_is_complete(flux_session_t *session, uint8_t stream_id)
{
    if (!session) {
        return false;
    }
    flux_session_state_lock(session);
    for (int i = 0; i < FLUX_MAX_CONCURRENT_SENDS; i++) {
        flux_fragment_send_state_t *send_state = &session->fragment_send[i];
        if (send_state->stream_id != stream_id) {
            continue;
        }
        if (send_state->completed) {
            flux_session_state_unlock(session);
            return true;
        }
        if (send_state->send_active) {
            flux_session_state_unlock(session);
            return send_state->sent_fragments >= send_state->total_fragments;
        }
    }
    flux_session_state_unlock(session);
    return false;
}

uint8_t flux_session_send_get_progress(flux_session_t *session, uint8_t stream_id)
{
    if (!session) {
        return 0;
    }
    flux_session_state_lock(session);
    for (int i = 0; i < FLUX_MAX_CONCURRENT_SENDS; i++) {
        flux_fragment_send_state_t *send_state = &session->fragment_send[i];
        if (send_state->stream_id != stream_id) {
            continue;
        }
        if (send_state->total_size == 0) {
            flux_session_state_unlock(session);
            return 0;
        }
        if (send_state->completed) {
            flux_session_state_unlock(session);
            return 100;
        }
        if (send_state->send_active) {
            flux_session_state_unlock(session);
            return (uint8_t)((send_state->sent_size * 100) / send_state->total_size);
        }
    }
    flux_session_state_unlock(session);
    return 0;
}

esp_err_t flux_session_send_reset(flux_session_t *session, uint8_t stream_id)
{
    if (!session) {
        return ESP_ERR_INVALID_ARG;
    }

    flux_session_state_lock(session);
    flux_fragment_send_state_t *send_state = flux_session_send_find(session, stream_id);
    if (!send_state) {
        ESP_LOGW(TAG, "Send state not found (stream=%d)", stream_id);
        flux_session_state_unlock(session);
        return ESP_OK;
    }

    // Abort: clear protocol state only; source_data remains caller-owned.
    flux_send_state_clear(send_state);
    flux_session_state_unlock(session);
    ESP_LOGD(TAG, "Fragment send state reset (stream=%d)", stream_id);

    return ESP_OK;
}

bool flux_session_recv_idle(flux_session_t *session)
{
    if (!session) {
        return false;
    }
    flux_session_state_lock(session);
    // Idle only if no receive slot is active.
    for (int i = 0; i < FLUX_MAX_CONCURRENT_RECVS; i++) {
        if (session->fragment_recv[i].recv_active) {
            flux_session_state_unlock(session);
            return false;
        }
    }
    flux_session_state_unlock(session);
    return true;
}

bool flux_session_recv_is_complete(flux_session_t *session, uint8_t stream_id)
{
    if (!session) {
        return false;
    }
    flux_session_state_lock(session);
    flux_fragment_recv_state_t *recv_state = flux_session_recv_find(session, stream_id);
    if (!recv_state) {
        bool complete = session->last_completed_recv_valid && session->last_completed_recv_stream_id == stream_id;
        flux_session_state_unlock(session);
        return complete;
    }
    bool complete = (recv_state->received_fragments >= recv_state->total_fragments);
    flux_session_state_unlock(session);
    return complete;
}

uint8_t flux_session_recv_get_progress(flux_session_t *session, uint8_t stream_id)
{
    if (!session) {
        return 0;
    }
    flux_session_state_lock(session);
    flux_fragment_recv_state_t *recv_state = flux_session_recv_find(session, stream_id);
    if (!recv_state) {
        if (session->last_completed_recv_valid && session->last_completed_recv_stream_id == stream_id) {
            flux_session_state_unlock(session);
            return 100;
        }
        flux_session_state_unlock(session);
        return 0;
    }

    if (recv_state->total_size == 0) {
        flux_session_state_unlock(session);
        return 0;
    }
    uint8_t progress = (uint8_t)((recv_state->received_size * 100) / recv_state->total_size);
    flux_session_state_unlock(session);
    return progress;
}

esp_err_t flux_session_recv_reset(flux_session_t *session, uint8_t stream_id)
{
    if (!session) {
        return ESP_ERR_INVALID_ARG;
    }

    flux_session_state_lock(session);
    flux_fragment_recv_state_t *recv_state = flux_session_recv_find(session, stream_id);
    if (!recv_state) {
        if (session->last_completed_recv_valid && session->last_completed_recv_stream_id == stream_id) {
            session->last_completed_recv_valid = false;
            flux_session_state_unlock(session);
            return ESP_OK;
        }
        flux_session_state_unlock(session);
        return ESP_ERR_INVALID_STATE;
    }

    flux_recv_state_reset(recv_state);
    if (session->last_completed_recv_valid && session->last_completed_recv_stream_id == stream_id) {
        session->last_completed_recv_valid = false;
    }

    flux_session_state_unlock(session);
    ESP_LOGD(TAG, "Fragment receive state reset (stream=%d)", stream_id);
    return ESP_OK;
}
