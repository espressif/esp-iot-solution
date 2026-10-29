/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FLUX_TRANSPORT_H
#define FLUX_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* ────────────────────── Configuration Defaults ────────────────────── */
#define FLUX_MAX_MBUF_QUOTA         10
#define FLUX_MAX_BATCH_SENT_COUNT   10
#define FLUX_MAX_RETRIES            3
#define FLUX_MAX_FRAGMENTS          128
#define FLUX_MAX_REASSEMBLY_SIZE    (64 * 1024) // 64KB maximum reassembly buffer
#define FLUX_FRAGMENT_TIMEOUT_MS    10000       // Overall fragment transfer timeout 10s
#define FLUX_ACK_MIN_INTERVAL_MS    100         // ACK minimum interval 100ms
#define FLUX_ATT_HEADER_SIZE        3           // ATT header size (for BLE MTU calculation)
#define FLUX_SACK_BITMAP_SIZE       16          // SACK bitmap size (16 bytes = 128 bits)
#define FLUX_MAX_RETRANSMIT_RETRIES 3           // Maximum retransmission count per fragment
#define FLUX_RETRANSMIT_TIMEOUT_MS  500         // Per-fragment retransmission timeout 500ms

// Compile-time guard: the SACK bitmap must be wide enough to hold one bit per fragment.
// If FLUX_MAX_FRAGMENTS is increased without updating FLUX_SACK_BITMAP_SIZE, fragments
// beyond the bitmap width would be silently excluded from SACK, causing data loss.
_Static_assert(FLUX_MAX_FRAGMENTS <= FLUX_SACK_BITMAP_SIZE * 8,
               "FLUX_SACK_BITMAP_SIZE too small for FLUX_MAX_FRAGMENTS — update both together");

// Concurrency: number of simultaneous logical transfers per session.
// The wire-level stream_id (carried in the packet header "control" nibble, 0-15)
// lets the sender/receiver demultiplex concurrent transfers.
#define FLUX_MAX_CONCURRENT_SENDS    2            // Max concurrent send streams (1 active + 1 pipelined)
#define FLUX_MAX_CONCURRENT_RECVS    2            // Max concurrent receive streams

#define FLUX_PACKET_TYPE_MASK        0x07         // packet_type occupies the 3-bit control field
#define FLUX_STREAM_ID_MASK          0x07         // stream_id occupies the 3-bit control field
#define FLUX_STREAM_ID_INVALID       0xFF         // Sentinel for "no stream"

/* ────────────────────── Packet Type Definitions ────────────────────── */
#define FLUX_PACKET_TYPE_SINGLE     0x1 // Single packet data
#define FLUX_PACKET_TYPE_FRAGMENT   0x2 // Fragment packet
#define FLUX_PACKET_TYPE_SACK       0x3 // SACK packet (selective acknowledgment)
#define FLUX_PACKET_TYPE_START      0x4 // Fragment start packet

/* ────────────────────── Transport Layer Interface ────────────────────── */

/** Opaque session handle — layout is private (see esp_flux_session_priv.h). */
typedef struct flux_session flux_session_t;

/**
* @brief Transport layer operations (implemented by the actual transport, e.g., BLE, Wi-Fi, UART)
*
* The transport layer only needs to provide unreliable send and MTU query.
* All reliability (SACK, retransmission, windowing) is handled by the esp_flux protocol.
*/
typedef struct {
    /**
    * @brief Send data through the transport (unreliable, no delivery guarantee)
    *
    * @param ctx     Opaque transport context pointer (e.g., BLE connection info)
    * @param data    Data buffer to send
    * @param size    Data size in bytes
    * @return ESP_OK on success (queued for sending), error code on failure
    */
    esp_err_t (*send)(void *ctx, const uint8_t *data, uint16_t size);

    /**
    * @brief Get current maximum transmission unit
    *
    * @param ctx     Opaque transport context pointer
    * @return MTU size in bytes (including headers)
    */
    uint16_t (*get_mtu)(void *ctx);
} flux_transport_ops_t;

/* ────────────────────── Application Callbacks ────────────────────── */

/**
* @brief Application-level callbacks for session events
*/
typedef struct {
    /** Called when a send operation completes, fails, or is dropped by destroy. */
    void (*session_complete_cb)(flux_session_t *session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size);

    /** Called when complete data has been received and reassembled.
     *
     *  Ownership of @p data is transferred to the callback: the buffer was heap-allocated
     *  by esp_flux and the caller is responsible for calling free((void *)data) when done.
     *  The receive slot is already reset by the time this callback fires, so @p data is
     *  the only reference — it is safe to hold or forward the pointer across tasks. */
    void (*data_received_cb)(flux_session_t *session, uint8_t stream_id, const uint8_t *data, uint32_t size);

    /** Called periodically during send/receive to report progress.
     *  @p stream_id selects the in-flight transfer. @p transferred and @p total are byte counts for that stream. */
    void (*progress_cb)(flux_session_t *session, uint8_t stream_id, uint32_t transferred, uint32_t total);

    /** Called on error (timeout, max retries exceeded, etc.) */
    void (*error_cb)(flux_session_t *session, esp_err_t error);

    /** User-defined argument passed to all callbacks */
    void *arg;
} flux_callbacks_t;

/* ────────────────────── Public API ────────────────────── */

/**
* @brief Create a new flux session
*
* @param ops           Transport operations (must outlive the session)
* @param transport_ctx Opaque context passed to transport ops
* @param callbacks     Application callbacks (copied internally)
* @param mtu           Current MTU size
* @return Session handle, or NULL on failure
*/
flux_session_t *flux_session_create(const flux_transport_ops_t *ops, void *transport_ctx,
                                    const flux_callbacks_t *callbacks, uint16_t mtu);

/**
* @brief Destroy a flux session and free all resources
*
* Must not be called from flux callbacks running on the transmit task.
*/
esp_err_t flux_session_destroy(flux_session_t *session);

/**
* @brief Update the MTU for a session (e.g., after MTU negotiation)
*/
esp_err_t flux_session_set_mtu(flux_session_t *session, uint16_t mtu);

/**
* @brief Get the current MTU for a session
*/
uint16_t flux_session_get_mtu(const flux_session_t *session);

/**
* @brief Get the session identifier assigned at create time
*/
uint32_t flux_session_get_id(const flux_session_t *session);

/**
* @brief Get the user argument from the session's callback config
*/
void *flux_session_get_user_arg(const flux_session_t *session);

/**
* @brief Maximum reassembled message size for the current MTU (bytes)
*
* Equals max START payload + (FLUX_MAX_FRAGMENTS - 1) * max FRAGMENT payload.
* Returns 0 if the session is NULL or MTU has not been applied yet.
*/
size_t flux_session_max_message_bytes(const flux_session_t *session);

/**
* @brief Start sending data with automatic fragmentation
*
* @param session                   Session handle
* @param data                      Data to send (caller-owned; never freed by esp_flux)
* @param size                      Data size in bytes
* @param window_size               Sliding window size (0xFF = unlimited, 0 = default 6)
* @param window_threshold_percent  Window occupancy % to trigger immediate SACK (0 = default 50)
* @return ESP_OK on success
*/
esp_err_t flux_session_send(flux_session_t *session, const uint8_t *data,
                            uint32_t size, uint8_t window_size, uint8_t window_threshold_percent);

/**
* @brief Feed received data into the session (called by transport layer)
*
* When the transport layer receives data, it should call this function
* to feed raw bytes into the protocol engine. The protocol engine will
* parse packet types and handle accordingly (reassembly, ACK processing, etc.)
*
* @param session   Session handle
* @param data      Received raw packet data
* @param size      Packet data size
* @return ESP_OK on success
*/
esp_err_t flux_session_feed_data(flux_session_t *session, const uint8_t *data, uint16_t size);

/**
* @brief Check if the send engine is idle (no active send)
*/
bool flux_session_send_idle(flux_session_t *session);

/**
* @brief Check if the current send operation is complete
*/
bool flux_session_send_is_complete(flux_session_t *session, uint8_t stream_id);

/**
* @brief Get current send progress (0-100%)
*/
uint8_t flux_session_send_get_progress(flux_session_t *session, uint8_t stream_id);

/**
* @brief Reset send state and free internal resources (call after on_complete)
*/
esp_err_t flux_session_send_reset(flux_session_t *session, uint8_t stream_id);

/**
* @brief Check if the receive engine is idle
*/
bool flux_session_recv_idle(flux_session_t *session);

/**
* @brief Check if reception is complete
*/
bool flux_session_recv_is_complete(flux_session_t *session, uint8_t stream_id);

/**
* @brief Get current receive progress (0-100%)
*/
uint8_t flux_session_recv_get_progress(flux_session_t *session, uint8_t stream_id);

/**
* @brief Reset receive state for the given stream and release internal resources.
*
* The receive slot is freed and becomes available for a new stream.
* Note: since esp_flux transfers buffer ownership to the application via
* data_received_cb, the reassembly buffer is NOT freed here — the caller
* is responsible for free()ing the data pointer received in the callback.
*/
esp_err_t flux_session_recv_reset(flux_session_t *session, uint8_t stream_id);

#ifdef __cplusplus
}
#endif

#endif // FLUX_TRANSPORT_H
