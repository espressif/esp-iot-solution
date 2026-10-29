/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_gmp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize global GMP state. Call once before other APIs. */
void esp_gmp_init(void);

/**
 * Release internal resources after all links have been unregistered and all
 * transport TX completions have been delivered.
 *
 * This is not a runtime link teardown API. If any link is still active, the
 * call is ignored to avoid freeing mutexes/buffers still owned by transports.
 */
void esp_gmp_deinit(void);

/**
 * Optional maintenance hook for TX queue retry or statistics.
 * Transport timeouts are handled by the bound transport.
 */
void esp_gmp_poll(void);

/**
 * Register a link bound to a transport.
 * The transport ops must outlive the registration.
 */
esp_err_t esp_gmp_link_register(esp_gmp_link_t link, const esp_gmp_transport_t *ops, void *transport_ctx);

/**
 * Unregister a link and drop queued (not yet in-flight) TX buffers.
 * In-flight frames are released later via esp_gmp_transport_tx_done or a failed
 * send path; the link slot may remain until those completions arrive.
 */
void esp_gmp_link_unregister(esp_gmp_link_t link);

/** Register application handler for complete reassembled GMP messages. */
void esp_gmp_on_packet_register(esp_gmp_on_packet_fn cb, void *user_ctx);

/**
 * Inject one complete GMP frame from a transport.
 * Return true if on_packet took ownership of @p data; otherwise caller must free it.
 */
bool esp_gmp_input(esp_gmp_link_t link, const uint8_t *data, size_t len);

/**
 * Notify GMP that an asynchronous transport has finished using a TX frame.
 * A transport that returns ESP_OK from send() takes temporary ownership of the
 * frame pointer and must eventually call this once it no longer uses the bytes.
 * Call from the transport completion path after send() has returned.
 */
void esp_gmp_transport_tx_done(esp_gmp_link_t link, const uint8_t *data);

/**
 * Send one complete GMP message via the bound transport.
 * Copies @p payload into an owned frame buffer.
 *
 * @return
 *  - ESP_OK: frame sent or queued for send.
 *  - ESP_ERR_ESP_GMP_TX_QUEUE_FULL: transient backpressure. The link is alive
 *    and the caller may retry after in-flight frames complete; use
 *    esp_gmp_tx_in_flight() to gauge depth instead of retrying blindly.
 *  - ESP_ERR_INVALID_STATE: the link is not registered or is closing. Retrying
 *    will never succeed; the caller should abandon the transfer.
 *  - ESP_ERR_INVALID_ARG / ESP_ERR_NO_MEM / ESP_ERR_INVALID_SIZE: bad
 *    parameters, allocation failure, or payload exceeding the link's limit.
 */
esp_err_t esp_gmp_send(esp_gmp_link_t link, const esp_gmp_tx_params_t *params, const uint8_t *payload, size_t payload_len);

/**
 * @brief Number of TX frames currently queued or in flight for a link.
 *
 * Lets a profile size its own pipeline depth against real backpressure instead
 * of guessing a constant or polling until a send fails.
 *
 * @param link The link to query
 * @return Queued + in-flight frame count, or 0 if the link is not registered
 */
size_t esp_gmp_tx_in_flight(esp_gmp_link_t link);

/**
 * @brief TX queue capacity per link.
 *
 * Compare against esp_gmp_tx_in_flight() to find remaining headroom.
 *
 * @return Maximum frames a single link can hold queued plus in flight
 */
size_t esp_gmp_tx_capacity(void);

/**
 * @brief Suggested application/profile pipeline depth for @p link.
 *
 * Returns remaining TX headroom (capacity - in_flight), at least 1 when the
 * link is registered. Profiles (OTA host / FT) should not exceed this when
 * pipelining DATA writes.
 */
size_t esp_gmp_recommended_pipeline_depth(esp_gmp_link_t link);

/** Effective max GMP payload for the link (policy cap ∩ transport limit). */
size_t esp_gmp_max_payload_effective(esp_gmp_link_t link);

/**
 * @brief Packet handler function type for profile registration.
 *
 * @param pkt Received packet with decoded header and payload
 * @return true if handler took ownership of payload buffer, false otherwise
 */
typedef bool (*esp_gmp_packet_handler_fn)(const esp_gmp_rx_t *pkt);

/**
 * @brief Register a packet handler for a specific group ID.
 *
 * Multiple handlers may share the same group_id; each matching RX packet is
 * delivered to every registered handler until one returns true (takes frame
 * ownership). Profiles stay independent (e.g. OS device + OTA host both on
 * GRP_OS).
 *
 * @param group_id GMP group ID to handle (e.g., ESP_GMP_GRP_OTA)
 * @param handler Handler function to invoke for matching packets
 * @return ESP_OK on success, ESP_ERR_NO_MEM if handler table is full
 */
esp_err_t esp_gmp_register_handler(uint8_t group_id,
                                   esp_gmp_packet_handler_fn handler);

/**
 * @brief Unregister a packet handler previously registered for @p group_id.
 *
 * Matches both group_id and handler function so co-registered profiles on the
 * same group do not unregister each other.
 *
 * @param group_id GMP group ID
 * @param handler Handler function to remove
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if not registered
 */
esp_err_t esp_gmp_unregister_handler(uint8_t group_id,
                                     esp_gmp_packet_handler_fn handler);

/**
 * @brief Link event types for subscription.
 */
typedef enum {
    ESP_GMP_LINK_EVENT_DOWN,            /**< Link went down */
    ESP_GMP_LINK_EVENT_TRANSPORT_ERROR, /**< Transport reported an error */
    ESP_GMP_LINK_EVENT_MTU_CHANGED,     /**< MTU capability changed */
} esp_gmp_link_event_type_t;

/**
 * @brief Link event callback function type.
 *
 * @param link The link that triggered the event
 * @param event Event type
 * @param error Error code (for TRANSPORT_ERROR events)
 * @param ctx User context passed during subscription
 */
typedef void (*esp_gmp_link_event_fn)(esp_gmp_link_t link,
                                      esp_gmp_link_event_type_t event,
                                      esp_err_t error,
                                      void *ctx);

/**
 * @brief Opaque handle for link event subscription.
 */
typedef struct esp_gmp_link_event_sub *esp_gmp_link_event_sub_t;

/**
 * @brief Subscribe to link events.
 *
 * Multiple profiles can subscribe to link events simultaneously.
 * This replaces the single-slot callback that caused subscription conflicts.
 *
 * @param fn Callback function to invoke on link events
 * @param ctx User context passed to callback
 * @return Subscription handle, or NULL on failure
 */
esp_gmp_link_event_sub_t esp_gmp_link_event_subscribe(esp_gmp_link_event_fn fn,
                                                      void *ctx);

/**
 * @brief Unsubscribe from link events.
 *
 * Profiles should call this during deinit.
 *
 * @param sub Subscription handle from esp_gmp_link_event_subscribe
 */
void esp_gmp_link_event_unsubscribe(esp_gmp_link_event_sub_t sub);

/**
 * @brief Notify all subscribers of a link event (internal use).
 *
 * Called by Flux adapter or transport layer when link events occur.
 *
 * @param link The link that triggered the event
 * @param event Event type
 * @param error Error code (for TRANSPORT_ERROR events)
 */
void esp_gmp_notify_link_event(esp_gmp_link_t link,
                               esp_gmp_link_event_type_t event,
                               esp_err_t error);

/**
 * @brief Send a response to a received request (helper function).
 *
 * Automatically fills in the response op, sequence, and other fields
 * based on the request packet.
 *
 * @param req The received request packet
 * @param status GMP status code (ESP_GMP_STATUS_OK, etc.)
 * @param payload Response payload (or NULL)
 * @param payload_len Response payload length
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t esp_gmp_reply(const esp_gmp_rx_t *req,
                        uint8_t status,
                        const void *payload,
                        size_t payload_len);

/**
 * @brief Allocate next sequence number for a link (helper function).
 *
 * Provides per-link sequence allocation to avoid conflicts between
 * multiple profiles using the same link.
 *
 * @param link The link to allocate sequence for
 * @return Next sequence number (1-65535, skips 0)
 */
uint16_t esp_gmp_seq_next(esp_gmp_link_t link);

#ifdef __cplusplus
}
#endif
