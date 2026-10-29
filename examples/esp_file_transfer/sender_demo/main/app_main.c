/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

#include "ble_manager.h"
#include "file_transfer_example_common.h"
#include "flux_gatt_session.h"
#include "gatt_discovery.h"

static const char *TAG = "ft_sender_demo";

typedef struct {
    gatt_session_t *session;
    uint32_t generation;
} notify_context_t;

static ble_manager_t *s_manager;
static gatt_session_t *s_session;
static gatt_session_t *s_teardown_pending;
static bool s_connecting;
static bool s_scan_pending;
static bool s_tearing_down;
static uint32_t s_generation;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

static const ble_uuid128_t s_service_uuid =
    FILE_TRANSFER_EXAMPLE_SERVICE_UUID;
static const ble_uuid128_t s_rx_uuid = FILE_TRANSFER_EXAMPLE_RX_UUID;
static const ble_uuid128_t s_tx_uuid = FILE_TRANSFER_EXAMPLE_TX_UUID;

static esp_err_t start_scan(void);

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
    portENTER_CRITICAL(&s_state_lock);
    s_tearing_down = false;
    s_scan_pending = true;
    portEXIT_CRITICAL(&s_state_lock);
    vTaskDelete(NULL);
}

static void process_pending_teardown(void)
{
    gatt_session_t *session;
    portENTER_CRITICAL(&s_state_lock);
    session = s_teardown_pending;
    s_teardown_pending = NULL;
    portEXIT_CRITICAL(&s_state_lock);
    if (session) {
        teardown_session(session);
        portENTER_CRITICAL(&s_state_lock);
        s_tearing_down = false;
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
    }
}

static void process_pending_scan(void)
{
    bool start = false;
    portENTER_CRITICAL(&s_state_lock);
    if (s_scan_pending && !s_connecting && !s_session &&
            !s_tearing_down && !s_teardown_pending) {
        s_scan_pending = false;
        start = true;
    }
    portEXIT_CRITICAL(&s_state_lock);
    if (start) {
        esp_err_t err = start_scan();
        if (err != ESP_OK) {
            portENTER_CRITICAL(&s_state_lock);
            s_scan_pending = true;
            portEXIT_CRITICAL(&s_state_lock);
            ESP_LOGE(TAG, "Failed to restart scan: %s", esp_err_to_name(err));
        }
    }
}

static void activate_sender_link_task(void *arg)
{
    notify_context_t *context = (notify_context_t *)arg;
    if (!context || !context->session) {
        free(context);
        vTaskDelete(NULL);
        return;
    }

    gatt_session_t *session = context->session;
    uint32_t generation = context->generation;
    uint16_t conn_handle = session->conn_handle;
    free(context);
    context = NULL;

    bool current = false;
    portENTER_CRITICAL(&s_state_lock);
    current = (s_session == session && s_generation == generation);
    portEXIT_CRITICAL(&s_state_lock);
    if (!current) {
        vTaskDelete(NULL);
        return;
    }

    const uint16_t need_mtu = 185; /* enough for META + Flux/GMP headers */
    uint16_t mtu = BLE_ATT_MTU_DFLT;
    for (int i = 0; i < 40; i++) {
        portENTER_CRITICAL(&s_state_lock);
        current = (s_session == session && s_generation == generation &&
                   !session->was_disconnected && !session->destroy_scheduled &&
                   !session->destroying);
        portEXIT_CRITICAL(&s_state_lock);
        if (!current) {
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

    portENTER_CRITICAL(&s_state_lock);
    current = (s_session == session && s_generation == generation &&
               !session->was_disconnected && !session->destroy_scheduled &&
               !session->destroying);
    portEXIT_CRITICAL(&s_state_lock);
    if (!current) {
        vTaskDelete(NULL);
        return;
    }

    (void)gatt_session_set_mtu(session, mtu);

    esp_err_t err = file_transfer_example_link_up(session);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to activate file transfer link: %s",
                 esp_err_to_name(err));
        (void)ble_manager_stop_scan(s_manager);
        bool owned = false;
        portENTER_CRITICAL(&s_state_lock);
        if (s_session == session) {
            s_session = NULL;
            ++s_generation;
            owned = true;
            s_tearing_down = true;
            s_scan_pending = true;
        }
        portEXIT_CRITICAL(&s_state_lock);
        if (owned) {
            if (xTaskCreate(teardown_task, "ft_link_down", 4096,
                            session, 4, NULL) != pdPASS) {
                portENTER_CRITICAL(&s_state_lock);
                s_teardown_pending = session;
                portEXIT_CRITICAL(&s_state_lock);
            }
            ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        }
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Sender link ready, conn=%u, mtu=%u",
             conn_handle, gatt_session_get_mtu(session));
    vTaskDelete(NULL);
}

static int on_notification_enabled(uint16_t conn_handle,
                                   const struct ble_gatt_error *error,
                                   struct ble_gatt_attr *attr, void *arg)
{
    (void)attr;
    notify_context_t *context = (notify_context_t *)arg;
    bool current = false;

    portENTER_CRITICAL(&s_state_lock);
    current = context && s_session == context->session &&
              s_generation == context->generation &&
              s_session->conn_handle == conn_handle;
    portEXIT_CRITICAL(&s_state_lock);

    if (!current) {
        free(context);
        return 0;
    }

    int status = error ? error->status : BLE_HS_EUNKNOWN;
    if (status != 0) {
        ESP_LOGE(TAG, "Failed to enable data notifications: status=%d",
                 status);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        free(context);
        return 0;
    }

    /* Defer FT init until ATT MTU exchange finishes (do not block NimBLE host). */
    if (xTaskCreate(activate_sender_link_task, "ft_tx_up", 4096,
                    context, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start sender activate task");
        free(context);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

static esp_err_t enable_notifications(gatt_discovery_t *discovery,
                                      gatt_session_t *session,
                                      uint32_t generation)
{
    gatt_descriptor_t *descriptor =
        gatt_discovery_find_descriptor_by_uuid(
            discovery, &s_service_uuid.u, &s_tx_uuid.u,
            BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (!descriptor) {
        return ESP_ERR_NOT_FOUND;
    }

    notify_context_t *context = calloc(1, sizeof(*context));
    if (!context) {
        return ESP_ERR_NO_MEM;
    }
    context->session = session;
    context->generation = generation;

    const uint8_t value[2] = { 1, 0 };
    int rc = ble_gattc_write_flat(
                 discovery->conn_handle, descriptor->dsc.handle,
                 value, sizeof(value), on_notification_enabled, context);
    if (rc != 0) {
        free(context);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void clear_current_session(gatt_session_t *session)
{
    portENTER_CRITICAL(&s_state_lock);
    if (s_session == session) {
        s_session = NULL;
        ++s_generation;
    }
    portEXIT_CRITICAL(&s_state_lock);
}

static void on_discovery_complete(gatt_discovery_t *discovery,
                                  int status, void *arg)
{
    (void)arg;
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "Service discovery failed: status=%d", status);
        ble_gap_terminate(discovery->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    gatt_characteristic_t *rx =
        gatt_discovery_find_characteristic_by_uuid(
            discovery, &s_service_uuid.u, &s_rx_uuid.u);
    gatt_characteristic_t *tx =
        gatt_discovery_find_characteristic_by_uuid(
            discovery, &s_service_uuid.u, &s_tx_uuid.u);
    if (!rx || !tx) {
        ESP_LOGE(TAG, "File transfer data channel not found");
        ble_gap_terminate(discovery->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    uint16_t mtu = BLE_ATT_MTU_DFLT;
    esp_err_t mtu_err = ble_manager_get_mtu_by_conn(
                            s_manager, discovery->conn_handle, &mtu);
    if (mtu_err != ESP_OK || mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }
    gatt_session_t *session = gatt_session_create(
                                  discovery->conn_handle, NULL,
                                  rx->chr.val_handle, tx->chr.val_handle,
                                  mtu, GATT_SESSION_ROLE_MASTER);
    if (!session) {
        ESP_LOGE(TAG, "Failed to create link session");
        ble_gap_terminate(discovery->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    ble_session_callbacks_t callbacks;
    esp_err_t err = file_transfer_example_get_session_callbacks(
                        session, &callbacks);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure link callbacks: %s",
                 esp_err_to_name(err));
        file_transfer_example_link_down(session);
        gatt_session_schedule_destroy(session);
        ble_gap_terminate(discovery->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }
    session->callbacks = callbacks;

    uint32_t generation;
    bool stored = false;
    portENTER_CRITICAL(&s_state_lock);
    if (!s_session) {
        s_session = session;
        generation = ++s_generation;
        stored = true;
    }
    portEXIT_CRITICAL(&s_state_lock);
    if (!stored) {
        file_transfer_example_link_down(session);
        gatt_session_schedule_destroy(session);
        ble_gap_terminate(discovery->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    err = enable_notifications(discovery, session, generation);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable notifications: %s",
                 esp_err_to_name(err));
        clear_current_session(session);
        file_transfer_example_link_down(session);
        gatt_session_schedule_destroy(session);
        ble_gap_terminate(discovery->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
    }
    gatt_discovery_destroy(discovery);
}

static void on_connect(ble_manager_t *manager, uint8_t type,
                       ble_connection_t *connection, int status, void *arg)
{
    (void)manager;
    (void)arg;

    if (type == BLE_EVENT_TYPE_CONNECT_FAILED ||
            type == BLE_EVENT_TYPE_RECONNECT_FAILED) {
        portENTER_CRITICAL(&s_state_lock);
        s_connecting = false;
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
        ESP_LOGW(TAG, "Connection failed: status=%d; restarting scan",
                 status);
        return;
    }
    if (type != BLE_EVENT_TYPE_CONNECT_COMPLETE &&
            type != BLE_EVENT_TYPE_RECONNECT_COMPLETE) {
        return;
    }

    portENTER_CRITICAL(&s_state_lock);
    s_connecting = false;
    portEXIT_CRITICAL(&s_state_lock);

    const ble_discovery_callbacks_t callbacks = {
        .discovery_complete_cb = on_discovery_complete,
        .service_found_cb = NULL,
        .characteristic_found_cb = NULL,
        .descriptor_found_cb = NULL,
        .arg = NULL,
    };
    gatt_discovery_t *discovery =
        gatt_discovery_start(connection->conn_handle, &callbacks);
    if (!discovery) {
        ESP_LOGE(TAG, "Failed to start service discovery");
        ble_gap_terminate(connection->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        portENTER_CRITICAL(&s_state_lock);
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
        return;
    }
    esp_err_t err = gatt_discovery_discover_service_by_uuid(discovery,
                                                            &s_service_uuid.u);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start service discovery by UUID: %s",
                 esp_err_to_name(err));
        gatt_discovery_destroy(discovery);
        ble_gap_terminate(connection->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
        portENTER_CRITICAL(&s_state_lock);
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
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
    portENTER_CRITICAL(&s_state_lock);
    if (s_session &&
            s_session->conn_handle == connection->conn_handle) {
        session = s_session;
        s_session = NULL;
        ++s_generation;
    }
    s_connecting = false;
    portEXIT_CRITICAL(&s_state_lock);

    /* Stop discovery so on_scan cannot start a new connect during teardown. */
    (void)ble_manager_stop_scan(s_manager);

    if (session) {
        portENTER_CRITICAL(&s_state_lock);
        s_tearing_down = true;
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
        if (xTaskCreate(teardown_task, "ft_link_down", 4096,
                        session, 4, NULL) != pdPASS) {
            portENTER_CRITICAL(&s_state_lock);
            s_teardown_pending = session;
            portEXIT_CRITICAL(&s_state_lock);
        }
    } else {
        portENTER_CRITICAL(&s_state_lock);
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
    }
    ESP_LOGI(TAG, "Disconnected, conn=%u", connection->conn_handle);
}

static void on_scan(ble_manager_t *manager, uint8_t type,
                    const struct ble_gap_disc_desc *result, void *arg)
{
    (void)manager;
    (void)arg;
    if (type != BLE_GAP_EVENT_DISC) {
        return;
    }

    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, result->data,
                                result->length_data) != 0) {
        return;
    }

    bool name_match = false;
    if (fields.name && fields.name_len > 0) {
        size_t expected_len = strlen(FILE_TRANSFER_EXAMPLE_DEVICE_NAME);
        if (fields.name_len == expected_len &&
                memcmp(fields.name, FILE_TRANSFER_EXAMPLE_DEVICE_NAME,
                       expected_len) == 0) {
            name_match = true;
        }
    }

    bool uuid_match = false;
    for (uint8_t i = 0; i < fields.num_uuids128; i++) {
        if (ble_uuid_cmp(&fields.uuids128[i].u, &s_service_uuid.u) == 0) {
            uuid_match = true;
            break;
        }
    }
    if (!name_match && !uuid_match) {
        return;
    }

    bool connect = false;
    portENTER_CRITICAL(&s_state_lock);
    if (!s_connecting && !s_session && !s_tearing_down && !s_teardown_pending) {
        s_connecting = true;
        connect = true;
    }
    portEXIT_CRITICAL(&s_state_lock);
    if (!connect) {
        return;
    }

    ESP_LOGI(TAG, "Receiver demo found; connecting");
    esp_err_t err = ble_manager_connect(
                        s_manager, &result->addr, 24, 24, 0, 300);
    if (err != ESP_OK) {
        portENTER_CRITICAL(&s_state_lock);
        s_connecting = false;
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
        ESP_LOGE(TAG, "Connection request failed: %s",
                 esp_err_to_name(err));
    }
}

static esp_err_t start_scan(void)
{
    esp_err_t err = ble_manager_start_scan(s_manager, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(err));
    }
    return err;
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
    if (s_manager) {
        (void)ble_manager_stop_scan(s_manager);
        (void)start_scan();
    }
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
                        "ESP-File-Transfer-Sender"));
    nimble_port_freertos_init(host_task);

    const ble_manager_callbacks_t callbacks = {
        .ble_adv_cb = NULL,
        .ble_scan_cb = on_scan,
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
    start_scan();
    ESP_LOGI(TAG, "Sender demo ready; waiting for receiver");

    while (true) {
        file_transfer_example_poll();
        process_pending_teardown();
        process_pending_scan();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
