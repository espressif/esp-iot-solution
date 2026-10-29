/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GATT_SESSION_H
#define GATT_SESSION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "host/ble_hs.h"
#include "sys/queue.h"
#include "esp_flux_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATT_SESSION_MAX_MBUF_QUOTA 10
#define GATT_SESSION_MAX_RETRIES    3

// BLE role definitions
typedef enum {
    GATT_SESSION_ROLE_MASTER = 0, // Master (Central) - use client API
    GATT_SESSION_ROLE_SLAVE = 1   // Slave (Peripheral) - use server API
} gatt_session_role_t;

// Forward declaration
typedef struct gatt_session gatt_session_t;

// Legacy callback types (map to flux callbacks)
// Terminal result of one send stream. stream_id identifies which transfer completed
// or was dropped during session destroy.
// (lets the caller order back-to-back sends). The library passes back the original source
// buffer (data/size) so the caller can free the correct buffer. The library auto-clears
// the send slot (frees its internal tracking arrays) but does NOT free data.
typedef void (*session_complete_cb)(gatt_session_t *session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size);
// A fully reassembled message for one receive stream. stream_id identifies which stream
// delivered the message (lets the caller order concurrent receives). Ownership of data is
// transferred to the application; caller must free((void *)data) after processing.
typedef void (*data_received_cb)(gatt_session_t *session, uint8_t stream_id, const uint8_t *data, uint32_t size);
// Progress for one in-flight stream. stream_id matches the send/receive slot;
// transferred and total are byte counts for that stream.
typedef void (*progress_cb)(gatt_session_t *session, uint8_t stream_id, uint32_t transferred, uint32_t total);
typedef void (*error_cb)(gatt_session_t *session, esp_err_t error);
typedef void (*mtu_changed_cb)(gatt_session_t *session, uint16_t mtu);

typedef struct {
    session_complete_cb session_complete_cb;
    data_received_cb data_received_cb;
    progress_cb progress_cb;
    error_cb error_cb;
    mtu_changed_cb mtu_changed_cb;
    void *arg;
} ble_session_callbacks_t;

// BLE transport context (passed to flux transport ops)
typedef struct {
    uint16_t conn_handle;
    uint16_t write_val_handle;
    gatt_session_role_t role;
} ble_transport_ctx_t;

// GATT Session structure - thin wrapper over flux_session_t
struct gatt_session {
    SLIST_ENTRY(gatt_session) next;     // Linked list node
    uint16_t conn_handle;               // Connection handle
    uint16_t write_val_handle;          // Write value handle
    uint16_t read_val_handle;           // Read value handle
    uint16_t mtu_size;                  // MTU size
    gatt_session_role_t role;           // BLE role: Master or Slave
    ble_session_callbacks_t callbacks;  // Application callbacks
    flux_session_t *flux_session;       // Protocol engine (esp_flux)
    ble_transport_ctx_t transport_ctx;  // BLE transport context
    bool was_disconnected;              // Whether connection was ever disconnected (for detecting reconnect)
    bool destroying;                    // True once destroy starts; blocks new callbacks
    bool destroy_scheduled;             // True once destroy event is queued to NimBLE host task
    uint16_t callback_inflight;         // Number of callbacks currently executing
};

SLIST_HEAD(gatt_session_list, gatt_session);

// GATT service handler (for BLE server characteristic access callback)
int gatt_session_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg);

// Session lifecycle
gatt_session_t *gatt_session_create(uint16_t conn_handle, const ble_session_callbacks_t *callbacks,
                                    uint16_t write_val_handle, uint16_t read_val_handle, uint16_t mtu_size, gatt_session_role_t role);
// NOTE: This API returns a raw pointer without lifecycle pinning.
// Do not cache or use it across asynchronous boundaries; prefer internal pinned helpers.
// In concurrent destroy scenarios this pointer may become invalid.
gatt_session_t *gatt_session_find_by_handle(uint16_t conn_handle);
// API contract: gatt_session_destroy() must be called from the NimBLE host task context.
// Avoid calling destroy from session callbacks to prevent reentrancy deadlocks.
esp_err_t gatt_session_destroy(gatt_session_t *session);
// Safe from any task context; posts session destroy to NimBLE host task queue.
esp_err_t gatt_session_schedule_destroy(gatt_session_t *session);
esp_err_t gatt_session_set_mtu(gatt_session_t *session, uint16_t mtu);

/** Underlying flux protocol session (may be NULL if not yet created / destroyed). */
flux_session_t *gatt_session_get_flux(const gatt_session_t *session);

/** Negotiated / configured MTU for this GATT session. */
uint16_t gatt_session_get_mtu(const gatt_session_t *session);

/*
 * Send / recv API — names align with flux_session_send / flux_session_recv_*.
 * The gatt_session_fragment_* symbols remain as deprecated aliases.
 */
esp_err_t gatt_session_send(gatt_session_t *session, const uint8_t *data,
                            uint32_t size, uint8_t window_size, uint8_t window_threshold_percent);
bool gatt_session_send_idle(gatt_session_t *session);
bool gatt_session_send_is_complete(gatt_session_t *session, uint8_t stream_id);
uint8_t gatt_session_send_get_progress(gatt_session_t *session, uint8_t stream_id);
esp_err_t gatt_session_send_reset(gatt_session_t *session, uint8_t stream_id);

bool gatt_session_recv_idle(gatt_session_t *session);
bool gatt_session_recv_is_complete(gatt_session_t *session, uint8_t stream_id);
uint8_t gatt_session_recv_get_progress(gatt_session_t *session, uint8_t stream_id);
esp_err_t gatt_session_recv_reset(gatt_session_t *session, uint8_t stream_id);

/** @deprecated Use gatt_session_send / gatt_session_send_* / gatt_session_recv_* */
#define gatt_session_fragment_send              gatt_session_send
#define gatt_session_fragment_send_idle         gatt_session_send_idle
#define gatt_session_fragment_send_is_complete  gatt_session_send_is_complete
#define gatt_session_fragment_send_get_progress gatt_session_send_get_progress
#define gatt_session_fragment_send_reset        gatt_session_send_reset
#define gatt_session_fragment_recv_idle         gatt_session_recv_idle
#define gatt_session_fragment_recv_is_complete  gatt_session_recv_is_complete
#define gatt_session_fragment_recv_get_progress gatt_session_recv_get_progress
#define gatt_session_fragment_recv_reset        gatt_session_recv_reset

#ifdef __cplusplus
}
#endif

#endif // GATT_SESSION_H
