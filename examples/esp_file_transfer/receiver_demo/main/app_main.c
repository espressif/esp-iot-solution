/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "ble_manager.h"
#include "esp_gmp_ft.h"
#include "file_transfer_example_common.h"
#include "flux_gatt_session.h"

static const char *TAG = "ft_receiver_demo";

static ble_manager_t *s_manager;
static gatt_session_t *s_session;
static gatt_session_t *s_teardown_pending;
static bool s_tearing_down;
static bool s_adv_pending;
static portMUX_TYPE s_session_lock = portMUX_INITIALIZER_UNLOCKED;
static uint16_t s_rx_handle;
static uint16_t s_tx_handle;

static const ble_uuid128_t s_service_uuid =
    FILE_TRANSFER_EXAMPLE_SERVICE_UUID;
static const ble_uuid128_t s_rx_uuid = FILE_TRANSFER_EXAMPLE_RX_UUID;
static const ble_uuid128_t s_tx_uuid = FILE_TRANSFER_EXAMPLE_TX_UUID;

static void start_advertising(void);

static void teardown_session(gatt_session_t *session)
{
    if (!session) {
        return;
    }
    file_transfer_example_link_down(session);
    esp_err_t err = gatt_session_schedule_destroy(session);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to schedule session destroy: %s",
                 esp_err_to_name(err));
    }
}

static void teardown_task(void *arg)
{
    teardown_session((gatt_session_t *)arg);
    portENTER_CRITICAL(&s_session_lock);
    s_tearing_down = false;
    s_adv_pending = true;
    portEXIT_CRITICAL(&s_session_lock);
    vTaskDelete(NULL);
}

static void process_pending_teardown(void)
{
    gatt_session_t *session;
    bool start_adv = false;
    portENTER_CRITICAL(&s_session_lock);
    session = s_teardown_pending;
    s_teardown_pending = NULL;
    portEXIT_CRITICAL(&s_session_lock);
    if (session) {
        teardown_session(session);
        portENTER_CRITICAL(&s_session_lock);
        s_tearing_down = false;
        s_adv_pending = true;
        portEXIT_CRITICAL(&s_session_lock);
    }
    portENTER_CRITICAL(&s_session_lock);
    if (s_adv_pending && !s_tearing_down && !s_teardown_pending && !s_session) {
        s_adv_pending = false;
        start_adv = true;
    }
    portEXIT_CRITICAL(&s_session_lock);
    if (start_adv) {
        start_advertising();
    }
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = { 0 };
    struct ble_hs_adv_fields response = { 0 };
    struct ble_gap_adv_params params = { 0 };
    const char *name = FILE_TRANSFER_EXAMPLE_DEVICE_NAME;

    /* Ensure GAP local name matches what the sender filters on. */
    (void)ble_svc_gap_device_name_set(name);

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set advertising fields: rc=%d", rc);
        return;
    }

    response.uuids128 = (ble_uuid128_t *)&s_service_uuid;
    response.num_uuids128 = 1;
    response.uuids128_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set scan response: rc=%d", rc);
        return;
    }

    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    esp_err_t err = ble_manager_start_advertising(s_manager, &params, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start advertising: %s",
                 esp_err_to_name(err));
    }
}

static void activate_link_task(void *arg)
{
    (void)arg;
    gatt_session_t *session;
    uint16_t conn_handle;

    portENTER_CRITICAL(&s_session_lock);
    session = s_session;
    portEXIT_CRITICAL(&s_session_lock);
    if (!session) {
        vTaskDelete(NULL);
        return;
    }
    conn_handle = session->conn_handle;

    const uint16_t need_mtu = 185; /* enough for META + Flux/GMP headers */
    uint16_t mtu = BLE_ATT_MTU_DFLT;
    esp_err_t err = ESP_FAIL;

    for (int i = 0; i < 40; i++) {
        bool alive = false;
        portENTER_CRITICAL(&s_session_lock);
        alive = (s_session == session && !session->was_disconnected &&
                 !session->destroy_scheduled && !session->destroying);
        portEXIT_CRITICAL(&s_session_lock);
        if (!alive) {
            vTaskDelete(NULL);
            return;
        }
        if (ble_manager_get_mtu_by_conn(s_manager, conn_handle, &mtu) == ESP_OK &&
                mtu >= need_mtu) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (mtu < need_mtu) {
        ESP_LOGW(TAG, "MTU still low (%u); trying FT init anyway", (unsigned)mtu);
    }

    bool proceed = false;
    portENTER_CRITICAL(&s_session_lock);
    proceed = (s_session == session && !session->was_disconnected &&
               !session->destroy_scheduled && !session->destroying);
    portEXIT_CRITICAL(&s_session_lock);
    if (!proceed) {
        bool owned = false;
        portENTER_CRITICAL(&s_session_lock);
        if (s_session == session) {
            s_session = NULL;
            owned = true;
            s_tearing_down = true;
        }
        portEXIT_CRITICAL(&s_session_lock);
        if (owned) {
            if (xTaskCreate(teardown_task, "ft_link_down", 4096,
                            session, 4, NULL) != pdPASS) {
                portENTER_CRITICAL(&s_session_lock);
                s_teardown_pending = session;
                portEXIT_CRITICAL(&s_session_lock);
            }
            ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        vTaskDelete(NULL);
        return;
    }

    (void)gatt_session_set_mtu(session, mtu);

    ble_session_callbacks_t callbacks;
    err = file_transfer_example_get_session_callbacks(session, &callbacks);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "callbacks failed: 0x%x (%s)", (unsigned)err, esp_err_to_name(err));
    } else {
        session->callbacks = callbacks;
        err = file_transfer_example_link_up(session);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "link_up failed: 0x%x (%s) mtu=%u",
                     (unsigned)err, esp_err_to_name(err), (unsigned)mtu);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to activate file transfer link: 0x%x (%s)",
                 (unsigned)err, esp_err_to_name(err));
        bool owned = false;
        portENTER_CRITICAL(&s_session_lock);
        if (s_session == session) {
            s_session = NULL;
            owned = true;
            s_tearing_down = true;
        }
        portEXIT_CRITICAL(&s_session_lock);
        if (owned) {
            if (xTaskCreate(teardown_task, "ft_link_down", 4096,
                            session, 4, NULL) != pdPASS) {
                portENTER_CRITICAL(&s_session_lock);
                s_teardown_pending = session;
                portEXIT_CRITICAL(&s_session_lock);
            }
            ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Receiver link ready, conn=%u, mtu=%u",
             conn_handle, (unsigned)mtu);
    vTaskDelete(NULL);
}

static void on_connect(ble_manager_t *manager, uint8_t type,
                       ble_connection_t *connection, int status, void *arg)
{
    (void)manager;
    (void)status;
    (void)arg;

    if (type != BLE_EVENT_TYPE_CONNECT_COMPLETE &&
            type != BLE_EVENT_TYPE_RECONNECT_COMPLETE) {
        return;
    }
    if (!s_rx_handle || !s_tx_handle) {
        ESP_LOGE(TAG, "Data channel handles are not ready");
        ble_gap_terminate(connection->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    uint16_t mtu = BLE_ATT_MTU_DFLT;
    esp_err_t mtu_err = ble_manager_get_mtu_by_conn(
                            s_manager, connection->conn_handle, &mtu);
    if (mtu_err != ESP_OK || mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }
    gatt_session_t *session = gatt_session_create(
                                  connection->conn_handle, NULL,
                                  s_tx_handle, s_rx_handle, mtu,
                                  GATT_SESSION_ROLE_SLAVE);
    if (!session) {
        ESP_LOGE(TAG, "Failed to create link session");
        ble_gap_terminate(connection->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    portENTER_CRITICAL(&s_session_lock);
    if (s_session || s_teardown_pending || s_tearing_down) {
        portEXIT_CRITICAL(&s_session_lock);
        ESP_LOGE(TAG, "session slot busy");
        gatt_session_schedule_destroy(session);
        ble_gap_terminate(connection->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        return;
    }
    s_session = session;
    portEXIT_CRITICAL(&s_session_lock);

    /* Defer FT init until ATT MTU exchange finishes (default 23 is too small). */
    if (xTaskCreate(activate_link_task, "ft_rx_up", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to schedule link activation");
        portENTER_CRITICAL(&s_session_lock);
        if (s_session == session) {
            s_session = NULL;
        }
        s_tearing_down = true;
        portEXIT_CRITICAL(&s_session_lock);
        if (xTaskCreate(teardown_task, "ft_link_down", 4096,
                        session, 4, NULL) != pdPASS) {
            portENTER_CRITICAL(&s_session_lock);
            s_teardown_pending = session;
            portEXIT_CRITICAL(&s_session_lock);
        }
        ble_gap_terminate(connection->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
}

static void on_disconnect(ble_manager_t *manager,
                          ble_connection_t *connection,
                          int status, void *arg)
{
    (void)manager;
    (void)status;
    (void)arg;

    gatt_session_t *session = NULL;
    portENTER_CRITICAL(&s_session_lock);
    if (s_session &&
            s_session->conn_handle == connection->conn_handle) {
        session = s_session;
        s_session = NULL;
    }
    if (session) {
        s_tearing_down = true;
    }
    portEXIT_CRITICAL(&s_session_lock);

    if (session &&
            xTaskCreate(teardown_task, "ft_link_down", 4096,
                        session, 4, NULL) != pdPASS) {
        portENTER_CRITICAL(&s_session_lock);
        s_teardown_pending = session;
        portEXIT_CRITICAL(&s_session_lock);
    }
    ESP_LOGI(TAG, "Disconnected, conn=%u", connection->conn_handle);
    /* Advertising restarts after teardown completes (see process_pending_teardown). */
}

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        {
            {
                .uuid = &s_rx_uuid.u,
                .access_cb = gatt_session_gatt_handler,
                .val_handle = &s_rx_handle,
                .flags = BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_WRITE_NO_RSP,
            }, {
                .uuid = &s_tx_uuid.u,
                .access_cb = gatt_session_gatt_handler,
                .val_handle = &s_tx_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

static int gatt_server_init(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(s_services);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(s_services);
    }
    return rc;
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_store_config_init(void);

static void host_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    /* Host reset clears advertising; re-arm so the receiver stays discoverable. */
    portENTER_CRITICAL(&s_session_lock);
    if (!s_session && !s_tearing_down && !s_teardown_pending) {
        s_adv_pending = true;
    }
    portEXIT_CRITICAL(&s_session_lock);
}

static void configure_host(void)
{
    ble_hs_cfg.reset_cb = NULL;
    ble_hs_cfg.sync_cb = host_on_sync;
    ble_hs_cfg.store_status_cb = NULL;
    ble_store_config_init();
}

static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    init_nvs();
    ESP_ERROR_CHECK(file_transfer_example_init());
    ESP_ERROR_CHECK(nimble_port_init());
    configure_host();
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set(
                        FILE_TRANSFER_EXAMPLE_DEVICE_NAME));

    int rc = gatt_server_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT server initialization failed: rc=%d", rc);
        return;
    }
    nimble_port_freertos_init(host_task);

    const ble_manager_callbacks_t callbacks = {
        .ble_adv_cb = NULL,
        .ble_scan_cb = NULL,
        .ble_connect_cb = on_connect,
        .ble_disconnect_cb = on_disconnect,
        .ble_conn_update_cb = NULL,
        .arg = NULL,
    };
    s_manager = ble_manager_init(&callbacks);
    if (!s_manager) {
        ESP_LOGE(TAG, "Link manager initialization failed");
        return;
    }

    while (!ble_hs_synced()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    start_advertising();
    ESP_LOGI(TAG, "Receiver demo ready");

    while (true) {
        file_transfer_example_poll();
        process_pending_teardown();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
