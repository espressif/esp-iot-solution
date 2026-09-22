/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "host/ble_att.h"
#include "esp_ble_conn_mgr.h"
#include "flux_gatt_session.h"

static const char *TAG = "spp client";

/* 16 Bit SPP Service UUID */
#define BLE_SVC_SPP_UUID16      0xABF0
/* 16 Bit SPP Service Characteristic UUID */
#define BLE_SVC_SPP_CHR_UUID16  0xABF1

#define LL_PACKET_TIME      2120
#define LL_PACKET_LENGTH    251
#define OPTIMAL_MTU_SIZE    498

#define MAX_CONNECTIONS CONFIG_BLE_CONN_MGR_MAX_CONNECTIONS

#define SPP_TEST_TRANSFER_SIZE  (482 + 491 * 8) // 482 + 491 * 8 = 482 + 3928 = 4411

#define SPP_RECONNECT_INTERVAL_MS  5000

static gatt_session_t *g_session_instances[MAX_CONNECTIONS] = {0};
static bool g_conn_params_ready[MAX_CONNECTIONS] = {0};
static uint16_t g_connected_handles[MAX_CONNECTIONS] = {0};
static bool g_connected_handle_valid[MAX_CONNECTIONS] = {0};
static SemaphoreHandle_t g_sessions_mutex = NULL;
static TimerHandle_t s_reconnect_timer = NULL;

static inline void sessions_lock(void)
{
    if (g_sessions_mutex) {
        xSemaphoreTake(g_sessions_mutex, portMAX_DELAY);
    }
}

static inline void sessions_unlock(void)
{
    if (g_sessions_mutex) {
        xSemaphoreGive(g_sessions_mutex);
    }
}

static int spp_find_session_idx_by_conn(uint16_t conn_handle)
{
    sessions_lock();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_session_instances[i] && g_session_instances[i]->conn_handle == conn_handle) {
            sessions_unlock();
            return i;
        }
    }
    sessions_unlock();
    return -1;
}

static int spp_active_session_count(void)
{
    int n = 0;

    sessions_lock();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_session_instances[i]) {
            n++;
        }
    }
    sessions_unlock();
    return n;
}

static int spp_connected_count(void)
{
    int n = 0;

    sessions_lock();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connected_handle_valid[i]) {
            n++;
        }
    }
    sessions_unlock();
    return n;
}

static void spp_track_connected(uint16_t conn_handle)
{
    int free_idx = -1;

    sessions_lock();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connected_handle_valid[i] && g_connected_handles[i] == conn_handle) {
            sessions_unlock();
            return;
        }
        if (!g_connected_handle_valid[i] && free_idx < 0) {
            free_idx = i;
        }
    }
    if (free_idx >= 0) {
        g_connected_handles[free_idx] = conn_handle;
        g_connected_handle_valid[free_idx] = true;
    }
    sessions_unlock();

    if (free_idx < 0) {
        ESP_LOGW(TAG, "No connection tracking slot for conn_handle=%d", conn_handle);
    }
}

static void spp_track_disconnected(uint16_t conn_handle)
{
    sessions_lock();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_connected_handle_valid[i] && g_connected_handles[i] == conn_handle) {
            g_connected_handle_valid[i] = false;
            break;
        }
    }
    sessions_unlock();
}

static void spp_fill_test_buffer(uint8_t *buf, size_t size)
{
    const char *base_text = "The quick brown fox jumps over the lazy dog. This is a test message for BLE SPP communication. ";
    const size_t base_len = strlen(base_text);
    size_t written = 0;

    while (written < size) {
        size_t to_copy = (size - written < base_len) ? (size - written) : base_len;
        memcpy(buf + written, base_text, to_copy);
        written += to_copy;
    }
}

static void gatt_session_complete_cb(gatt_session_t *session, uint8_t stream_id, esp_err_t status, const uint8_t *data, uint32_t size)
{
    if (status == ESP_OK) {
        ESP_LOGI(TAG, "Session conn_handle %d stream %d send completed successfully (%" PRIu32 " bytes)", session->conn_handle, stream_id, size);
    } else {
        ESP_LOGE(TAG, "Session conn_handle %d stream %d send failed: %d", session->conn_handle, stream_id, status);
    }

    // The library passes back the original source buffer (malloced by this app before
    // gatt_session_send()) so the correct one is freed. The library has already
    // released its internal send-slot state. stream_id identifies which transfer this was.
    if (data) {
        free((void *)data);
    }
}

static void gatt_data_received_cb(gatt_session_t *session, uint8_t stream_id, const uint8_t *data, uint32_t size)
{
    ESP_LOGW(TAG, "Session conn_handle: %d stream %d received data, data size: %" PRIu32 " bytes (%.2f KB)", session->conn_handle, stream_id, size, size / 1024.0f);

    // Note: Free the data pointer after use.
    free((void *)data);
}

static void gatt_progress_cb(gatt_session_t *session, uint32_t transferred, uint32_t total)
{
    // Prevent division by zero
    if (total == 0) {
        ESP_LOGW(TAG, "Session conn_handle %d progress callback: total is 0, transferred=%" PRIu32, session->conn_handle, transferred);
        return;
    }
    uint8_t progress = (transferred * 100) / total;
    ESP_LOGI(TAG, "Session conn_handle: %d progress: %u%% (%" PRIu32 "/%" PRIu32 " bytes)", session->conn_handle, progress, transferred, total);
}

static void spp_tune_connection(uint16_t conn_handle)
{
    esp_err_t rc;

    rc = esp_ble_conn_set_data_len(conn_handle, LL_PACKET_LENGTH, LL_PACKET_TIME);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Set data length failed; rc=%d", rc);
    }
    rc = esp_ble_conn_mtu_update(conn_handle, OPTIMAL_MTU_SIZE);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Set MTU failed; rc=%d", rc);
    }
    rc = esp_ble_conn_set_preferred_phy(conn_handle, ESP_BLE_CONN_PHY_MASK_2M, ESP_BLE_CONN_PHY_MASK_2M, 0);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Set preferred phy failed; rc=%d", rc);
    }

    esp_ble_conn_params_t params = {
        .itvl_min = 24,
        .itvl_max = 24,
        .latency = 0,
        .supervision_timeout = 300,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    rc = esp_ble_conn_update_params(conn_handle, &params);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Update connection params failed; rc=%d", rc);
    }
}

static void spp_destroy_session(uint16_t conn_handle)
{
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        gatt_session_t *session_to_destroy = NULL;
        sessions_lock();
        if (g_session_instances[i] && g_session_instances[i]->conn_handle == conn_handle) {
            session_to_destroy = g_session_instances[i];
            g_session_instances[i] = NULL;
            g_conn_params_ready[i] = false;
        }
        sessions_unlock();
        if (session_to_destroy) {
            gatt_session_schedule_destroy(session_to_destroy);
            ESP_LOGI(TAG, "GATT session destroyed for conn_handle=%d", conn_handle);
        }
    }
}

static void spp_disconnect_failed_setup(uint16_t conn_handle, const char *reason)
{
    esp_err_t rc;

    ESP_LOGW(TAG, "%s, disconnecting conn_handle=%d for retry", reason, conn_handle);
    rc = esp_ble_conn_disconnect_by_handle(conn_handle);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect unusable peer, conn_handle=%d rc=%d", conn_handle, rc);
    }
}

static void spp_on_disc_complete(uint16_t conn_handle)
{
    uint16_t val_handle = 0;
    uint16_t mtu = BLE_ATT_MTU_DFLT;
    uint8_t notify_en[2] = {1, 0};
    esp_ble_conn_uuid_t svc_uuid = { .uuid16 = BLE_SVC_SPP_UUID16 };
    esp_ble_conn_uuid_t chr_uuid = { .uuid16 = BLE_SVC_SPP_CHR_UUID16 };
    esp_ble_conn_data_t sub = {
        .type = BLE_CONN_UUID_TYPE_16,
        .uuid = { .uuid16 = BLE_SVC_SPP_CHR_UUID16 },
        .data = notify_en,
        .data_len = sizeof(notify_en),
    };
    ble_session_callbacks_t callbacks = {
        .session_complete_cb = gatt_session_complete_cb,
        .data_received_cb = gatt_data_received_cb,
        .progress_cb = gatt_progress_cb,
        .error_cb = NULL
    };

    ESP_LOGI(TAG, "Service discovery completed for connection %d", conn_handle);

    if (spp_find_session_idx_by_conn(conn_handle) >= 0) {
        return;
    }

    if (esp_ble_conn_subscribe_by_handle(conn_handle, ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub) != ESP_OK) {
        spp_disconnect_failed_setup(conn_handle, "Notification setup failed");
        return;
    }

    if (esp_ble_conn_get_peer_chr_handle(conn_handle,
                                         BLE_CONN_UUID_TYPE_16, svc_uuid,
                                         BLE_CONN_UUID_TYPE_16, chr_uuid,
                                         &val_handle) != ESP_OK) {
        spp_disconnect_failed_setup(conn_handle, "SPP characteristic not found after discovery");
        return;
    }

    if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) != ESP_OK || mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }

    gatt_session_t *session = gatt_session_create(conn_handle, &callbacks, val_handle, val_handle,
                                                  mtu, GATT_SESSION_ROLE_MASTER);
    if (!session) {
        spp_disconnect_failed_setup(conn_handle, "Failed to create GATT session");
        return;
    }

    int session_idx = -1;
    sessions_lock();
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_session_instances[i]) {
            g_session_instances[i] = session;
            session_idx = i;
            break;
        }
    }
    sessions_unlock();

    if (session_idx < 0) {
        gatt_session_schedule_destroy(session);
        spp_disconnect_failed_setup(conn_handle, "No GATT session slot available");
        return;
    }

    g_conn_params_ready[session_idx] = true;
    ESP_LOGW(TAG, "GATT session ready for conn_handle=%d", session->conn_handle);
}

static bool spp_adv_has_uuid16(const esp_ble_conn_scan_result_t *result, uint16_t uuid16)
{
    const uint8_t ad_types[] = {
        ESP_BLE_CONN_ADV_TYPE_UUID16_INCOMP,
        ESP_BLE_CONN_ADV_TYPE_UUID16_COMPLETE,
    };

    for (size_t t = 0; t < sizeof(ad_types); t++) {
        const uint8_t *p = NULL;
        uint8_t len = 0;

        if (esp_ble_conn_parse_adv_data(result->adv_data, result->adv_data_len, ad_types[t], &p, &len) != ESP_OK) {
            continue;
        }
        for (uint8_t i = 0; i + 1 < len; i += 2) {
            uint16_t uuid = (uint16_t)p[i] | ((uint16_t)p[i + 1] << 8);
            if (uuid == uuid16) {
                return true;
            }
        }
    }
    return false;
}

static bool spp_scan_cb(const esp_ble_conn_scan_result_t *result, void *arg)
{
    (void)arg;

    if (!result || !spp_adv_has_uuid16(result, BLE_SVC_SPP_UUID16)) {
        return false;
    }

    ESP_LOGI(TAG, "Found SPP server " MACSTR " rssi=%d", MAC2STR(result->addr), result->rssi);
    return true;
}

static void spp_scan_for_available_slot(bool restart)
{
    esp_err_t rc;

    if (spp_connected_count() >= MAX_CONNECTIONS) {
        return;
    }

    if (restart) {
        /* Recover from a stalled scan while preserving established links. */
        (void)esp_ble_conn_scan_stop();
    }
    rc = esp_ble_conn_scan_start();
    if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Start scan failed: %d", rc);
    }
}

static void spp_update_reconnect_timer(void)
{
    if (!s_reconnect_timer) {
        return;
    }
    if (spp_connected_count() < MAX_CONNECTIONS) {
        xTimerStart(s_reconnect_timer, 0);
    } else {
        xTimerStop(s_reconnect_timer, 0);
    }
}

static void spp_reconnect_timer_cb(TimerHandle_t t)
{
    (void)t;
    spp_scan_for_available_slot(true);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;

    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_STARTED:
        spp_update_reconnect_timer();
        break;
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        esp_ble_conn_event_data_t *evt = event_data;
        uint16_t conn_handle;

        if (!evt) {
            break;
        }
        conn_handle = evt->connected.conn_handle;
        spp_track_connected(conn_handle);
        spp_update_reconnect_timer();
        ESP_LOGI(TAG, "Connected successfully to device " MACSTR " (conn_handle=%d)",
                 MAC2STR(evt->connected.peer_addr), conn_handle);
        spp_tune_connection(conn_handle);
        spp_scan_for_available_slot(false);
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED: {
        esp_ble_conn_event_data_t *evt = event_data;
        uint16_t conn_handle = evt ? evt->disconnected.conn_handle : BLE_CONN_HANDLE_INVALID;

        if (evt) {
            ESP_LOGI(TAG, "Disconnected from device " MACSTR " (conn_handle=%d)",
                     MAC2STR(evt->disconnected.peer_addr), conn_handle);
        } else {
            ESP_LOGI(TAG, "Disconnected (conn_handle=%d)", conn_handle);
        }
        spp_track_disconnected(conn_handle);
        spp_destroy_session(conn_handle);
        spp_update_reconnect_timer();
        spp_scan_for_available_slot(false);
        break;
    }
    case ESP_BLE_CONN_EVENT_DISC_COMPLETE: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        spp_on_disc_complete(evt->disc_complete.conn_handle);
        spp_update_reconnect_timer();
        spp_scan_for_available_slot(false);
        break;
    }
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE:
        /* Central notifications are also consumed by gatt_session's GAP listener.
         * This event owns its copy as documented by esp_ble_conn_data_t. */
        if (event_data) {
            esp_ble_conn_event_data_t *evt = event_data;
            free(evt->data_receive.data);
            evt->data_receive.data = NULL;
        }
        break;
    case ESP_BLE_CONN_EVENT_MTU: {
        esp_ble_conn_event_data_t *evt = event_data;
        gatt_session_t *session;

        if (!evt) {
            break;
        }
        session = gatt_session_find_by_handle(evt->mtu_update.conn_handle);
        if (session) {
            gatt_session_set_mtu(session, evt->mtu_update.mtu);
        }
        break;
    }
    case ESP_BLE_CONN_EVENT_CONN_PARAM_UPDATE: {
        esp_ble_conn_event_data_t *evt = event_data;
        int idx;

        if (!evt) {
            break;
        }
        idx = spp_find_session_idx_by_conn(evt->conn_param_update.conn_handle);
        if (idx < 0) {
            break;
        }
        g_conn_params_ready[idx] = true;
        ESP_LOGW(TAG, "Connection parameters ready for conn_handle=%d (interval=%.2fms)",
                 evt->conn_param_update.conn_handle, evt->conn_param_update.params.itvl_min * 1.25f);
        break;
    }
    default:
        break;
    }
}

void app_main(void)
{
    esp_err_t ret;
    esp_ble_conn_config_t config = {
        .device_name = "ble-spp-client",
    };

    ESP_LOGI(TAG, "Starting BLE Component Test Example");
    esp_log_level_set("gatt_session", ESP_LOG_WARN);
    esp_log_level_set("esp_flux", ESP_LOG_WARN);
    esp_log_level_set("spp server", ESP_LOG_WARN);
    esp_log_level_set("spp client", ESP_LOG_WARN);
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    esp_log_level_set("blecm_nimble", ESP_LOG_WARN);

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    g_sessions_mutex = xSemaphoreCreateMutex();
    if (!g_sessions_mutex) {
        ESP_LOGE(TAG, "Failed to create sessions mutex");
        return;
    }

    s_reconnect_timer = xTimerCreate("spp_reconn", pdMS_TO_TICKS(SPP_RECONNECT_INTERVAL_MS),
                                     pdTRUE, NULL, spp_reconnect_timer_cb);
    if (!s_reconnect_timer) {
        ESP_LOGE(TAG, "Failed to create reconnect timer");
        return;
    }

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, app_ble_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    {
        esp_ble_conn_scan_params_t scan_params = {
            .itvl = 0x50,
            .window = 0x30,
            .passive = false,
            .filter_duplicates = false,
        };
        ESP_ERROR_CHECK(esp_ble_conn_scan_params_set(&scan_params));
    }
    ESP_ERROR_CHECK(esp_ble_conn_register_scan_callback(spp_scan_cb, NULL));
    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        return;
    }

    ESP_LOGW(TAG, "Free Heap: %" PRIu32 " bytes", esp_get_free_heap_size());

    uint32_t heap_log_counter = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));

        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            sessions_lock();
            gatt_session_t *session = g_session_instances[i];
            bool ready = g_conn_params_ready[i];
            if (!session || !ready || !gatt_session_send_idle(session)) {
                sessions_unlock();
                continue;
            }

            uint8_t *send_buffer = malloc(SPP_TEST_TRANSFER_SIZE);
            if (!send_buffer) {
                sessions_unlock();
                ESP_LOGE(TAG, "Failed to allocate send buffer");
                continue;
            }
            spp_fill_test_buffer(send_buffer, SPP_TEST_TRANSFER_SIZE);
            uint16_t conn_handle = session->conn_handle;
            ret = gatt_session_send(session, send_buffer, SPP_TEST_TRANSFER_SIZE, 0xFF, 80);
            sessions_unlock();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Started send to conn_handle=%d (%d bytes)", conn_handle, SPP_TEST_TRANSFER_SIZE);
            } else {
                ESP_LOGW(TAG, "Failed to send to conn_handle=%d: %d", conn_handle, ret);
                free(send_buffer);
            }
        }

        if (++heap_log_counter >= 50) {
            heap_log_counter = 0;
            ESP_LOGW(TAG, "Free Heap: %" PRIu32 " bytes, active connections: %d/%d",
                     esp_get_free_heap_size(), spp_active_session_count(), MAX_CONNECTIONS);
        }
    }
}
