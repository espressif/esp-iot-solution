/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_att.h"

#include "esp_ble_conn_mgr.h"
#include "esp_flux_transport.h"
#include "flux_gatt_session.h"
#include "esp_gmp.h"
#include "esp_gmp_flux.h"
#include "esp_gmp_os.h"
#include "esp_gmp_ota.h"
#include "gmp_ble_uuids.h"
#include "gmp_session_teardown.h"
#include "sdkconfig.h"

static const char *TAG = "gmp_ota_srv";

#define MAX_CONNECTIONS CONFIG_BLE_CONN_MGR_MAX_CONNECTIONS

static gatt_session_t *g_sessions[MAX_CONNECTIONS];
static bool g_notify_enabled[MAX_CONNECTIONS];
static gatt_session_t *g_teardown_pending[MAX_CONNECTIONS];
static portMUX_TYPE g_sessions_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE g_teardown_lock = portMUX_INITIALIZER_UNLOCKED;
static uint16_t g_chr_rx_handle;
static uint16_t g_chr_tx_handle;

static esp_ble_conn_character_t gmp_chrs[] = {
    {
        .name = "gmp_rx",
        .type = BLE_CONN_UUID_TYPE_128,
        .flag = BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_NO_RSP,
        .uuid = { .uuid128 = { GMP_BLE_UUID_CHR_RX_BYTES } },
        .uuid_fn = NULL,
    },
    {
        .name = "gmp_tx",
        .type = BLE_CONN_UUID_TYPE_128,
        .flag = BLE_CONN_GATT_CHR_NOTIFY,
        .uuid = { .uuid128 = { GMP_BLE_UUID_CHR_TX_BYTES } },
        .uuid_fn = NULL,
    },
};

static const esp_ble_conn_svc_t gmp_svc = {
    .type = BLE_CONN_UUID_TYPE_128,
    .uuid = { .uuid128 = { GMP_BLE_UUID_SVC_BYTES } },
    .nu_lookup_count = sizeof(gmp_chrs) / sizeof(gmp_chrs[0]),
    .nu_lookup = gmp_chrs,
};

/* Keep the GMP UUID in the connectable advertisement. A scan response is not
 * connectable, so the central must not initiate a connection from it. */
static const uint8_t gmp_adv_data[] = {
    0x02, ESP_BLE_CONN_ADV_TYPE_FLAGS, 0x06,
    BLE_UUID128_VAL_LEN + 1, ESP_BLE_CONN_ADV_TYPE_UUID128_COMPLETE,
    GMP_BLE_UUID_SVC_BYTES,
};

static int find_session_idx(uint16_t conn_handle)
{
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_sessions[i] && g_sessions[i]->conn_handle == conn_handle) {
            return i;
        }
    }
    return -1;
}

static void clear_session_slot(gatt_session_t *session)
{
    portENTER_CRITICAL(&g_sessions_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_sessions[i] == session) {
            g_sessions[i] = NULL;
            g_notify_enabled[i] = false;
            break;
        }
    }
    portEXIT_CRITICAL(&g_sessions_lock);
}

static bool session_slot_active(gatt_session_t *session)
{
    bool active = false;

    portENTER_CRITICAL(&g_sessions_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_sessions[i] == session) {
            active = !session->was_disconnected;
            break;
        }
    }
    portEXIT_CRITICAL(&g_sessions_lock);
    return active;
}

static bool session_notify_enabled(gatt_session_t *session)
{
    bool enabled = false;

    portENTER_CRITICAL(&g_sessions_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_sessions[i] == session) {
            enabled = g_notify_enabled[i];
            break;
        }
    }
    portEXIT_CRITICAL(&g_sessions_lock);
    return enabled;
}

static esp_err_t setup_gmp_callbacks(gatt_session_t *session)
{
    ble_session_callbacks_t callbacks = { 0 };
    esp_err_t err = esp_gmp_flux_get_gatt_callbacks(session, &callbacks, NULL);
    if (err != ESP_OK) {
        return err;
    }
    session->callbacks = callbacks;
    return ESP_OK;
}

static esp_err_t setup_gmp_session(gatt_session_t *session)
{
    return esp_gmp_flux_link_register(session, gatt_session_get_flux(session));
}

static void teardown_gmp_session(gatt_session_t *session)
{
    /* LINK_DOWN is notified by unregister; OTA device profile aborts itself. */
    esp_gmp_flux_link_unregister(session);
}

static void teardown_session(gatt_session_t *session)
{
    if (!session) {
        return;
    }

    if (gmp_session_teardown(session, teardown_gmp_session) != ESP_OK) {
        ESP_LOGE(TAG, "failed to schedule session destroy conn=%d", session->conn_handle);
    }
}

static bool teardown_pending_push(gatt_session_t *session)
{
    bool stored = false;

    portENTER_CRITICAL(&g_teardown_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_teardown_pending[i]) {
            g_teardown_pending[i] = session;
            stored = true;
            break;
        }
    }
    portEXIT_CRITICAL(&g_teardown_lock);
    return stored;
}

static gatt_session_t *teardown_pending_pop(void)
{
    gatt_session_t *session = NULL;

    portENTER_CRITICAL(&g_teardown_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_teardown_pending[i]) {
            session = g_teardown_pending[i];
            g_teardown_pending[i] = NULL;
            break;
        }
    }
    portEXIT_CRITICAL(&g_teardown_lock);
    return session;
}

static void teardown_session_task(void *arg)
{
    teardown_session((gatt_session_t *)arg);
    vTaskDelete(NULL);
}

static void process_pending_teardowns(void)
{
    gatt_session_t *session = NULL;

    while ((session = teardown_pending_pop()) != NULL) {
        teardown_session(session);
    }
}

static void activate_session_task(void *arg)
{
    gatt_session_t *session = (gatt_session_t *)arg;
    const uint16_t need_mtu = 185;
    uint16_t mtu = BLE_ATT_MTU_DFLT;
    bool notify_enabled = false;
    uint16_t conn_handle;

    if (!session) {
        vTaskDelete(NULL);
        return;
    }
    conn_handle = session->conn_handle;

    for (int i = 0; i < 40; i++) {
        if (!session_slot_active(session) || session->was_disconnected ||
                session->destroy_scheduled || session->destroying) {
            bool owned = false;
            portENTER_CRITICAL(&g_sessions_lock);
            for (int j = 0; j < MAX_CONNECTIONS; j++) {
                if (g_sessions[j] == session) {
                    g_sessions[j] = NULL;
                    g_notify_enabled[j] = false;
                    owned = true;
                    break;
                }
            }
            portEXIT_CRITICAL(&g_sessions_lock);
            if (owned) {
                gatt_session_schedule_destroy(session);
                (void)esp_ble_conn_disconnect_by_handle(conn_handle);
            }
            vTaskDelete(NULL);
            return;
        }
        notify_enabled = session_notify_enabled(session);
        if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) == ESP_OK &&
                mtu >= need_mtu && notify_enabled) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (mtu < need_mtu) {
        ESP_LOGW(TAG, "MTU still low (%u); activating anyway", (unsigned)mtu);
    }
    if (!notify_enabled) {
        ESP_LOGW(TAG, "Peer has not subscribed yet; activating anyway");
    }
    if (mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }

    if (!session_slot_active(session) || session->was_disconnected ||
            session->destroy_scheduled || session->destroying) {
        bool owned = false;
        portENTER_CRITICAL(&g_sessions_lock);
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (g_sessions[i] == session) {
                g_sessions[i] = NULL;
                g_notify_enabled[i] = false;
                owned = true;
                break;
            }
        }
        portEXIT_CRITICAL(&g_sessions_lock);
        if (owned) {
            gatt_session_schedule_destroy(session);
            (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        }
        vTaskDelete(NULL);
        return;
    }

    (void)gatt_session_set_mtu(session, mtu);

    if (!session_slot_active(session) || session->was_disconnected ||
            session->destroy_scheduled || session->destroying) {
        bool owned = false;
        portENTER_CRITICAL(&g_sessions_lock);
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (g_sessions[i] == session) {
                g_sessions[i] = NULL;
                g_notify_enabled[i] = false;
                owned = true;
                break;
            }
        }
        portEXIT_CRITICAL(&g_sessions_lock);
        if (owned) {
            gatt_session_schedule_destroy(session);
            (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        }
        vTaskDelete(NULL);
        return;
    }

    if (setup_gmp_callbacks(session) != ESP_OK) {
        ESP_LOGE(TAG, "setup GMP callbacks failed");
        clear_session_slot(session);
        if (gatt_session_schedule_destroy(session) != ESP_OK) {
            ESP_LOGE(TAG, "schedule_destroy failed; reinsert for disconnect cleanup");
            /* Prefer leaking until disconnect over destroying off host task. */
            portENTER_CRITICAL(&g_sessions_lock);
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (g_sessions[i] == NULL) {
                    g_sessions[i] = session;
                    break;
                }
            }
            portEXIT_CRITICAL(&g_sessions_lock);
        }
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        vTaskDelete(NULL);
        return;
    }

    if (!session_slot_active(session) || session->was_disconnected ||
            session->destroy_scheduled || session->destroying) {
        clear_session_slot(session);
        if (gatt_session_schedule_destroy(session) != ESP_OK) {
            ESP_LOGE(TAG, "schedule_destroy failed after disconnect race");
            portENTER_CRITICAL(&g_sessions_lock);
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (g_sessions[i] == NULL) {
                    g_sessions[i] = session;
                    break;
                }
            }
            portEXIT_CRITICAL(&g_sessions_lock);
        }
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = setup_gmp_session(session);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GMP Flux registration failed: %s", esp_err_to_name(err));
        clear_session_slot(session);
        if (gatt_session_schedule_destroy(session) != ESP_OK) {
            ESP_LOGE(TAG, "schedule_destroy failed after setup_gmp_session");
            portENTER_CRITICAL(&g_sessions_lock);
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (g_sessions[i] == NULL) {
                    g_sessions[i] = session;
                    break;
                }
            }
            portEXIT_CRITICAL(&g_sessions_lock);
        }
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        vTaskDelete(NULL);
        return;
    }

    if (!session_slot_active(session) || session->was_disconnected ||
            session->destroy_scheduled || session->destroying) {
        clear_session_slot(session);
        (void)esp_gmp_flux_link_unregister(session);
        if (gatt_session_schedule_destroy(session) != ESP_OK) {
            ESP_LOGE(TAG, "schedule_destroy failed after late disconnect");
            portENTER_CRITICAL(&g_sessions_lock);
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (g_sessions[i] == NULL) {
                    g_sessions[i] = session;
                    break;
                }
            }
            portEXIT_CRITICAL(&g_sessions_lock);
        }
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "GMP session ready conn=%d mtu=%u", conn_handle, (unsigned)mtu);
    vTaskDelete(NULL);
}

static void cache_local_chr_handles(void)
{
    esp_ble_conn_uuid_t svc_uuid = { .uuid128 = { GMP_BLE_UUID_SVC_BYTES } };
    esp_ble_conn_uuid_t rx_uuid = { .uuid128 = { GMP_BLE_UUID_CHR_RX_BYTES } };
    esp_ble_conn_uuid_t tx_uuid = { .uuid128 = { GMP_BLE_UUID_CHR_TX_BYTES } };
    uint16_t rx_handle = 0;
    uint16_t tx_handle = 0;

    if (esp_ble_conn_get_chr_handle(BLE_CONN_UUID_TYPE_128, svc_uuid,
                                    BLE_CONN_UUID_TYPE_128, rx_uuid,
                                    0, &rx_handle) != ESP_OK ||
            esp_ble_conn_get_chr_handle(BLE_CONN_UUID_TYPE_128, svc_uuid,
                                        BLE_CONN_UUID_TYPE_128, tx_uuid,
                                        0, &tx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "GMP GATT handles are not available");
        return;
    }

    g_chr_rx_handle = rx_handle;
    g_chr_tx_handle = tx_handle;
    ESP_LOGI(TAG, "GMP GATT ready rx=0x%04X tx=0x%04X", rx_handle, tx_handle);
}

static void on_connected(uint16_t conn_handle)
{
    if (g_chr_rx_handle == 0 || g_chr_tx_handle == 0) {
        ESP_LOGW(TAG, "GATT handles not ready");
        return;
    }

    uint16_t mtu = BLE_ATT_MTU_DFLT;
    if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) != ESP_OK ||
            mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }

    gatt_session_t *session = gatt_session_create(conn_handle, NULL,
                                                  g_chr_tx_handle, g_chr_rx_handle,
                                                  mtu, GATT_SESSION_ROLE_SLAVE);
    if (!session) {
        ESP_LOGE(TAG, "gatt_session_create failed");
        return;
    }

    int slot = -1;
    portENTER_CRITICAL(&g_sessions_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_sessions[i]) {
            g_sessions[i] = session;
            g_notify_enabled[i] = false;
            slot = i;
            break;
        }
    }
    portEXIT_CRITICAL(&g_sessions_lock);
    if (slot < 0) {
        ESP_LOGW(TAG, "max connections (%d) reached, rejecting conn=%d",
                 MAX_CONNECTIONS, conn_handle);
        gatt_session_schedule_destroy(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    /* Defer GMP register until ATT MTU exchange finishes (default/0 is too small). */
    if (xTaskCreate(activate_session_task, "gmp_srv_up", 4096, session, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to schedule session activation");
        clear_session_slot(session);
        gatt_session_schedule_destroy(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
    }
}

static void on_disconnected(uint16_t conn_handle)
{
    gatt_session_t *session = NULL;

    portENTER_CRITICAL(&g_sessions_lock);
    int idx = find_session_idx(conn_handle);
    if (idx >= 0) {
        session = g_sessions[idx];
        g_sessions[idx] = NULL;
        g_notify_enabled[idx] = false;
    }
    portEXIT_CRITICAL(&g_sessions_lock);
    if (!session) {
        return;
    }
    if (xTaskCreate(teardown_session_task, "gmp_srv_teardown", 4096, session, 4, NULL) != pdPASS &&
            !teardown_pending_push(session)) {
        ESP_LOGE(TAG, "failed to schedule teardown for conn=%d", conn_handle);
    }
    ESP_LOGI(TAG, "Disconnected conn=%d", conn_handle);
}

static void app_ble_conn_event_handler(void *handler_args, esp_event_base_t base,
                                       int32_t id, void *event_data)
{
    (void)handler_args;

    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_STARTED:
        cache_local_chr_handles();
        break;
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        on_connected(evt->connected.conn_handle);
        break;
    }
    case ESP_BLE_CONN_EVENT_DISCONNECTED: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        on_disconnected(evt->disconnected.conn_handle);
        break;
    }
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE: {
        esp_ble_conn_data_t *conn_data = event_data;

        if (!conn_data) {
            break;
        }
        if (conn_data->data && conn_data->data_len) {
            (void)gatt_session_feed_data_by_handle(conn_data->write_conn_id,
                                                   conn_data->data,
                                                   conn_data->data_len);
        }
        free(conn_data->data);
        conn_data->data = NULL;
        break;
    }
    case ESP_BLE_CONN_EVENT_MTU: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        (void)gatt_session_set_mtu_by_handle(evt->mtu_update.conn_handle,
                                             evt->mtu_update.mtu);
        break;
    }
    case ESP_BLE_CONN_EVENT_CCCD_UPDATE: {
        esp_ble_conn_cccd_update_t *cccd = event_data;
        const uint8_t tx_uuid[BLE_UUID128_VAL_LEN] = { GMP_BLE_UUID_CHR_TX_BYTES };

        if (!cccd || cccd->uuid_type != BLE_CONN_UUID_TYPE_128 ||
                memcmp(cccd->uuid.uuid128, tx_uuid, sizeof(tx_uuid)) != 0) {
            break;
        }
        portENTER_CRITICAL(&g_sessions_lock);
        int idx = find_session_idx(cccd->conn_handle);
        if (idx >= 0) {
            g_notify_enabled[idx] = cccd->notify_enable;
        }
        portEXIT_CRITICAL(&g_sessions_lock);
        ESP_LOGI(TAG, "CCCD notify=%d conn=%d", cccd->notify_enable, cccd->conn_handle);
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
        .device_name = GMP_BLE_DEVICE_NAME,
    };

    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    esp_log_level_set("blecm_nimble", ESP_LOG_WARN);
    esp_log_level_set("gatt_session", ESP_LOG_WARN);
    esp_log_level_set("esp_flux", ESP_LOG_WARN);

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_gmp_init();
    ESP_ERROR_CHECK(esp_gmp_os_init());
    ESP_ERROR_CHECK(esp_gmp_ota_init(NULL));

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_adv_data_set(gmp_adv_data, sizeof(gmp_adv_data)));
    ESP_ERROR_CHECK(esp_ble_conn_add_svc(&gmp_svc));
    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        return;
    }

    ESP_LOGI(TAG, "GMP OTA server ready (%s)", GMP_BLE_DEVICE_NAME);

    while (1) {
        esp_gmp_poll();
        process_pending_teardowns();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
