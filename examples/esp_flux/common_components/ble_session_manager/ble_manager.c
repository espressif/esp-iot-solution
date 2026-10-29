/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "host/ble_gap.h"
#include "ble_manager.h"

static const char *TAG = "ble_manager";

static ble_manager_t *g_ble_manager = NULL;

static inline void ble_manager_connections_lock(ble_manager_t *manager)
{
    if (manager && manager->connections_mutex) {
        xSemaphoreTake(manager->connections_mutex, portMAX_DELAY);
    }
}

static inline void ble_manager_connections_unlock(ble_manager_t *manager)
{
    if (manager && manager->connections_mutex) {
        xSemaphoreGive(manager->connections_mutex);
    }
}

static ble_connection_t *ble_manager_find_connection_by_handle_locked(ble_manager_t *manager, uint16_t conn_handle)
{
    ble_connection_t *conn;
    SLIST_FOREACH(conn, &manager->connections, next) {
        if (conn->conn_handle == conn_handle) {
            return conn;
        }
    }
    return NULL;
}

static ble_connection_t *ble_manager_find_connection_by_addr_locked(ble_manager_t *manager, const uint8_t *addr, uint8_t addr_type)
{
    ble_connection_t *conn;
    SLIST_FOREACH(conn, &manager->connections, next) {
        if (memcmp(conn->peer_addr.val, addr, BLE_DEV_ADDR_LEN) == 0 && conn->peer_addr.type == addr_type) {
            return conn;
        }
    }
    return NULL;
}

// Reconnect timer callback parameter structure
typedef struct {
    ble_manager_t *manager;
    ble_addr_t peer_addr;
} __attribute__((packed)) reconnect_timer_params_t;

char *addr_str(const void *addr)
{
    static char buf[6 * 2 + 5 + 1];
    const uint8_t *u8p;

    u8p = addr;
    sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);

    return buf;
}

ble_manager_t *ble_manager_init(const ble_manager_callbacks_t *callbacks)
{
    // Initialize manager structure
    g_ble_manager = calloc(1, sizeof(ble_manager_t));
    if (!g_ble_manager) {
        ESP_LOGE(TAG, "Failed to allocate memory for BLE manager");
        return NULL;
    }

    // Initialize connection list
    SLIST_INIT(&g_ble_manager->connections);
    g_ble_manager->connections_mutex = xSemaphoreCreateMutex();
    if (!g_ble_manager->connections_mutex) {
        ESP_LOGE(TAG, "Failed to create connections mutex");
        free(g_ble_manager);
        g_ble_manager = NULL;
        return NULL;
    }
    g_ble_manager->initialized = true;
    g_ble_manager->def_mtu = OPTIMAL_MTU_SIZE;

    if (callbacks) {
        memcpy(&g_ble_manager->callbacks, callbacks, sizeof(ble_manager_callbacks_t));
        ESP_LOGI(TAG, "BLE manager callbacks set");
    }

    int rc = ble_att_set_preferred_mtu(g_ble_manager->def_mtu);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set preferred MTU; rc = %d", rc);
    }

    // Set default PHY preference to prefer 2M PHY for better throughput
    // This sets the preferred PHY for both TX and RX directions
    // BLE_HCI_LE_PHY_2M_PREF_MASK = 0x02 (prefer 2M PHY)
    // BLE_HCI_LE_PHY_1M_PREF_MASK = 0x01 (fallback to 1M PHY if 2M not supported)
    uint8_t tx_phys = BLE_HCI_LE_PHY_2M_PREF_MASK | BLE_HCI_LE_PHY_1M_PREF_MASK;
    uint8_t rx_phys = BLE_HCI_LE_PHY_2M_PREF_MASK | BLE_HCI_LE_PHY_1M_PREF_MASK;
    rc = ble_gap_set_prefered_default_le_phy(tx_phys, rx_phys);
    if (rc != 0) {
        ESP_LOGW(TAG, "Failed to set default PHY preference; rc = %d (2M PHY may not be supported)", rc);
    } else {
        ESP_LOGI(TAG, "Default PHY preference set to prefer 2M PHY");
    }

    ESP_LOGI(TAG, "BLE manager initialized");
    return g_ble_manager;
}

esp_err_t ble_manager_deinit(ble_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // Disconnect all connections (this will clean up reconnect timers via ble_manager_remove_connection_node)
    ble_manager_disconnect_all(manager);
    ble_manager_stop_scan(manager);
    ble_manager_stop_advertising(manager);

    // Clean up any remaining connection nodes (in case some weren't cleaned up by disconnect_all)
    // Note: ble_manager_disconnect_all should have removed all nodes, but we do this as a safety measure
    ble_connection_t *conn;
    while (true) {
        ble_manager_connections_lock(manager);
        conn = SLIST_FIRST(&manager->connections);
        if (!conn) {
            ble_manager_connections_unlock(manager);
            break;
        }
        SLIST_REMOVE_HEAD(&manager->connections, next);
        ble_manager_connections_unlock(manager);
        // Clean up reconnect timer before removing node
        if (conn->reconnect_timer) {
            reconnect_timer_params_t *timer_params = (reconnect_timer_params_t *)pvTimerGetTimerID(conn->reconnect_timer);
            if (timer_params) {
                free(timer_params);
            }
            xTimerDelete(conn->reconnect_timer, 0);
            conn->reconnect_timer = NULL;
        }
        free(conn);
    }

    if (manager->connections_mutex) {
        vSemaphoreDelete(manager->connections_mutex);
        manager->connections_mutex = NULL;
    }

    // Clean up manager
    free(g_ble_manager);
    g_ble_manager = NULL;

    ESP_LOGI(TAG, "BLE manager deinitialized");
    return ESP_OK;
}

// Remove and free connection node from list (internal function)
static void ble_manager_remove_connection_node(ble_manager_t *manager, ble_connection_t *conn)
{
    if (!manager || !conn) {
        return;
    }

    ESP_LOGI(TAG, "Removing connection node for device %s", addr_str(conn->peer_addr.val));
    // Stop and clean up reconnection timer
    if (conn->reconnect_timer) {
        reconnect_timer_params_t *timer_params = (reconnect_timer_params_t *)pvTimerGetTimerID(conn->reconnect_timer);
        if (timer_params) {
            free(timer_params);
        }
        xTimerDelete(conn->reconnect_timer, 0);
        conn->reconnect_timer = NULL;
    }

    // Remove node from list
    ble_connection_t *prev = NULL;
    ble_manager_connections_lock(manager);
    ble_connection_t *cur = SLIST_FIRST(&manager->connections);

    // If it's the first element
    if (cur == conn) {
        SLIST_REMOVE_HEAD(&manager->connections, next);
    } else {
        // Find the element to remove and its previous element
        while (cur != NULL && cur != conn) {
            prev = cur;
            cur = SLIST_NEXT(cur, next);
        }

        if (cur == conn && prev != NULL) {
            SLIST_REMOVE_AFTER(prev, next);
        }
    }
    ble_manager_connections_unlock(manager);

    // Free connection node
    free(conn);
    ESP_LOGD(TAG, "Connection node removed and freed");
}

static bool ble_manager_remove_connection_node_by_handle(ble_manager_t *manager, uint16_t conn_handle)
{
    if (!manager || !manager->initialized) {
        return false;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *target = ble_manager_find_connection_by_handle_locked(manager, conn_handle);
    if (!target) {
        ble_manager_connections_unlock(manager);
        return false;
    }

    ble_connection_t *prev = NULL;
    ble_connection_t *cur = SLIST_FIRST(&manager->connections);
    if (cur == target) {
        SLIST_REMOVE_HEAD(&manager->connections, next);
    } else {
        while (cur != NULL && cur != target) {
            prev = cur;
            cur = SLIST_NEXT(cur, next);
        }
        if (cur == target && prev != NULL) {
            SLIST_REMOVE_AFTER(prev, next);
        }
    }
    ble_manager_connections_unlock(manager);

    if (target->reconnect_timer) {
        reconnect_timer_params_t *timer_params = (reconnect_timer_params_t *)pvTimerGetTimerID(target->reconnect_timer);
        if (timer_params) {
            free(timer_params);
        }
        xTimerDelete(target->reconnect_timer, 0);
        target->reconnect_timer = NULL;
    }
    free(target);
    return true;
}

static bool ble_manager_remove_connection_node_by_addr(ble_manager_t *manager, const uint8_t *addr, uint8_t addr_type)
{
    if (!manager || !manager->initialized || !addr) {
        return false;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *target = ble_manager_find_connection_by_addr_locked(manager, addr, addr_type);
    if (!target) {
        ble_manager_connections_unlock(manager);
        return false;
    }

    ble_connection_t *prev = NULL;
    ble_connection_t *cur = SLIST_FIRST(&manager->connections);
    if (cur == target) {
        SLIST_REMOVE_HEAD(&manager->connections, next);
    } else {
        while (cur != NULL && cur != target) {
            prev = cur;
            cur = SLIST_NEXT(cur, next);
        }
        if (cur == target && prev != NULL) {
            SLIST_REMOVE_AFTER(prev, next);
        }
    }
    ble_manager_connections_unlock(manager);

    if (target->reconnect_timer) {
        reconnect_timer_params_t *timer_params = (reconnect_timer_params_t *)pvTimerGetTimerID(target->reconnect_timer);
        if (timer_params) {
            free(timer_params);
        }
        xTimerDelete(target->reconnect_timer, 0);
        target->reconnect_timer = NULL;
    }
    free(target);
    return true;
}

// Check if a connection is a valid reconnect candidate
static bool ble_manager_is_valid_reconnect_candidate(ble_connection_t *conn)
{
    return conn && conn->auto_reconnect_enabled && (conn->state == BLE_CONN_STATE_RECONNECT_QUEUED) &&
           (conn->reconnect_max_attempts == 0 || conn->reconnect_attempt_count < conn->reconnect_max_attempts);
}

// Check if there's a connection attempt in progress
// A connection is truly in progress only if state is CONNECTING
// (BLE_CONN_STATE_RECONNECT_QUEUED means waiting in queue, not actually connecting)
bool ble_manager_is_connecting_in_progress(ble_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return false;
    }

    ble_connection_t *conn;
    ble_manager_connections_lock(manager);
    SLIST_FOREACH(conn, &manager->connections, next) {
        // Only consider it truly connecting if state is CONNECTING
        // (not RECONNECT_QUEUED which means waiting in queue)
        if (conn->state == BLE_CONN_STATE_CONNECTING) {
            ble_manager_connections_unlock(manager);
            return true;
        }
    }
    ble_manager_connections_unlock(manager);
    return false;
}

// Process next device in reconnect queue from a specific starting node
static void ble_manager_reconnect_queue_process_next_from(ble_manager_t *manager, ble_connection_t *start_from)
{
    if (!manager || !manager->initialized) {
        return;
    }

    // Check if there's already a connection attempt in progress
    if (ble_manager_is_connecting_in_progress(manager)) {
        ESP_LOGD(TAG, "Connection attempt already in progress, skipping queue processing (start_from=%s)",
                 start_from ? addr_str(start_from->peer_addr.val) : "NULL");
        return;
    }

    // Find next connection in reconnect queue starting from start_from node
    // Strategy: search circularly from start_from (or from beginning if start_from is NULL)
    ble_connection_t *conn = NULL;
    bool search_after_start = false;

    ESP_LOGD(TAG, "Processing reconnect queue from start_from=%s", start_from ? addr_str(start_from->peer_addr.val) : "NULL");

    ble_manager_connections_lock(manager);
    // First pass: search from start_from to end of list
    SLIST_FOREACH(conn, &manager->connections, next) {
        if (start_from && conn == start_from) {
            search_after_start = true;
            continue; // Skip start node itself
        }
        if (search_after_start || start_from == NULL) {
            if (ble_manager_is_valid_reconnect_candidate(conn)) {
                ESP_LOGD(TAG, "Found next candidate: %s (state=%d, attempt=%d)",
                         addr_str(conn->peer_addr.val), conn->state, conn->reconnect_attempt_count);
                break;
            }
        }
    }

    // Second pass: if not found, search from beginning to start_from (circular search)
    if (!conn && start_from) {
        SLIST_FOREACH(conn, &manager->connections, next) {
            if (conn == start_from) {
                // Check start_from itself as last resort
                if (ble_manager_is_valid_reconnect_candidate(conn)) {
                    ESP_LOGI(TAG, "No other candidates found, retrying failed device %s", addr_str(conn->peer_addr.val));
                    break;
                }
                // No valid candidate found
                conn = NULL;
                break;
            }
            if (ble_manager_is_valid_reconnect_candidate(conn)) {
                ESP_LOGD(TAG, "Found candidate before start node: %s", addr_str(conn->peer_addr.val));
                break;
            }
        }
    }

    if (!conn) {
        ble_manager_connections_unlock(manager);
        ESP_LOGI(TAG, "No valid reconnect candidates found in queue");
        return;
    }

    // Change state from RECONNECT_QUEUED to CONNECTING (actually attempting to connect)
    conn->state = BLE_CONN_STATE_CONNECTING;
    // Update reconnect count
    conn->reconnect_attempt_count++;
    ble_manager_connections_unlock(manager);

    ESP_LOGI(TAG, "Processing reconnect queue: attempting to reconnect to device %s (attempt %d/%d)",
             addr_str(conn->peer_addr.val), conn->reconnect_attempt_count, conn->reconnect_max_attempts > 0 ? conn->reconnect_max_attempts : 999);

    // State is already CONNECTING (set above), conn_handle will be set in BLE_GAP_EVENT_CONNECT event
    // Attempt reconnect (state is already CONNECTING, ble manager connect will reuse it)
    esp_err_t ret = ble_manager_connect(manager, &conn->peer_addr, conn->min_interval, conn->max_interval,
                                        conn->conn_latency, conn->supervision_timeout);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Reconnect attempt failed: %d for device %s", ret, addr_str(conn->peer_addr.val));
        // If reconnect failed, reset conn_handle and set state back to RECONNECT_QUEUED
        ble_manager_connections_lock(manager);
        conn->state = BLE_CONN_STATE_DISCONNECTED;
        conn->conn_handle = BLE_CONN_HANDLE_INVALID; // Reset conn_handle
        // If reconnect failed and auto-reconnect still enabled, add back to queue
        if (conn->auto_reconnect_enabled) {
            // Check if max reconnect attempts exceeded
            if (conn->reconnect_max_attempts > 0 && conn->reconnect_attempt_count >= conn->reconnect_max_attempts) {
                ble_manager_connections_unlock(manager);
                ESP_LOGW(TAG, "Max reconnect attempts (%d) reached, disabling auto reconnect", conn->reconnect_max_attempts);
                ble_manager_connections_lock(manager);
                conn->auto_reconnect_enabled = false;
                ble_manager_connections_unlock(manager);
                // Delete connection node
                ble_manager_remove_connection_node(manager, conn);
                // Process next entry starting from NULL (node was deleted)
                ble_manager_reconnect_queue_process_next_from(manager, NULL);
            } else {
                // Add back to queue for retry (set state to RECONNECT_QUEUED)
                conn->state = BLE_CONN_STATE_RECONNECT_QUEUED;
                ble_manager_connections_unlock(manager);
                // Process next entry starting from this failed node
                ble_manager_reconnect_queue_process_next_from(manager, conn);
            }
        } else {
            ble_manager_connections_unlock(manager);
            // Auto-reconnect disabled, set state to DISCONNECTED
            ESP_LOGI(TAG, "Auto-reconnect disabled, setting state to DISCONNECTED");
            // Process next entry starting from this node
            ble_manager_reconnect_queue_process_next_from(manager, conn);
        }
    }
    // If connection attempt succeeded, state will be updated to CONNECTED in BLE_GAP_EVENT_CONNECT event
}

// Process next device in reconnect queue
static void ble_manager_reconnect_queue_process_next(ble_manager_t *manager)
{
    // Find current connecting node (if any) and start from there
    // A node is truly connecting only if state is CONNECTING (not RECONNECT_QUEUED)
    ble_connection_t *current_connecting = NULL;
    ble_manager_connections_lock(manager);
    SLIST_FOREACH(current_connecting, &manager->connections, next) {
        if (current_connecting->state == BLE_CONN_STATE_CONNECTING && current_connecting->auto_reconnect_enabled) {
            break;
        }
    }
    ble_manager_connections_unlock(manager);

    // Process from current connecting node (or NULL if none)
    ble_manager_reconnect_queue_process_next_from(manager, current_connecting);
}

// Reconnect timer callback function
static void ble_manager_reconnect_timer_callback(TimerHandle_t xTimer)
{
    reconnect_timer_params_t *params = (reconnect_timer_params_t *)pvTimerGetTimerID(xTimer);
    if (!params || !params->manager) {
        ESP_LOGE(TAG, "Invalid reconnect timer parameters");
        return;
    }

    ble_manager_t *manager = params->manager;
    ble_addr_t peer_addr = params->peer_addr;
    ESP_LOGW(TAG, "Reconnect timer callback, timer stopped for device %s", addr_str(peer_addr.val));

    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_addr_locked(manager, peer_addr.val, peer_addr.type);
    if (!conn) {
        ble_manager_connections_unlock(manager);
        ESP_LOGW(TAG, "Connection node not found for reconnect, timer stopped");
        free(params);
        return;
    }

    TimerHandle_t reconnect_timer = conn->reconnect_timer;
    conn->reconnect_timer = NULL;

    // Check if auto-reconnect is still enabled
    if (!conn->auto_reconnect_enabled) {
        ble_manager_connections_unlock(manager);
        ESP_LOGI(TAG, "Auto reconnect disabled, reconnect cancelled");
        if (reconnect_timer) {
            xTimerDelete(reconnect_timer, 0);
        }
        free(params);
        return;
    }

    // Check if already connected
    if (conn->state == BLE_CONN_STATE_CONNECTED) {
        ble_manager_connections_unlock(manager);
        ESP_LOGI(TAG, "Already connected, reconnect cancelled");
        if (reconnect_timer) {
            xTimerDelete(reconnect_timer, 0);
        }
        free(params);
        return;
    }

    // Check if max reconnection attempts exceeded
    if (conn->reconnect_max_attempts > 0 && conn->reconnect_attempt_count >= conn->reconnect_max_attempts) {
        conn->auto_reconnect_enabled = false;
        ble_manager_connections_unlock(manager);
        ESP_LOGW(TAG, "Max reconnect attempts (%d) reached, disabling auto reconnect", conn->reconnect_max_attempts);
        if (reconnect_timer) {
            xTimerDelete(reconnect_timer, 0);
        }
        // Max reconnection attempts reached, delete connection node
        ble_manager_remove_connection_node_by_addr(manager, peer_addr.val, peer_addr.type);
        free(params);
        return;
    }

    // We'll use queue mechanism instead.
    if (reconnect_timer) {
        xTimerDelete(reconnect_timer, 0);
    }
    free(params);

    // Mark connection as queued for reconnect
    if (conn->state != BLE_CONN_STATE_CONNECTING && conn->state != BLE_CONN_STATE_CONNECTED) {
        conn->state = BLE_CONN_STATE_RECONNECT_QUEUED;
    }
    conn->conn_handle = BLE_CONN_HANDLE_INVALID; // Reset conn_handle
    ble_manager_connections_unlock(manager);
    ESP_LOGI(TAG, "Device %s added to reconnect queue, processing queue", addr_str(peer_addr.val));

    // Process queue
    ble_manager_reconnect_queue_process_next(manager);
}

static esp_err_t ble_manager_trigger_reconnect_by_connection(ble_manager_t *manager, ble_connection_t *conn)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if auto-reconnect is enabled
    if (!conn->auto_reconnect_enabled) {
        ESP_LOGD(TAG, "Auto reconnect not enabled for device %s", addr_str(conn->peer_addr.val));
        return ESP_ERR_INVALID_STATE;
    }

    // Check if already connected
    if (conn->state == BLE_CONN_STATE_CONNECTED) {
        ESP_LOGD(TAG, "Already connected to device %s", addr_str(conn->peer_addr.val));
        return ESP_OK;
    }

    // Check if max reconnection attempts exceeded
    if (conn->reconnect_max_attempts > 0 && conn->reconnect_attempt_count >= conn->reconnect_max_attempts) {
        ESP_LOGW(TAG, "Max reconnect attempts (%d) reached", conn->reconnect_max_attempts);
        conn->auto_reconnect_enabled = false;
        conn->state = BLE_CONN_STATE_DISCONNECTED;
        return ESP_ERR_INVALID_STATE;
    }

    // If reconnect timer already exists, stop and delete it first
    if (conn->reconnect_timer) {
        reconnect_timer_params_t *old_params = (reconnect_timer_params_t *)pvTimerGetTimerID(conn->reconnect_timer);
        if (old_params) {
            free(old_params);
        }
        xTimerDelete(conn->reconnect_timer, 0);
        conn->reconnect_timer = NULL;
    }

    // Create reconnect timer parameters
    reconnect_timer_params_t *params = malloc(sizeof(reconnect_timer_params_t));
    if (!params) {
        ESP_LOGE(TAG, "Failed to allocate reconnect timer parameters");
        return ESP_ERR_NO_MEM;
    }

    params->manager = manager;
    memcpy(&params->peer_addr, &conn->peer_addr, sizeof(ble_addr_t));

    // Create reconnect timer (one-shot)
    conn->reconnect_timer = xTimerCreate("ble_reconnect", pdMS_TO_TICKS(conn->reconnect_interval_ms),
                                         pdFALSE, params, ble_manager_reconnect_timer_callback);
    if (!conn->reconnect_timer) {
        ESP_LOGE(TAG, "Failed to create reconnect timer");
        free(params);
        return ESP_ERR_NO_MEM;
    }

    // Start timer
    if (xTimerStart(conn->reconnect_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start reconnect timer");
        free(params);
        xTimerDelete(conn->reconnect_timer, 0);
        conn->reconnect_timer = NULL;
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Reconnect timer started for device %s, interval=%lu ms", addr_str(conn->peer_addr.val), conn->reconnect_interval_ms);
    return ESP_OK;
}

static esp_err_t ble_manager_trigger_reconnect_by_handle(ble_manager_t *manager, uint16_t conn_handle)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(manager, conn_handle);
    if (!conn) {
        ble_manager_connections_unlock(manager);
        // If connection doesn't exist, try to find by address (connection may be disconnected but node still exists)
        // Note: after disconnection, conn_handle may be invalid, need to find by address
        ESP_LOGW(TAG, "Connection handle %d not found for reconnect", conn_handle);
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = ble_manager_trigger_reconnect_by_connection(manager, conn);
    ble_manager_connections_unlock(manager);
    return ret;
}

static ble_connection_t *ble_manager_allocate_connection(ble_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return NULL;
    }

    // Check if max connection limit exceeded
    uint8_t count = ble_manager_get_active_connections(manager);
    if (count >= CONFIG_BLE_MANAGER_MAX_CONNECTIONS) {
        ESP_LOGE(TAG, "Maximum connections reached: %d", CONFIG_BLE_MANAGER_MAX_CONNECTIONS);
        return NULL;
    }

    // Allocate new connection node
    ble_connection_t *conn = calloc(1, sizeof(ble_connection_t));
    if (!conn) {
        ESP_LOGE(TAG, "Failed to allocate connection");
        return NULL;
    }

    // Initialize connection info
    conn->is_first_connect = true; // Default is first connection
    conn->auto_reconnect_enabled = false;
    conn->reconnect_interval_ms = 2000; // Default 2 seconds
    conn->reconnect_max_attempts = 0; // Default unlimited reconnection
    conn->reconnect_attempt_count = 0;
    conn->reconnect_timer = NULL; // Initialize reconnect timer

    // Add to list head
    ble_manager_connections_lock(manager);
    SLIST_INSERT_HEAD(&manager->connections, conn, next);
    ble_manager_connections_unlock(manager);

    return conn;
}

static int ble_manager_gap_event(struct ble_gap_event *event, void *arg)
{
    int rc;
    ESP_LOGD(TAG, "BLE gap event: %d", event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        /* Handle scan results */
        if (g_ble_manager && g_ble_manager->callbacks.ble_scan_cb) {
            g_ble_manager->callbacks.ble_scan_cb(g_ble_manager, event->type, &event->disc, g_ble_manager->callbacks.arg);
        }
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "discovery complete; reason=%d\n", event->disc_complete.reason);
        if (g_ble_manager &&
                (event->disc_complete.reason == 0 ||
                 event->disc_complete.reason == BLE_HS_ETIMEOUT)) {
            g_ble_manager->scanning = false;
            ESP_LOGI(TAG, "scan stopped");
        }
        /* Handle scan results */
        if (g_ble_manager && g_ble_manager->callbacks.ble_scan_cb) {
            g_ble_manager->callbacks.ble_scan_cb(g_ble_manager, event->type, NULL, g_ble_manager->callbacks.arg);
        }
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertising complete; reason=%d\n", event->adv_complete.reason);
        // if advertising is stopped, set advertising to false
        if (event->adv_complete.reason == 0 || event->adv_complete.reason == BLE_HS_ETIMEOUT) {
            g_ble_manager->advertising = false;
            ESP_LOGI(TAG, "advertising stopped");
        }
        // call ble_adv_cb callback
        if (g_ble_manager && g_ble_manager->callbacks.ble_adv_cb) {
            g_ble_manager->callbacks.ble_adv_cb(g_ble_manager, event->type, g_ble_manager->callbacks.arg);
        }
        break;

    // case BLE_GAP_EVENT_LINK_ESTAB:

    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Connect event status: %d, conn_handle: %d", event->connect.status, event->connect.conn_handle);
        if (event->connect.status == 0) {
            struct ble_gap_conn_desc desc;
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0) {
                ble_manager_connections_lock(g_ble_manager);
                ble_connection_t *conn = ble_manager_find_connection_by_addr_locked(g_ble_manager, desc.peer_ota_addr.val, desc.peer_ota_addr.type);
                ble_manager_connections_unlock(g_ble_manager);

                // If connected as peripheral (connected during advertising), need to create new connection node
                if (!conn && desc.role == BLE_GAP_ROLE_SLAVE) {
                    conn = ble_manager_allocate_connection(g_ble_manager);
                    if (conn) {
                        memcpy(&conn->peer_addr, &desc.peer_ota_addr, sizeof(ble_addr_t));
                        // Connected as peripheral, this is the first connection
                        conn->is_first_connect = true;
                        // Initialize auto-reconnect related fields
                        conn->auto_reconnect_enabled = false;
                        conn->reconnect_interval_ms = 2000; // Default 2 seconds
                        conn->reconnect_max_attempts = 0; // Default unlimited reconnection
                        conn->reconnect_attempt_count = 0;
                        conn->reconnect_timer = NULL; // Initialize reconnect timer
                    }
                }

                if (conn) {
                    // Connection successful, stop and clean up reconnect timer
                    if (conn->reconnect_timer) {
                        reconnect_timer_params_t *timer_params = (reconnect_timer_params_t *)pvTimerGetTimerID(conn->reconnect_timer);
                        if (timer_params) {
                            free(timer_params);
                        }
                        xTimerDelete(conn->reconnect_timer, 0);
                        conn->reconnect_timer = NULL;
                    }

                    conn->conn_handle = event->connect.conn_handle;
                    conn->role = desc.role;
                    conn->state = BLE_CONN_STATE_CONNECTED;
                    conn->conn_interval = desc.conn_itvl;
                    conn->conn_latency = desc.conn_latency;
                    conn->supervision_timeout = desc.supervision_timeout;
                    conn->connect_timestamp = esp_timer_get_time() / 1000; // Convert to milliseconds
                    memcpy(&conn->peer_addr, &desc.peer_ota_addr, sizeof(ble_addr_t));

                    // Determine if this is a reconnect scenario
                    // Logic:
                    // 1. disconnect_timestamp > 0: indicates previous disconnection, this is a reconnect
                    // 2. reconnect_attempt_count > 0: indicates ongoing reconnect attempt, this is a reconnect
                    // Note: auto_reconnect_enabled cannot be used as a criterion, as it may be enabled on first connection
                    bool is_reconnect = (conn->disconnect_timestamp > 0 || conn->reconnect_attempt_count > 0);

                    // Set is_first_connect based on judgment result
                    if (is_reconnect) {
                        // Reconnect scenario: ensure is_first_connect is false
                        if (conn->is_first_connect != false) {
                            ESP_LOGD(TAG, "Setting is_first_connect=false for reconnect (disconnect_ts=%" PRIu32 ", attempt_count=%d)",
                                     conn->disconnect_timestamp, conn->reconnect_attempt_count);
                            conn->is_first_connect = false;
                        }
                    } else {
                        // First connection scenario: ensure is_first_connect is true
                        // If node is created via calloc but not explicitly set, is_first_connect may be false (calloc initializes to 0)
                        if (conn->is_first_connect != true) {
                            ESP_LOGD(TAG, "Setting is_first_connect=true for first connection");
                            conn->is_first_connect = true;
                        }
                    }

                    ESP_LOGI(TAG, "Connected to device %s (role: %s, %s)", addr_str(conn->peer_addr.val),
                             desc.role == BLE_GAP_ROLE_SLAVE ? "Slave" : "Master", is_reconnect ? "reconnect" : "first time");
                    ESP_LOGI(TAG, "Initial connection conn_handle: %d, interval: %.2fms (units=%d), latency=%d, timeout=%dms",
                             event->connect.conn_handle, desc.conn_itvl * 1.25f, desc.conn_itvl, desc.conn_latency, desc.supervision_timeout * 10);

                    // If connected as peripheral, stop advertising
                    if (desc.role == BLE_GAP_ROLE_SLAVE && g_ble_manager->advertising) {
                        ble_gap_adv_stop();
                        g_ble_manager->advertising = false;
                    }

                    /* Connection successfully established. */
                    /* XXX Set packet length in controller for better throughput */
                    int rc = ble_hs_hci_util_set_data_len(event->connect.conn_handle, LL_PACKET_LENGTH, LL_PACKET_TIME);
                    if (rc != 0) {
                        ESP_LOGE(TAG, "Set packet length failed; rc = %d", rc);
                    }

                    /* Set connection parameters */
                    if (desc.role == BLE_GAP_ROLE_MASTER) {
                        rc = ble_manager_update_mtu(g_ble_manager, event->connect.conn_handle);
                        if (rc != 0) {
                            ESP_LOGE(TAG, "Failed to negotiate MTU; rc = %d", rc);
                        }

                        // Try to switch to 2M PHY for better throughput
                        // This will attempt to use 2M PHY if both devices support it
                        rc = ble_manager_update_phy(g_ble_manager, event->connect.conn_handle);
                        if (rc != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to set PHY to 2M; rc = %d (device may not support 2M PHY)", rc);
                        }

                        // Optimize connection parameters to improve throughput (delay briefly to ensure connection stability)
                        rc = ble_manager_update_connection_params(g_ble_manager, event->connect.conn_handle, conn->min_interval,
                                                                  conn->max_interval, conn->conn_latency, conn->supervision_timeout);
                        if (rc != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to update connection parameters; rc = %d", rc);
                        }
                    }

                    // If reconnect successful, reset reconnect count and disconnect timestamp
                    if (is_reconnect) {
                        conn->reconnect_attempt_count = 0;
                        conn->disconnect_timestamp = 0; // Clear disconnect timestamp for next disconnection
                        ESP_LOGI(TAG, "Reconnected to device %s (attempt count reset)", addr_str(conn->peer_addr.val));
                    } else {
                        // First connection, mark as connected (prepare for reconnect after next disconnection)
                        conn->is_first_connect = false;
                    }

                    // Ensure reconnect timer is cleaned up (double check)
                    if (conn->reconnect_timer) {
                        reconnect_timer_params_t *timer_params = (reconnect_timer_params_t *)pvTimerGetTimerID(conn->reconnect_timer);
                        if (timer_params) {
                            free(timer_params);
                        }
                        xTimerDelete(conn->reconnect_timer, 0);
                        conn->reconnect_timer = NULL;
                    }

                    if (g_ble_manager && g_ble_manager->callbacks.ble_connect_cb) {
                        g_ble_manager->callbacks.ble_connect_cb(g_ble_manager, is_reconnect ? BLE_EVENT_TYPE_RECONNECT_COMPLETE : BLE_EVENT_TYPE_CONNECT_COMPLETE,
                                                                conn, event->connect.status, g_ble_manager->callbacks.arg);
                    }
                }
            }

            // connection update allow one process at a time, so process the next device in reconnect queue after the connection update is complete
        } else {
            // Connection failed
            // Try to find the connection node that was attempting to connect
            // Find the most recently started connection attempt (the one with CONNECTING state that was started most recently)
            int64_t latest_timestamp = 0;
            ble_connection_t *conn = NULL;
            ble_connection_t *temp_conn;

            ble_manager_connections_lock(g_ble_manager);
            SLIST_FOREACH(temp_conn, &g_ble_manager->connections, next) {
                if (temp_conn->state == BLE_CONN_STATE_CONNECTING) {
                    // Find the connection with the most recent connect_timestamp
                    if (temp_conn->connect_timestamp > latest_timestamp) {
                        latest_timestamp = temp_conn->connect_timestamp;
                        conn = temp_conn;
                    }
                }
            }
            ble_manager_connections_unlock(g_ble_manager);

            if (!conn) {
                ESP_LOGW(TAG, "Connection failed but no connection node found, processing queue from beginning");
                // No connection found, process from beginning
                ble_manager_reconnect_queue_process_next_from(g_ble_manager, NULL);
                break;
            }

            ESP_LOGD(TAG, "Connection failed for device %s, state=%d, attempt_count=%d",
                     addr_str(conn->peer_addr.val), conn->state, conn->reconnect_attempt_count);

            // Determine if this is a reconnect scenario
            // Logic:
            // 1. disconnect_timestamp > 0: indicates previous disconnection, this is a reconnect
            // 2. reconnect_attempt_count > 0: indicates ongoing reconnect attempt, this is a reconnect
            // Note: auto_reconnect_enabled cannot be used as a criterion, as it may be enabled on first connection
            bool is_reconnect = (conn->disconnect_timestamp > 0 || conn->reconnect_attempt_count > 0);

            if (g_ble_manager && g_ble_manager->callbacks.ble_connect_cb) {
                g_ble_manager->callbacks.ble_connect_cb(g_ble_manager, is_reconnect ? BLE_EVENT_TYPE_RECONNECT_FAILED : BLE_EVENT_TYPE_CONNECT_FAILED,
                                                        conn, event->connect.status, g_ble_manager->callbacks.arg);
            }

            // Save the failed connection pointer before potentially deleting it
            ble_connection_t *failed_conn = conn;
            conn->state = BLE_CONN_STATE_DISCONNECTED;
            conn->conn_handle = BLE_CONN_HANDLE_INVALID; // Reset conn_handle

            // If reconnect failed and auto-reconnect is still enabled, add back to queue
            if (is_reconnect && conn->auto_reconnect_enabled) {
                // Check if max reconnect attempts exceeded
                if (conn->reconnect_max_attempts > 0 && conn->reconnect_attempt_count >= conn->reconnect_max_attempts) {
                    ESP_LOGW(TAG, "Max reconnect attempts (%d) reached for device %s, disabling auto reconnect",
                             conn->reconnect_max_attempts, addr_str(conn->peer_addr.val));
                    conn->auto_reconnect_enabled = false;
                    // Delete connection node
                    ble_manager_remove_connection_node(g_ble_manager, conn);
                    // Node deleted, set failed_conn to NULL so we start from beginning
                    failed_conn = NULL;
                } else {
                    // Add back to queue for retry (set state to RECONNECT_QUEUED)
                    conn->state = BLE_CONN_STATE_RECONNECT_QUEUED;
                    ESP_LOGI(TAG, "Device %s added back to reconnect queue (attempt %d/%d)", addr_str(conn->peer_addr.val),
                             conn->reconnect_attempt_count, conn->reconnect_max_attempts > 0 ? conn->reconnect_max_attempts : 999);
                }
            } else {
                // Not a reconnect or auto-reconnect disabled, set state to DISCONNECTED
                ESP_LOGI(TAG, "Device %s not reconnecting: is_reconnect=%d, auto_reconnect_enabled=%d",
                         addr_str(conn->peer_addr.val), is_reconnect, conn->auto_reconnect_enabled);
            }

            // Process next device in reconnect queue (pass the failed connection as starting point)
            // This ensures we start searching from the next node after the failed one
            // If failed_conn is NULL (node was deleted), start from beginning
            ESP_LOGD(TAG, "Processing reconnect queue from failed device %s", failed_conn ? addr_str(failed_conn->peer_addr.val) : "NULL");
            ble_manager_reconnect_queue_process_next_from(g_ble_manager, failed_conn);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        if (g_ble_manager) {
            bool found = false;
            bool auto_reconnect_enabled = false;
            bool max_attempt_reached = false;
            uint8_t role = 0;
            ble_connection_t conn_snapshot = {0};

            ble_manager_connections_lock(g_ble_manager);
            ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(g_ble_manager, event->disconnect.conn.conn_handle);
            if (conn) {
                conn->state = BLE_CONN_STATE_DISCONNECTED;
                conn->disconnect_timestamp = esp_timer_get_time() / 1000; // Record disconnect time
                role = conn->role;
                auto_reconnect_enabled = conn->auto_reconnect_enabled;
                max_attempt_reached = (conn->reconnect_max_attempts > 0 &&
                                       conn->reconnect_attempt_count >= conn->reconnect_max_attempts);
                if (max_attempt_reached) {
                    conn->auto_reconnect_enabled = false;
                    auto_reconnect_enabled = false;
                }
                conn_snapshot = *conn;
                found = true;
            }
            ble_manager_connections_unlock(g_ble_manager);

            if (found) {
                ESP_LOGI(TAG, "Disconnected from device %s, reason: %d", addr_str(conn_snapshot.peer_addr.val), event->disconnect.reason);

                if (g_ble_manager->callbacks.ble_disconnect_cb) {
                    g_ble_manager->callbacks.ble_disconnect_cb(g_ble_manager, &conn_snapshot, event->disconnect.reason, g_ble_manager->callbacks.arg);
                }

                // Determine behavior after disconnection based on role
                if (role == BLE_GAP_ROLE_MASTER) {
                    if (auto_reconnect_enabled) {
                        ble_manager_trigger_reconnect_by_handle(g_ble_manager, event->disconnect.conn.conn_handle);
                    } else {
                        if (max_attempt_reached) {
                            ESP_LOGW(TAG, "Auto reconnect disabled: max attempts reached");
                        }
                        ble_manager_remove_connection_node_by_handle(g_ble_manager, event->disconnect.conn.conn_handle);
                    }
                } else if (role == BLE_GAP_ROLE_SLAVE) {
                    // Slave role: restart advertising
                    ESP_LOGI(TAG, "Slave disconnected, restarting advertising");
                    ble_manager_remove_connection_node_by_handle(g_ble_manager, event->disconnect.conn.conn_handle);

                    // Restart advertising (using saved advertising parameters)
                    if (!g_ble_manager->advertising) {
                        struct ble_gap_adv_params saved_params = g_ble_manager->saved_adv_params;
                        esp_err_t ret = ble_manager_start_advertising(g_ble_manager,
                                                                      &saved_params, g_ble_manager->saved_adv_duration_ms);
                        if (ret != ESP_OK) {
                            ESP_LOGE(TAG, "Failed to restart advertising after disconnect: %d", ret);
                        } else {
                            ESP_LOGI(TAG, "Advertising restarted successfully");
                        }
                    }
                } else {
                    // Unknown role, delete connection node
                    ble_manager_remove_connection_node_by_handle(g_ble_manager, event->disconnect.conn.conn_handle);
                }
            }
        }
        break;

    case BLE_GAP_EVENT_CONN_UPDATE: {
        // Connection parameters update complete, use ble_gap_conn_find to get updated connection parameters
        struct ble_gap_conn_desc desc;
        int rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        ble_connection_t conn_snapshot = {0};
        bool found = false;
        if (rc == 0) {
            ble_manager_connections_lock(g_ble_manager);
            ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(g_ble_manager, event->conn_update.conn_handle);
            if (conn) {
                conn->conn_interval = desc.conn_itvl;
                conn->conn_latency = desc.conn_latency;
                conn->supervision_timeout = desc.supervision_timeout;
                conn_snapshot = *conn;
                found = true;
            }
            ble_manager_connections_unlock(g_ble_manager);

            if (found) {
                ESP_LOGW(TAG, "Connection parameters updated: conn_handle=%d, interval=%.2fms (units=%d), latency=%d, timeout=%dms",
                         event->conn_update.conn_handle, desc.conn_itvl * 1.25f, desc.conn_itvl, desc.conn_latency, desc.supervision_timeout * 10);
            } else {
                ESP_LOGW(TAG, "Connection parameters updated for new conn_handle=%d, interval=%.2fms (units=%d), latency=%d, timeout=%dms",
                         event->conn_update.conn_handle, desc.conn_itvl * 1.25f, desc.conn_itvl, desc.conn_latency, desc.supervision_timeout * 10);
            }
        } else {
            ESP_LOGW(TAG, "Failed to get connection info for conn_handle=%d: rc=%d", event->conn_update.conn_handle, rc);
        }

        if (g_ble_manager && g_ble_manager->callbacks.ble_conn_update_cb && found) {
            g_ble_manager->callbacks.ble_conn_update_cb(g_ble_manager, &conn_snapshot, g_ble_manager->callbacks.arg);
        }

        // connection update allow one process at a time, so process the next device in reconnect queue after the connection update is complete
        // Process next device in reconnect queue (state is already CONNECTED, so it won't be processed)
        ble_manager_reconnect_queue_process_next(g_ble_manager);
        break;
    }

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        ESP_LOGD(TAG, "Connection parameters update request");
        break;

    case BLE_GAP_EVENT_MTU: {
        ESP_LOGW(TAG, "MTU updated: %d", event->mtu.value);
        // Update MTU in connection info
        ble_manager_connections_lock(g_ble_manager);
        ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(g_ble_manager, event->mtu.conn_handle);
        if (conn) {
            conn->mtu = event->mtu.value;
            ESP_LOGI(TAG, "Updated MTU for connection %d: %d", event->mtu.conn_handle, event->mtu.value);
        }
        ble_manager_connections_unlock(g_ble_manager);
        break;
    }

    case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE: {
        ESP_LOGW(TAG, "PHY update complete: conn_handle=%d, tx_phy=%d, rx_phy=%d, status=%d",
                 event->phy_updated.conn_handle, event->phy_updated.tx_phy, event->phy_updated.rx_phy, event->phy_updated.status);

        ble_manager_connections_lock(g_ble_manager);
        ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(g_ble_manager, event->phy_updated.conn_handle);
        if (conn) {
            if (event->phy_updated.status == 0) {
                // PHY update successful
                // BLE_HCI_LE_PHY_1M = 1, BLE_HCI_LE_PHY_2M = 2, BLE_HCI_LE_PHY_CODED = 3
                if (event->phy_updated.tx_phy == BLE_HCI_LE_PHY_2M || event->phy_updated.rx_phy == BLE_HCI_LE_PHY_2M) {
                    ESP_LOGI(TAG, "Successfully switched to 2M PHY for connection %d (tx_phy=%d, rx_phy=%d)",
                             event->phy_updated.conn_handle, event->phy_updated.tx_phy, event->phy_updated.rx_phy);
                } else {
                    ESP_LOGI(TAG, "PHY update completed but using 1M PHY (tx_phy=%d, rx_phy=%d) - device may not support 2M PHY",
                             event->phy_updated.tx_phy, event->phy_updated.rx_phy);
                }
            } else {
                ESP_LOGW(TAG, "PHY update failed: status=%d", event->phy_updated.status);
            }
        }
        ble_manager_connections_unlock(g_ble_manager);
        break;
    }

    default:
        break;
    }

    return 0;
}

esp_err_t ble_manager_start_scan(ble_manager_t *manager, uint32_t duration_ms)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }

    if (manager->scanning) {
        ESP_LOGW(TAG, "Scan already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    struct ble_gap_disc_params disc_params = {0};
    disc_params.itvl = BLE_HCI_SCAN_ITVL_DEF;
    disc_params.window = BLE_HCI_SCAN_WINDOW_DEF;
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;

    uint32_t scan_duration = duration_ms > 0 ? duration_ms : BLE_HS_FOREVER;
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, scan_duration, &disc_params, ble_manager_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start scan: %d", rc);
        return ESP_FAIL;
    }

    manager->scanning = true;

    ESP_LOGI(TAG, "BLE scan started");
    return ESP_OK;
}

esp_err_t ble_manager_stop_scan(ble_manager_t *manager)
{
    if (!manager) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!manager->scanning) {
        return ESP_OK;
    }

    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop scan: %d", rc);
        return ESP_FAIL;
    }

    manager->scanning = false;

    ESP_LOGI(TAG, "BLE scan stopped");
    return ESP_OK;
}

esp_err_t ble_manager_start_advertising(ble_manager_t *manager, const struct ble_gap_adv_params *adv_params, uint32_t duration_ms)
{
    if (!manager || !manager->initialized || !adv_params) {
        return ESP_ERR_INVALID_ARG;
    }

    if (manager->advertising) {
        ESP_LOGW(TAG, "Advertising already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    int rc;

    // Save advertising parameters for restarting advertising after disconnection
    memcpy(&manager->saved_adv_params, adv_params, sizeof(struct ble_gap_adv_params));
    manager->saved_adv_duration_ms = duration_ms;

    // Start advertising (application layer needs to call ble_gap_adv_set_fields first to set advertising data)
    uint32_t adv_duration = duration_ms > 0 ? duration_ms : BLE_HS_FOREVER;
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, adv_duration, adv_params, ble_manager_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
        return ESP_FAIL;
    }

    manager->advertising = true;

    ESP_LOGI(TAG, "BLE advertising started");
    return ESP_OK;
}

esp_err_t ble_manager_stop_advertising(ble_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!manager->advertising) {
        return ESP_OK;
    }

    int rc = ble_gap_adv_stop();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to stop advertising; rc=%d", rc);
        return ESP_FAIL;
    }

    manager->advertising = false;

    ESP_LOGI(TAG, "BLE advertising stopped");
    return ESP_OK;
}

esp_err_t ble_manager_connect(ble_manager_t *manager, const ble_addr_t *addr, uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout)
{
    int rc;
    uint8_t own_addr_type;

    if (!manager || !manager->initialized || !addr) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check if there's already a connection attempt in progress
    // If this is a queued reconnect, the connection node will have state CONNECTING
    // For direct user calls, if there's already a connection in CONNECTING state, we should reject
    bool is_queued_reconnect = false;
    ble_manager_connections_lock(manager);
    ble_connection_t *existing_conn = ble_manager_find_connection_by_addr_locked(manager, addr->val, addr->type);
    if (existing_conn && existing_conn->state == BLE_CONN_STATE_CONNECTING && existing_conn->auto_reconnect_enabled) {
        is_queued_reconnect = true;
    }
    ble_manager_connections_unlock(manager);

    if (!is_queued_reconnect && ble_manager_is_connecting_in_progress(manager)) {
        ESP_LOGW(TAG, "Connection attempt already in progress, cannot start new connection");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if already connected to this device
    if (ble_manager_is_connected(manager, addr)) {
        ESP_LOGW(TAG, "Already connected to device");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if connection node for this address already exists (may be used for reconnect)
    uint16_t gap_arg_conn_handle = BLE_CONN_HANDLE_INVALID;
    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_addr_locked(manager, addr->val, addr->type);
    if (conn) {
        // Node exists, check state
        if (conn->state == BLE_CONN_STATE_CONNECTED) {
            ble_manager_connections_unlock(manager);
            ESP_LOGW(TAG, "Connection node already exists and is connected");
            return ESP_ERR_INVALID_STATE;
        }

        // Reuse existing node (for reconnect scenario)
        // Keep current value of is_first_connect (reconnect task has set it to false)
        ESP_LOGI(TAG, "Reusing existing connection node for reconnect");
        conn->state = BLE_CONN_STATE_CONNECTING;
        conn->connect_timestamp = esp_timer_get_time() / 1000; // Update connection timestamp
        // Keep other reconnect-related fields unchanged
        gap_arg_conn_handle = conn->conn_handle;
        ble_manager_connections_unlock(manager);
    } else {
        ble_manager_connections_unlock(manager);
        // Node doesn't exist, allocate new connection node
        conn = ble_manager_allocate_connection(manager);
        if (!conn) {
            ESP_LOGE(TAG, "Failed to allocate connection");
            return ESP_ERR_NO_MEM;
        }

        ble_manager_connections_lock(manager);
        ESP_LOGI(TAG, "Allocating new connection node for device %s", addr_str(addr->val));
        // Initialize connection info
        conn->state = BLE_CONN_STATE_CONNECTING;
        memcpy(&conn->peer_addr, addr, sizeof(ble_addr_t));
        conn->connect_timestamp = esp_timer_get_time() / 1000; // Convert to milliseconds
        conn->mtu = OPTIMAL_MTU_SIZE; // optimal MTU size
        // Initialize auto-reconnect related fields (first connection)
        conn->is_first_connect = true;
        conn->auto_reconnect_enabled = false;
        conn->reconnect_interval_ms = 2000; // Default 2 seconds
        conn->reconnect_max_attempts = 0; // Default unlimited reconnection
        conn->reconnect_attempt_count = 0;
        conn->disconnect_timestamp = 0;
        conn->min_interval = min_interval;
        conn->max_interval = max_interval;
        conn->conn_latency = latency;
        conn->supervision_timeout = timeout;
        conn->conn_handle = BLE_CONN_HANDLE_INVALID;
        gap_arg_conn_handle = conn->conn_handle;
        ble_manager_connections_unlock(manager);
    }

#if !(MYNEWT_VAL(BLE_HOST_ALLOW_CONNECT_WITH_SCAN))
    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Failed to cancel scan; rc=%d\n", rc);
        return ESP_FAIL;
    }
    g_ble_manager->scanning = false;
#endif

    /* Figure out address to use for connect (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return ESP_FAIL;
    }

    // Here should call NimBLE connection function
    // Use uintptr_t to safely convert conn_handle (uint16_t) to void* to avoid int-to-pointer-cast warning
    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL, ble_manager_gap_event, (void *)(uintptr_t)gap_arg_conn_handle);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error: Failed to connect to device; addr_type=%d "
                 "addr=%s; rc=%d\n", addr->type, addr_str(addr->val), rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Initiating connection to device");
    return ESP_OK;
}

esp_err_t ble_manager_disconnect(ble_manager_t *manager, uint16_t conn_handle)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_connection_t conn_snapshot = {0};
    bool found = false;
    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(manager, conn_handle);
    if (conn) {
        conn->auto_reconnect_enabled = false;
        conn->state = BLE_CONN_STATE_DISCONNECTED;
        conn->disconnect_timestamp = esp_timer_get_time() / 1000;
        conn_snapshot = *conn;
        found = true;
    }
    ble_manager_connections_unlock(manager);

    if (!found) {
        ESP_LOGW(TAG, "Connection not found: %d", conn_handle);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Disconnecting connection: %d", conn_handle);
    ESP_LOGI(TAG, "Auto reconnect disabled due to manual disconnect");

    // Here should call NimBLE disconnect function
    ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);

    // Call disconnect callback
    if (manager->callbacks.ble_disconnect_cb) {
        manager->callbacks.ble_disconnect_cb(manager, &conn_snapshot, BLE_ERR_REM_USER_CONN_TERM, manager->callbacks.arg);
    }

    // Remove and free connection node from list
    // Note: when user initiates disconnect, delete connection node regardless of auto-reconnect status
    // Auto-reconnect only triggers on passive disconnect (BLE_GAP_EVENT_DISCONNECT event)
    ble_manager_remove_connection_node_by_handle(manager, conn_handle);

    return ESP_OK;
}

esp_err_t ble_manager_disconnect_all(ble_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Disconnecting all connections");

    // Snapshot handles first to avoid iterating a mutating list.
    uint16_t handles[CONFIG_BLE_MANAGER_MAX_CONNECTIONS];
    uint8_t handle_count = 0;
    ble_connection_t *conn;
    ble_manager_connections_lock(manager);
    SLIST_FOREACH(conn, &manager->connections, next) {
        if (conn->state != BLE_CONN_STATE_DISCONNECTED &&
                handle_count < CONFIG_BLE_MANAGER_MAX_CONNECTIONS) {
            handles[handle_count++] = conn->conn_handle;
        }
    }
    ble_manager_connections_unlock(manager);

    for (uint8_t i = 0; i < handle_count; i++) {
        ble_manager_disconnect(manager, handles[i]);
    }

    return ESP_OK;
}

uint8_t ble_manager_get_active_connections(ble_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return 0;
    }

    uint8_t count = 0;
    ble_connection_t *conn;
    ble_manager_connections_lock(manager);
    SLIST_FOREACH(conn, &manager->connections, next) {
        if (conn->state == BLE_CONN_STATE_CONNECTED) {
            count++;
        }
    }
    ble_manager_connections_unlock(manager);

    return count;
}

uint8_t ble_manager_get_total_connections(ble_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return 0;
    }

    uint8_t count = 0;
    ble_connection_t *conn;
    ble_manager_connections_lock(manager);
    SLIST_FOREACH(conn, &manager->connections, next) {
        count++;
    }
    ble_manager_connections_unlock(manager);

    return count;
}

bool ble_manager_get_connection_snapshot_by_handle(ble_manager_t *manager, uint16_t conn_handle, ble_connection_t *out_conn)
{
    if (!manager || !manager->initialized || !out_conn) {
        return false;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(manager, conn_handle);
    if (!conn) {
        ble_manager_connections_unlock(manager);
        return false;
    }
    *out_conn = *conn;
    ble_manager_connections_unlock(manager);
    return true;
}

bool ble_manager_get_connection_snapshot_by_addr(ble_manager_t *manager, const uint8_t *addr, uint8_t addr_type, ble_connection_t *out_conn)
{
    if (!manager || !manager->initialized || !addr || !out_conn) {
        return false;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_addr_locked(manager, addr, addr_type);
    if (!conn) {
        ble_manager_connections_unlock(manager);
        return false;
    }
    *out_conn = *conn;
    ble_manager_connections_unlock(manager);
    return true;
}

bool ble_manager_is_connected(ble_manager_t *manager, const ble_addr_t *addr)
{
    if (!manager || !manager->initialized || !addr) {
        return false;
    }

    ble_connection_t *conn;
    ble_manager_connections_lock(manager);
    SLIST_FOREACH(conn, &manager->connections, next) {
        if (conn->state == BLE_CONN_STATE_CONNECTED) {
            if (memcmp(&conn->peer_addr, addr, sizeof(ble_addr_t)) == 0) {
                ble_manager_connections_unlock(manager);
                return true;
            }
        }
    }
    ble_manager_connections_unlock(manager);

    return false;
}

esp_err_t ble_manager_update_rssi(ble_manager_t *manager, uint16_t conn_handle, int8_t rssi)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(manager, conn_handle);
    if (!conn) {
        ble_manager_connections_unlock(manager);
        return ESP_ERR_NOT_FOUND;
    }

    conn->rssi = rssi;
    ble_manager_connections_unlock(manager);
    ESP_LOGD(TAG, "Updated RSSI for connection %d: %d dBm", conn_handle, rssi);

    return ESP_OK;
}

esp_err_t ble_manager_get_mtu_by_conn(ble_manager_t *manager, uint16_t conn_handle, uint16_t *mtu)
{
    if (!manager || !manager->initialized || !mtu) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_connection_t conn_snapshot = {0};
    if (!ble_manager_get_connection_snapshot_by_handle(manager, conn_handle, &conn_snapshot)) {
        return ESP_ERR_NOT_FOUND;
    }

    *mtu = conn_snapshot.mtu;
    return ESP_OK;
}

esp_err_t ble_manager_update_mtu(ble_manager_t *manager, uint16_t conn_handle)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_connection_t conn_snapshot = {0};
    if (!ble_manager_get_connection_snapshot_by_handle(manager, conn_handle, &conn_snapshot)) {
        return ESP_ERR_NOT_FOUND;
    }

    int rc = ble_gattc_exchange_mtu(conn_handle, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to negotiate MTU; rc = %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ble_manager_update_connection_params(ble_manager_t *manager, uint16_t conn_handle,
                                               uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_connection_t conn_snapshot = {0};
    if (!ble_manager_get_connection_snapshot_by_handle(manager, conn_handle, &conn_snapshot)) {
        return ESP_ERR_NOT_FOUND;
    }

    // Validate parameter range
    // BLE specification: connection interval range 6-3200 (7.5ms - 4000ms)
    if (min_interval < 6 || min_interval > 3200 || max_interval < 6 || max_interval > 3200 || min_interval > max_interval) {
        ESP_LOGE(TAG, "Invalid connection interval parameters: min=%d, max=%d", min_interval, max_interval);
        return ESP_ERR_INVALID_ARG;
    }

    // BLE specification: latency range 0-499
    if (latency > 499) {
        ESP_LOGE(TAG, "Invalid latency parameter: %d", latency);
        return ESP_ERR_INVALID_ARG;
    }

    // BLE specification: supervision timeout range 10-3200 (100ms - 32000ms)
    if (timeout < 10 || timeout > 3200) {
        ESP_LOGE(TAG, "Invalid supervision timeout parameter: %d", timeout);
        return ESP_ERR_INVALID_ARG;
    }

    // Optimal connection parameter configuration (optimized for high throughput)
    // min_interval = 6  (7.5ms)  - Minimum connection interval, increase data transfer frequency
    // max_interval = 12 (15ms)   - Maximum connection interval, balance power consumption and performance
    // latency = 0                - Slave latency, no delay, immediate response
    // timeout = 30 (300ms)       - Supervision timeout, ensure connection stability
    struct ble_gap_upd_params params = {
        .itvl_min = min_interval,
        .itvl_max = max_interval,
        .latency = latency,
        .supervision_timeout = timeout,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };

    ESP_LOGI(TAG, "Requested connection parameters update: conn_handle=%d, interval=%.2f-%.2fms (units=%d-%d), latency=%d, timeout=%dms",
             conn_handle, min_interval * 1.25f, max_interval * 1.25f, min_interval, max_interval, latency, timeout * 10);

    int rc = ble_gap_update_params(conn_handle, &params);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to update connection parameters; rc = %d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ble_manager_enable_auto_reconnect(ble_manager_t *manager, uint16_t conn_handle, uint32_t reconnect_interval_ms, uint8_t max_attempts)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(manager, conn_handle);
    if (!conn) {
        ble_manager_connections_unlock(manager);
        // If connection doesn't exist, try to find by address (connection may be disconnected but node still exists)
        ESP_LOGW(TAG, "Connection handle %d not found", conn_handle);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t peer_addr[BLE_DEV_ADDR_LEN];
    memcpy(peer_addr, conn->peer_addr.val, BLE_DEV_ADDR_LEN);
    conn->auto_reconnect_enabled = true;
    conn->reconnect_interval_ms = reconnect_interval_ms > 0 ? reconnect_interval_ms : 2000; // Default 2 seconds
    conn->reconnect_max_attempts = max_attempts;
    conn->reconnect_attempt_count = 0;
    uint32_t applied_interval = conn->reconnect_interval_ms;
    ble_manager_connections_unlock(manager);

    ESP_LOGI(TAG, "Auto reconnect enabled for device %s: interval=%" PRIu32 " ms, max_attempts=%d",
             addr_str(peer_addr), applied_interval, max_attempts);

    return ESP_OK;
}

esp_err_t ble_manager_disable_auto_reconnect(ble_manager_t *manager, uint16_t conn_handle)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_manager_connections_lock(manager);
    ble_connection_t *conn = ble_manager_find_connection_by_handle_locked(manager, conn_handle);
    if (!conn) {
        ble_manager_connections_unlock(manager);
        // If connection doesn't exist, try to find by address (connection may be disconnected but node still exists)
        ESP_LOGW(TAG, "Connection handle %d not found", conn_handle);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t peer_addr[BLE_DEV_ADDR_LEN];
    memcpy(peer_addr, conn->peer_addr.val, BLE_DEV_ADDR_LEN);
    conn->auto_reconnect_enabled = false;
    conn->reconnect_attempt_count = 0;
    TimerHandle_t reconnect_timer = conn->reconnect_timer;
    conn->reconnect_timer = NULL;
    ble_manager_connections_unlock(manager);

    // Stop and clean up reconnection timer
    if (reconnect_timer) {
        reconnect_timer_params_t *timer_params = (reconnect_timer_params_t *)pvTimerGetTimerID(reconnect_timer);
        if (timer_params) {
            free(timer_params);
        }
        xTimerDelete(reconnect_timer, 0);
    }

    ESP_LOGI(TAG, "Auto reconnect disabled for device %s", addr_str(peer_addr));

    return ESP_OK;
}

esp_err_t ble_manager_update_phy(ble_manager_t *manager, uint16_t conn_handle)
{
    if (!manager || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ble_connection_t conn_snapshot = {0};
    if (!ble_manager_get_connection_snapshot_by_handle(manager, conn_handle, &conn_snapshot)) {
        return ESP_ERR_NOT_FOUND;
    }

    // Set preferred PHY to 2M for both TX and RX directions
    // BLE_HCI_LE_PHY_2M_PREF_MASK = 0x02 (prefer 2M PHY)
    // fallback to 1M PHY if 2M not supported
    // The third parameter (phy_options) is set to 0 (no preference for coded PHY)
    uint8_t tx_phys = BLE_HCI_LE_PHY_2M_PREF_MASK;
    uint8_t rx_phys = BLE_HCI_LE_PHY_2M_PREF_MASK;
    int rc = ble_gap_set_prefered_le_phy(conn_handle, tx_phys, rx_phys, 0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set PHY preference; rc = %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Requested PHY update to 2M for connection %d", conn_handle);
    return ESP_OK;
}
