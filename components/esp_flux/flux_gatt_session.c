/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief BLE adapter for esp_flux reliable transport protocol.
 *
 * This file is a thin wrapper that bridges BLE (NimBLE) transport with
 * the transport-agnostic esp_flux protocol engine. It implements:
 * 1. BLE transport ops (send via GATT write/notify)
 * 2. GAP event bridging (notification RX → flux_session_feed_data)
 * 3. GATT handler bridging (write RX → flux_session_feed_data)
 * 4. Session lifecycle management with BLE-specific fields
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "os/os_mempool.h"
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "flux_gatt_session.h"

static const char *TAG = "gatt_session";

/* On SOC_ESP_NIMBLE_CONTROLLER builds, IDF does not link os_mempool.c into libbt. */
struct os_mempool *os_mempool_info_get_next(struct os_mempool *mp, struct os_mempool_info *omi)
__attribute__((weak));

static struct ble_gap_event_listener g_gatt_session_gap_event_listener;
static bool g_gap_listener_registered = false;
static struct gatt_session_list g_sessions_list = SLIST_HEAD_INITIALIZER(g_sessions_list);
static SemaphoreHandle_t g_gatt_sessions_mutex = NULL;
typedef struct {
    struct ble_npl_event ev;
    gatt_session_t *session;
} gatt_session_destroy_event_t;

static void gatt_session_destroy_event_fn(struct ble_npl_event *ev)
{
    gatt_session_destroy_event_t *event = (gatt_session_destroy_event_t *)ev;
    if (!event) {
        return;
    }
    (void)gatt_session_destroy(event->session);
    ble_npl_event_deinit(ev);
    free(event);
}

static bool gatt_session_callback_pin_locked(gatt_session_t *session)
{
    if (!session || session->destroying) {
        return false;
    }
    session->callback_inflight++;
    return true;
}

static void gatt_session_callback_leave(gatt_session_t *session)
{
    if (!session) {
        return;
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    }
    if (session->callback_inflight > 0) {
        session->callback_inflight--;
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreGive(g_gatt_sessions_mutex);
    }
}

static gatt_session_t *gatt_session_find_by_handle_pinned(uint16_t conn_handle)
{
    gatt_session_t *target = NULL;
    if (g_gatt_sessions_mutex) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    }
    gatt_session_t *session;
    SLIST_FOREACH(session, &g_sessions_list, next) {
        if (session->conn_handle == conn_handle && gatt_session_callback_pin_locked(session)) {
            target = session;
            break;
        }
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreGive(g_gatt_sessions_mutex);
    }
    return target;
}

/* ────────────────────── BLE Transport Ops ────────────────────── */
static bool msys_pool_info(const char *name, struct os_mempool_info *out)
{
    if (os_mempool_info_get_next == NULL) {
        return false;
    }

    struct os_mempool *mp = NULL;
    struct os_mempool_info info;

    while ((mp = os_mempool_info_get_next(mp, &info)) != NULL) {
        if (strcmp(info.omi_name, name) == 0) {
            *out = info;
            return true;
        }
    }
    return false;
}

int ble_hs_l2cap_pkt_quota_get(void)
{
    if (os_mempool_info_get_next == NULL) {
        return GATT_SESSION_MAX_MBUF_QUOTA;
    }

    struct os_mempool_info info;

    /* ble_hs_mbuf_l2cap_pkt() alloc from msys_1 pool */
    if (!msys_pool_info("msys_1", &info)) {
        return GATT_SESSION_MAX_MBUF_QUOTA;
    }

    return info.omi_num_free;
}

// static esp_err_t gatt_session_write_with_response(gatt_session_t *session, const uint8_t *data, uint16_t data_size)
// {
//     int rc = -1, retry_count = 0;
//     while (rc != 0 && retry_count < GATT_SESSION_MAX_RETRIES) {
//         // Use different APIs to send SACK packet based on role
//         if (session->role == GATT_SESSION_ROLE_MASTER) {
//             // Master role: use client API to write characteristic value (with response)
//             rc = ble_gattc_write_flat(session->conn_handle, session->write_val_handle, (uint8_t *)data, data_size, NULL, NULL);
//             ESP_LOGD(TAG, "sent write with rsp; conn_handle=%d, attr_handle=%d, attr_len=%d",
//                      session->conn_handle, session->write_val_handle, data_size);
//         } else {
//             // Slave role: use server API to send indication (with response)
//             struct os_mbuf *om = ble_hs_mbuf_from_flat((uint8_t *)data, data_size);
//             if (!om) {
//                 ESP_LOGE(TAG, "Failed to create mbuf");
//                 rc = ESP_ERR_NO_MEM;
//                 break;
//             }
//             rc = ble_gatts_indicate_custom(session->conn_handle, session->write_val_handle, om);
//             ESP_LOGD(TAG, "sent indicate with rsp; conn_handle=%d, attr_handle=%d, attr_len=%d",
//                      session->conn_handle, session->write_val_handle, data_size);
//             if (rc != 0) {
//                 os_mbuf_free_chain(om);
//             }
//         }
//         // Check if there are enough MBUFs available
//         if (rc == BLE_HS_ENOMEM || ble_hs_l2cap_pkt_quota_get() < GATT_SESSION_MAX_MBUF_QUOTA) {
//             ESP_LOGD(TAG, "Not enough MBUFs available, waiting...");
//             vTaskDelay(pdMS_TO_TICKS(10));
//         }
//         retry_count++;
//     }

//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to send data: rc=%d", rc);
//         return rc;
//     }

//     return ESP_OK;
// }

// static esp_err_t gatt_session_write_without_response(gatt_session_t *session, const uint8_t *data, uint16_t data_size)
// {
//     int rc = -1, retry_count = 0;
//     while (rc != 0 && retry_count < 100) {
//         // Use different APIs to send SACK packet based on role
//         if (session->role == GATT_SESSION_ROLE_MASTER) {
//             // Master role: use client API to write characteristic value (no response)
//             rc = ble_gattc_write_no_rsp_flat(session->conn_handle, session->write_val_handle, (uint8_t *)data, data_size);
//             // Batch send optimization: reduce log output, improve performance
//             ESP_LOGI(TAG, "sent write no rsp; conn_handle=%d, attr_handle=%d, attr_len=%d",
//                      session->conn_handle, session->write_val_handle, data_size);
//         } else {
//             // Slave role: use server API to send notification
//             struct os_mbuf *om = ble_hs_mbuf_from_flat((uint8_t *)data, data_size);
//             if (!om) {
//                 ESP_LOGE(TAG, "Failed to create mbuf");
//                 rc = ESP_ERR_NO_MEM;
//                 goto end;
//             }
//             rc = ble_gatts_notify_custom(session->conn_handle, session->write_val_handle, om);
//             // Batch send optimization: reduce log output, improve performance
//             ESP_LOGD(TAG, "sent notification; conn_handle=%d, attr_handle=%d, attr_len=%d",
//                      session->conn_handle, session->write_val_handle, data_size);
//             if (rc != 0) {
//                 os_mbuf_free_chain(om);
//             }
//         }
//         // Check if there are enough MBUFs available
//         if (rc == BLE_HS_ENOMEM || ble_hs_l2cap_pkt_quota_get() < GATT_SESSION_MAX_MBUF_QUOTA) {
//             ESP_LOGD(TAG, "Not enough MBUFs available, waiting...");
//             vTaskDelay(pdMS_TO_TICKS(10));
//         }
//         retry_count++;
//     }

// end:
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to send data: rc=%d", rc);
//         return rc;
//     }

//     return ESP_OK;
// }

static esp_err_t ble_transport_send(void *ctx, const uint8_t *data, uint16_t data_size)
{
    int rc = -1, retry_count = 0;
    ble_transport_ctx_t *btx = (ble_transport_ctx_t *)ctx;

    while (rc != 0 && retry_count < 100) {
        // Use different APIs to send SACK packet based on role
        if (btx->role == GATT_SESSION_ROLE_MASTER) {
            // Master role: use client API to write characteristic value (no response)
            rc = ble_gattc_write_no_rsp_flat(btx->conn_handle, btx->write_val_handle, data, data_size);
            // Batch send optimization: reduce log output, improve performance
            ESP_LOGI(TAG, "sent write no rsp; conn_handle=%d, attr_handle=%d, attr_len=%d",
                     btx->conn_handle, btx->write_val_handle, data_size);
        } else {
            // Slave role: use server API to send notification
            struct os_mbuf *om = ble_hs_mbuf_from_flat((uint8_t *)data, data_size);
            if (om) {
                rc = ble_gatts_notify_custom(btx->conn_handle, btx->write_val_handle, om);
                // Batch send optimization: reduce log output, improve performance
                ESP_LOGD(TAG, "sent notification; conn_handle=%d, attr_handle=%d, attr_len=%d",
                         btx->conn_handle, btx->write_val_handle, data_size);
                if (rc != 0) {
                    os_mbuf_free_chain(om);
                }
            } else {
                ESP_LOGE(TAG, "Failed to create mbuf");
                rc = ESP_ERR_NO_MEM;
            }
        }
        // Check if there are enough MBUFs available
        if (rc == BLE_HS_ENOMEM || ble_hs_l2cap_pkt_quota_get() < GATT_SESSION_MAX_MBUF_QUOTA) {
            ESP_LOGD(TAG, "Not enough MBUFs available, waiting...");
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        retry_count++;
    }

    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send data: rc=%d", rc);
        return rc;
    }

    return ESP_OK;
}

static uint16_t ble_transport_get_mtu(void *ctx)
{
    ble_transport_ctx_t *btx = (ble_transport_ctx_t *)ctx;
    uint16_t mtu = ble_att_mtu(btx->conn_handle);
    return (mtu > 0) ? mtu : BLE_ATT_MTU_DFLT;
}

static const flux_transport_ops_t ble_transport_ops = {
    .send = ble_transport_send,
    .get_mtu = ble_transport_get_mtu,
};

/* ────────────────────── Flux Callback Bridges ────────────────────── */
static void flux_on_complete_bridge(flux_session_t *flux_session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size)
{
    gatt_session_t *target = NULL;
    session_complete_cb cb = NULL;
    if (g_gatt_sessions_mutex) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    }
    gatt_session_t *session;
    SLIST_FOREACH(session, &g_sessions_list, next) {
        if (session->flux_session == flux_session && session->callbacks.session_complete_cb &&
                gatt_session_callback_pin_locked(session)) {
            target = session;
            cb = session->callbacks.session_complete_cb;
            break;
        }
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreGive(g_gatt_sessions_mutex);
    }
    if (target && cb) {
        cb(target, stream_id, status, data, size);
        gatt_session_callback_leave(target);
    }
}

static void flux_on_data_received_bridge(flux_session_t *flux_session, uint8_t stream_id, const uint8_t *data, uint32_t size)
{
    gatt_session_t *target = NULL;
    data_received_cb cb = NULL;
    if (g_gatt_sessions_mutex) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    }
    gatt_session_t *session;
    SLIST_FOREACH(session, &g_sessions_list, next) {
        if (session->flux_session == flux_session && session->callbacks.data_received_cb &&
                gatt_session_callback_pin_locked(session)) {
            target = session;
            cb = session->callbacks.data_received_cb;
            break;
        }
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreGive(g_gatt_sessions_mutex);
    }
    if (target && cb) {
        cb(target, stream_id, data, size);
        gatt_session_callback_leave(target);
    }
}

static void flux_on_progress_bridge(flux_session_t *flux_session, uint8_t stream_id, uint32_t transferred, uint32_t total)
{
    gatt_session_t *target = NULL;
    progress_cb cb = NULL;
    if (g_gatt_sessions_mutex) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    }
    gatt_session_t *session;
    SLIST_FOREACH(session, &g_sessions_list, next) {
        if (session->flux_session == flux_session && session->callbacks.progress_cb &&
                gatt_session_callback_pin_locked(session)) {
            target = session;
            cb = session->callbacks.progress_cb;
            break;
        }
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreGive(g_gatt_sessions_mutex);
    }
    if (target && cb) {
        cb(target, stream_id, transferred, total);
        gatt_session_callback_leave(target);
    }
}

static void flux_on_error_bridge(flux_session_t *flux_session, esp_err_t error)
{
    gatt_session_t *target = NULL;
    error_cb cb = NULL;
    if (g_gatt_sessions_mutex) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    }
    gatt_session_t *session;
    SLIST_FOREACH(session, &g_sessions_list, next) {
        if (session->flux_session == flux_session && session->callbacks.error_cb &&
                gatt_session_callback_pin_locked(session)) {
            target = session;
            cb = session->callbacks.error_cb;
            break;
        }
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreGive(g_gatt_sessions_mutex);
    }
    if (target && cb) {
        cb(target, error);
        gatt_session_callback_leave(target);
    }
}

/* ────────────────────── Session Lookup ────────────────────── */
gatt_session_t *gatt_session_find_by_handle(uint16_t conn_handle)
{
    if (g_gatt_sessions_mutex) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    }
    gatt_session_t *session;
    SLIST_FOREACH(session, &g_sessions_list, next) {
        if (session->conn_handle == conn_handle) {
            if (g_gatt_sessions_mutex) {
                xSemaphoreGive(g_gatt_sessions_mutex);
            }
            return session;
        }
    }
    if (g_gatt_sessions_mutex) {
        xSemaphoreGive(g_gatt_sessions_mutex);
    }
    return NULL;
}

/* ────────────────────── Feed Data Helper ────────────────────── */
static void ble_feed_data_pinned(gatt_session_t *session, struct os_mbuf *om, uint16_t data_len)
{
    if (!session || !session->flux_session) {
        return;
    }

    uint8_t *buf = malloc(data_len);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate feed buffer");
        return;
    }

    int rc = os_mbuf_copydata(om, 0, data_len, buf);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to copy mbuf data: %d", rc);
        free(buf);
        return;
    }

    ESP_LOGD(TAG, "Feeding %d bytes to flux session (conn_handle=%d)", data_len, session->conn_handle);
    esp_err_t err = flux_session_feed_data(session->flux_session, buf, data_len);
    if (err != ESP_OK && session->callbacks.error_cb) {
        session->callbacks.error_cb(session, err);
    }
    free(buf);
}

static void ble_feed_data_from_mbuf(uint16_t conn_handle, struct os_mbuf *om, uint16_t data_len)
{
    gatt_session_t *session = gatt_session_find_by_handle_pinned(conn_handle);
    if (!session || !session->flux_session) {
        ESP_LOGW(TAG, "No session for conn_handle=%d", conn_handle);
        if (session) {
            gatt_session_callback_leave(session);
        }
        return;
    }

    ble_feed_data_pinned(session, om, data_len);
    gatt_session_callback_leave(session);
}

/* ────────────────────── GAP Event Handler ────────────────────── */
static int gatt_session_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_NOTIFY_RX: {
        /* Peer sent us a notification or indication. */
        uint16_t conn_handle = event->notify_rx.conn_handle;
        uint16_t data_len = OS_MBUF_PKTLEN(event->notify_rx.om);
        ESP_LOGD(TAG, "Notification RX: conn_handle=%d, len=%d", conn_handle, data_len);
        ble_feed_data_from_mbuf(conn_handle, event->notify_rx.om, data_len);
        return 0;
    }

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe: conn=%d attr=%d cur_notify=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle, event->subscribe.cur_notify);
        return 0;

    case BLE_GAP_EVENT_MTU: {
        gatt_session_t *session = gatt_session_find_by_handle_pinned(event->mtu.conn_handle);
        if (session) {
            session->mtu_size = event->mtu.value;
            if (session->flux_session) {
                flux_session_set_mtu(session->flux_session, event->mtu.value);
            }
            ESP_LOGI(TAG, "MTU updated: %d (conn_handle=%d)", event->mtu.value, event->mtu.conn_handle);
            if (session->callbacks.mtu_changed_cb) {
                session->callbacks.mtu_changed_cb(session, event->mtu.value);
            }
            gatt_session_callback_leave(session);
        }
        return 0;
    }

    case BLE_GAP_EVENT_CONNECT: {
        // Connection established event
        if (event->connect.status == 0) {
            gatt_session_t *session = gatt_session_find_by_handle_pinned(event->connect.conn_handle);
            if (session && session->role == GATT_SESSION_ROLE_MASTER && session->was_disconnected && session->read_val_handle != 0) {
                uint16_t cccd = session->read_val_handle + 1;
                uint8_t val[2] = {1, 0};
                ble_gattc_write_flat(event->connect.conn_handle, cccd, val, sizeof(val), NULL, NULL);
                ESP_LOGI(TAG, "Re-enabled notification on reconnect (cccd=%d)", cccd);
            }
            if (session) {
                session->was_disconnected = false;
                gatt_session_callback_leave(session);
            }
        }
        return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT: {
        // Connection disconnected event
        gatt_session_t *session = gatt_session_find_by_handle_pinned(event->disconnect.conn.conn_handle);
        if (session) {
            session->was_disconnected = true;
            gatt_session_callback_leave(session);
        }
        return 0;
    }

    case BLE_GAP_EVENT_NOTIFY_TX: {
        ESP_LOGD(TAG, "notify tx event; conn_handle=%d, status=%d, indication=%d",
                 event->notify_tx.conn_handle, event->notify_tx.status, event->notify_tx.indication);
        return 0;
    }

    default:
        return 0;
    }
}

/* ────────────────────── GATT Handler (Server Side) ────────────────────── */
int gatt_session_gatt_handler(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    gatt_session_t *session = gatt_session_find_by_handle_pinned(conn_handle);
    if (!session) {
        ESP_LOGW(TAG, "No session found for conn_handle=%d, attr_handle=%d", conn_handle, attr_handle);
        return BLE_ATT_ERR_UNLIKELY;
    }

    // Check if attr_handle matches
    if (attr_handle != session->write_val_handle && attr_handle != session->read_val_handle) {
        ESP_LOGW(TAG, "attr_handle mismatch: %d (expected write=%d or read=%d)",
                 attr_handle, session->write_val_handle, session->read_val_handle);
        gatt_session_callback_leave(session);
        return BLE_ATT_ERR_UNLIKELY;
    }

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
        // Read characteristic value - return empty data or status
        ESP_LOGD(TAG, "Read request: conn_handle=%d, attr_handle=%d", conn_handle, attr_handle);
        // Can return some status information, returning empty here
        int rc = os_mbuf_append(ctxt->om, NULL, 0);
        if (rc != 0) {
            gatt_session_callback_leave(session);
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        gatt_session_callback_leave(session);
        return 0;
    }

    case BLE_GATT_ACCESS_OP_WRITE_CHR: {
        // Write characteristic value - process received data
        uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
        ESP_LOGD(TAG, "Write: conn=%d, attr=%d, len=%d", conn_handle, attr_handle, data_len);
        ble_feed_data_pinned(session, ctxt->om, data_len);
        gatt_session_callback_leave(session);
        return 0;
    }

    default:
        gatt_session_callback_leave(session);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* ────────────────────── Session Lifecycle ────────────────────── */
gatt_session_t *gatt_session_create(uint16_t conn_handle, const ble_session_callbacks_t *callbacks,
                                    uint16_t write_val_handle, uint16_t read_val_handle, uint16_t mtu_size, gatt_session_role_t role)
{
    if (g_gatt_sessions_mutex == NULL) {
        g_gatt_sessions_mutex = xSemaphoreCreateMutex();
        if (g_gatt_sessions_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create gatt session mutex");
            return NULL;
        }
    }

    gatt_session_t *session = calloc(1, sizeof(gatt_session_t));
    if (!session) {
        return NULL;
    }

    session->conn_handle = conn_handle;
    session->write_val_handle = write_val_handle;
    session->read_val_handle = read_val_handle;
    session->mtu_size = mtu_size;
    session->role = role;
    // Initialize notify auto-reconnect related fields
    session->was_disconnected = false;      // First connection

    // Setup BLE transport context
    session->transport_ctx.conn_handle = conn_handle;
    session->transport_ctx.write_val_handle = write_val_handle;
    session->transport_ctx.role = role;

    // Setup flux callbacks bridging to gatt_session callbacks
    flux_callbacks_t flux_cbs = {
        .session_complete_cb = flux_on_complete_bridge,
        .data_received_cb = flux_on_data_received_bridge,
        .progress_cb = flux_on_progress_bridge,
        .error_cb = flux_on_error_bridge,
        .arg = NULL,
    };

    if (callbacks) {
        memcpy(&session->callbacks, callbacks, sizeof(ble_session_callbacks_t));
    }

    // Create flux session with BLE transport ops
    session->flux_session = flux_session_create(&ble_transport_ops, &session->transport_ctx, &flux_cbs, mtu_size);
    if (!session->flux_session) {
        ESP_LOGE(TAG, "Failed to create flux session");
        free(session);
        return NULL;
    }

    xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    session->destroying = false;
    // Register GAP event listener (once)
    if (!g_gap_listener_registered) {
        int rc = ble_gap_event_listener_register(&g_gatt_session_gap_event_listener, gatt_session_gap_event, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to register GAP listener: %d", rc);
        } else {
            g_gap_listener_registered = true;
        }
    }

    SLIST_INSERT_HEAD(&g_sessions_list, session, next);
    xSemaphoreGive(g_gatt_sessions_mutex);

    ESP_LOGI(TAG, "Session created: conn=%d, mtu=%d, role=%s",
             conn_handle, mtu_size, role == GATT_SESSION_ROLE_MASTER ? "MASTER" : "SLAVE");
    return session;
}

esp_err_t gatt_session_destroy(gatt_session_t *session)
{
    if (!session) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    session->destroying = true;
    // Remove from global session list
    gatt_session_t *prev = NULL;
    gatt_session_t *cur = SLIST_FIRST(&g_sessions_list);
    bool found = false;

    // Iterate through the list to find the node to delete
    while (cur != NULL) {
        if (cur == session) {
            // Found the node to delete
            if (prev == NULL) {
                // Is the first element
                SLIST_REMOVE_HEAD(&g_sessions_list, next);
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

    // Unregister GAP listener if no sessions left
    if (SLIST_EMPTY(&g_sessions_list) && g_gap_listener_registered) {
        ble_gap_event_listener_unregister(&g_gatt_session_gap_event_listener);
        g_gap_listener_registered = false;
    }
    xSemaphoreGive(g_gatt_sessions_mutex);

    /* Wait for in-flight GAP/feed callbacks before tearing down Flux. */
    while (true) {
        uint16_t callback_inflight = 0;
        if (g_gatt_sessions_mutex) {
            xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
        }
        callback_inflight = session->callback_inflight;
        if (g_gatt_sessions_mutex) {
            xSemaphoreGive(g_gatt_sessions_mutex);
        }
        if (callback_inflight == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (session->flux_session) {
        flux_session_destroy(session->flux_session);
        session->flux_session = NULL;
    }

    free(session);
    ESP_LOGI(TAG, "Session destroyed");
    return ESP_OK;
}

esp_err_t gatt_session_schedule_destroy(gatt_session_t *session)
{
    gatt_session_destroy_event_t *event = NULL;

    if (!session) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!g_gatt_sessions_mutex) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
    if (session->destroying || session->destroy_scheduled) {
        xSemaphoreGive(g_gatt_sessions_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    session->destroy_scheduled = true;
    xSemaphoreGive(g_gatt_sessions_mutex);

    event = calloc(1, sizeof(*event));
    if (!event) {
        xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
        session->destroy_scheduled = false;
        xSemaphoreGive(g_gatt_sessions_mutex);
        return ESP_ERR_NO_MEM;
    }

    event->session = session;
    ble_npl_event_init(&event->ev, gatt_session_destroy_event_fn, event);
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &event->ev);
    return ESP_OK;
}

esp_err_t gatt_session_set_mtu(gatt_session_t *session, uint16_t mtu)
{
    esp_err_t ret = ESP_OK;
    if (!session || mtu == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    session->mtu_size = mtu;
    if (session->flux_session) {
        ret = flux_session_set_mtu(session->flux_session, mtu);
    }

    if (ret == ESP_OK && session->callbacks.mtu_changed_cb) {
        bool pinned = false;
        if (g_gatt_sessions_mutex) {
            xSemaphoreTake(g_gatt_sessions_mutex, portMAX_DELAY);
            pinned = gatt_session_callback_pin_locked(session);
            xSemaphoreGive(g_gatt_sessions_mutex);
        }
        if (pinned) {
            session->callbacks.mtu_changed_cb(session, mtu);
            gatt_session_callback_leave(session);
        }
    }

    return ret;
}

esp_err_t gatt_session_set_mtu_by_handle(uint16_t conn_handle, uint16_t mtu)
{
    gatt_session_t *session = gatt_session_find_by_handle_pinned(conn_handle);
    if (!session) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = gatt_session_set_mtu(session, mtu);
    gatt_session_callback_leave(session);
    return err;
}

esp_err_t gatt_session_feed_data_by_handle(uint16_t conn_handle,
                                           const uint8_t *data, uint16_t data_len)
{
    gatt_session_t *session;
    esp_err_t err;

    if (!data && data_len != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (data_len == 0) {
        return ESP_OK;
    }

    session = gatt_session_find_by_handle_pinned(conn_handle);
    if (!session) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!session->flux_session) {
        err = ESP_ERR_INVALID_STATE;
    } else {
        err = flux_session_feed_data(session->flux_session, data, data_len);
        if (err != ESP_OK && session->callbacks.error_cb) {
            session->callbacks.error_cb(session, err);
        }
    }

    gatt_session_callback_leave(session);
    return err;
}

/* ────────────────────── Send / recv API (delegates to flux_session) ────────────────────── */
esp_err_t gatt_session_send(gatt_session_t *session, const uint8_t *data,
                            uint32_t size, uint8_t window_size, uint8_t window_threshold_percent)
{
    if (!session || !session->flux_session) {
        return ESP_ERR_INVALID_ARG;
    }

    return flux_session_send(session->flux_session, data, size, window_size, window_threshold_percent);
}

bool gatt_session_send_idle(gatt_session_t *session)
{
    if (!session || !session->flux_session) {
        return false;
    }

    return flux_session_send_idle(session->flux_session);
}

bool gatt_session_send_is_complete(gatt_session_t *session, uint8_t stream_id)
{
    if (!session || !session->flux_session) {
        return false;
    }

    return flux_session_send_is_complete(session->flux_session, stream_id);
}

uint8_t gatt_session_send_get_progress(gatt_session_t *session, uint8_t stream_id)
{
    if (!session || !session->flux_session) {
        return 0;
    }

    return flux_session_send_get_progress(session->flux_session, stream_id);
}

esp_err_t gatt_session_send_reset(gatt_session_t *session, uint8_t stream_id)
{
    if (!session || !session->flux_session) {
        return ESP_ERR_INVALID_ARG;
    }

    return flux_session_send_reset(session->flux_session, stream_id);
}

bool gatt_session_recv_idle(gatt_session_t *session)
{
    if (!session || !session->flux_session) {
        return false;
    }

    return flux_session_recv_idle(session->flux_session);
}

bool gatt_session_recv_is_complete(gatt_session_t *session, uint8_t stream_id)
{
    if (!session || !session->flux_session) {
        return false;
    }

    return flux_session_recv_is_complete(session->flux_session, stream_id);
}

uint8_t gatt_session_recv_get_progress(gatt_session_t *session, uint8_t stream_id)
{
    if (!session || !session->flux_session) {
        return 0;
    }

    return flux_session_recv_get_progress(session->flux_session, stream_id);
}

esp_err_t gatt_session_recv_reset(gatt_session_t *session, uint8_t stream_id)
{
    if (!session || !session->flux_session) {
        return ESP_ERR_INVALID_ARG;
    }

    return flux_session_recv_reset(session->flux_session, stream_id);
}

flux_session_t *gatt_session_get_flux(const gatt_session_t *session)
{
    return session ? session->flux_session : NULL;
}

uint16_t gatt_session_get_mtu(const gatt_session_t *session)
{
    return session ? session->mtu_size : 0;
}
