/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "freertos/semphr.h"
#include "host/ble_hs.h"
#include "sys/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_CONN_HANDLE_INVALID  0xFFFF

#define LL_PACKET_TIME      2120 // 2120 microseconds (2120 * 1.25ms = 2650ms)
#define LL_PACKET_LENGTH    251  // 251 bytes (251 * 8 = 2008 bits)  (251 * 1.25ms = 313.75ms)
#define OPTIMAL_MTU_SIZE    498  // Optimal MTU size for LL_PACKET_LENGTH 251 bytes

// Connection state enumeration
typedef enum {
    BLE_CONN_STATE_DISCONNECTED = 0,
    BLE_CONN_STATE_RECONNECT_QUEUED, // In reconnect queue, waiting to be processed
    BLE_CONN_STATE_CONNECTING,       // Actually attempting to connect (ble_gap_connect called)
    BLE_CONN_STATE_CONNECTED         // Connected to device
} ble_connection_state_t;

// BLE event type definitions
typedef enum {
    BLE_EVENT_TYPE_CONNECT_COMPLETE = 0,    // Connection successful
    BLE_EVENT_TYPE_RECONNECT_COMPLETE,      // Reconnect successful
    BLE_EVENT_TYPE_CONNECT_FAILED,          // Connection failed
    BLE_EVENT_TYPE_RECONNECT_FAILED,        // Reconnect failed
    BLE_EVENT_TYPE_DISCONNECTED,            // Disconnected
} ble_event_type_t;

typedef struct ble_manager ble_manager_t;
typedef struct ble_connection ble_connection_t;

// BLE event callback function type definitions
typedef void (*ble_adv_cb_t)(ble_manager_t *manager, uint8_t type, void *arg);
typedef void (*ble_scan_cb_t)(ble_manager_t *manager, uint8_t type, const struct ble_gap_disc_desc *result, void *arg);
typedef void (*ble_connect_cb_t)(ble_manager_t *manager, uint8_t type, ble_connection_t *conn, int status_or_reason, void *arg);
typedef void (*ble_disconnect_cb_t)(ble_manager_t *manager, ble_connection_t *conn, int reason, void *arg);
typedef void (*ble_conn_update_cb_t)(ble_manager_t *manager, ble_connection_t *conn, void *arg);

// BLE manager callback function structure
typedef struct {
    ble_adv_cb_t ble_adv_cb;               // Advertising callback
    ble_scan_cb_t ble_scan_cb;             // Scan callback
    ble_connect_cb_t ble_connect_cb;       // Connection/Reconnect callback
    ble_disconnect_cb_t ble_disconnect_cb; // Disconnect callback
    ble_conn_update_cb_t ble_conn_update_cb; // Connection parameters update complete callback
    void *arg;                             // User parameter
} __attribute__((packed)) ble_manager_callbacks_t;

// Connection info structure, uses SLIST_ENTRY to support linked list
struct __attribute__((packed)) ble_connection {
    SLIST_ENTRY(ble_connection) next;      // Linked list node
    uint16_t conn_handle;                  // Connection handle
    ble_addr_t peer_addr;                  // Peer address
    ble_connection_state_t state;          // Connection state
    uint8_t role;                          // Connection role
    uint16_t conn_interval;                // Connection interval
    uint16_t conn_latency;                 // Connection latency
    uint16_t supervision_timeout;          // Supervision timeout
    uint16_t min_interval;                 // Minimum connection interval
    uint16_t max_interval;                 // Maximum connection interval
    uint32_t connect_timestamp;            // Connection timestamp
    int8_t rssi;                           // Signal strength
    uint16_t mtu;                          // MTU size
    uint8_t device_name[32];               // Device name
    // Auto-reconnect related fields
    bool auto_reconnect_enabled;           // Whether auto-reconnect is enabled
    uint32_t reconnect_interval_ms;        // Reconnect interval (milliseconds)
    uint8_t reconnect_max_attempts;        // Maximum reconnect attempts (0 means unlimited)
    uint8_t reconnect_attempt_count;       // Current reconnect count
    uint32_t disconnect_timestamp;         // Disconnect timestamp
    bool is_first_connect;                 // Whether this is first connection (to distinguish first connection from reconnect)
    TimerHandle_t reconnect_timer;         // Reconnect timer handle (for delayed reconnect)
};

// Define connection list head
SLIST_HEAD(ble_connection_list, ble_connection);

// BLE manager structure
struct __attribute__((packed)) ble_manager {
    struct ble_connection_list connections; // Connection list
    SemaphoreHandle_t connections_mutex;    // Connection list lock
    struct ble_gap_adv_params saved_adv_params; // Saved advertising parameters (for restarting advertising after disconnection)
    uint32_t saved_adv_duration_ms;         // Saved advertising duration (milliseconds)
    ble_manager_callbacks_t callbacks;      // Callback functions
    bool initialized;                       // Initialization flag
    bool scanning;                          // Scanning state
    bool advertising;                       // Advertising state
    uint8_t active_connections;             // Active connection count
    uint16_t def_mtu;                       // Default MTU setting
};

ble_manager_t *ble_manager_init(const ble_manager_callbacks_t *callbacks);
esp_err_t ble_manager_deinit(ble_manager_t *manager);

esp_err_t ble_manager_start_advertising(ble_manager_t *manager, const struct ble_gap_adv_params *adv_params, uint32_t duration_ms);
esp_err_t ble_manager_stop_advertising(ble_manager_t *manager);

esp_err_t ble_manager_start_scan(ble_manager_t *manager, uint32_t duration_ms);
esp_err_t ble_manager_stop_scan(ble_manager_t *manager);

bool ble_manager_is_connected(ble_manager_t *manager, const ble_addr_t *addr);
esp_err_t ble_manager_connect(ble_manager_t *manager, const ble_addr_t *addr, uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);
esp_err_t ble_manager_disconnect(ble_manager_t *manager, uint16_t conn_handle);
esp_err_t ble_manager_disconnect_all(ble_manager_t *manager);

uint8_t ble_manager_get_active_connections(ble_manager_t *manager);
uint8_t ble_manager_get_total_connections(ble_manager_t *manager);
esp_err_t ble_manager_get_mtu_by_conn(ble_manager_t *manager, uint16_t conn_handle, uint16_t *mtu);
bool ble_manager_get_connection_snapshot_by_handle(ble_manager_t *manager, uint16_t conn_handle, ble_connection_t *out_conn);
bool ble_manager_get_connection_snapshot_by_addr(ble_manager_t *manager, const uint8_t *addr, uint8_t addr_type, ble_connection_t *out_conn);

esp_err_t ble_manager_update_rssi(ble_manager_t *manager, uint16_t conn_handle, int8_t rssi);
esp_err_t ble_manager_update_mtu(ble_manager_t *manager, uint16_t conn_handle);
esp_err_t ble_manager_update_connection_params(ble_manager_t *manager, uint16_t conn_handle,
                                               uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);
esp_err_t ble_manager_update_phy(ble_manager_t *manager, uint16_t conn_handle);

esp_err_t ble_manager_enable_auto_reconnect(ble_manager_t *manager, uint16_t conn_handle,
                                            uint32_t reconnect_interval_ms, uint8_t max_attempts);
esp_err_t ble_manager_disable_auto_reconnect(ble_manager_t *manager, uint16_t conn_handle);

char *addr_str(const void *addr);

#ifdef __cplusplus
}
#endif

#endif // BLE_MANAGER_H
