/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_att.h"

#include "esp_ble_conn_mgr.h"
#include "file_transfer_example_common.h"
#include "flux_gatt_session.h"

static const char *TAG = "ft_sender_demo";

#define LL_PACKET_TIME      2120
#define LL_PACKET_LENGTH    251
#define OPTIMAL_MTU_SIZE    517

typedef struct {
    gatt_session_t *session;
    uint32_t generation;
} notify_context_t;

static gatt_session_t *s_session;
static gatt_session_t *s_teardown_pending;
static bool s_scan_pending;
static bool s_tearing_down;
static TickType_t s_scan_retry_at;
static uint32_t s_generation;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t start_scan(void)
{
    esp_err_t err = esp_ble_conn_scan_start();

    /* ESP_ERR_INVALID_STATE means a scan or connect procedure is already running. */
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

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
    TickType_t now = xTaskGetTickCount();
    portENTER_CRITICAL(&s_state_lock);
    if (s_scan_pending && !s_session && !s_tearing_down && !s_teardown_pending &&
            (s_scan_retry_at == 0 || (int32_t)(now - s_scan_retry_at) >= 0)) {
        s_scan_pending = false;
        s_scan_retry_at = 0;
        start = true;
    }
    portEXIT_CRITICAL(&s_state_lock);
    if (start && start_scan() != ESP_OK) {
        portENTER_CRITICAL(&s_state_lock);
        s_scan_pending = true;
        s_scan_retry_at = xTaskGetTickCount() + pdMS_TO_TICKS(1000);
        portEXIT_CRITICAL(&s_state_lock);
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
        if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) == ESP_OK &&
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
            (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        }
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Sender link ready, conn=%u, mtu=%u",
             conn_handle, gatt_session_get_mtu(session));
    vTaskDelete(NULL);
}

static void tune_connection(uint16_t conn_handle)
{
    esp_ble_conn_params_t params = {
        .itvl_min = 24,
        .itvl_max = 24,
        .latency = 0,
        .supervision_timeout = 300,
        .min_ce_len = 0,
        .max_ce_len = 0,
    };
    esp_err_t err;

    err = esp_ble_conn_set_data_len(conn_handle, LL_PACKET_LENGTH, LL_PACKET_TIME);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Set data length failed: %s", esp_err_to_name(err));
    }
    err = esp_ble_conn_mtu_update(conn_handle, OPTIMAL_MTU_SIZE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Set MTU failed: %s", esp_err_to_name(err));
    }
    err = esp_ble_conn_set_preferred_phy(conn_handle, ESP_BLE_CONN_PHY_MASK_2M,
                                         ESP_BLE_CONN_PHY_MASK_2M, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Set preferred phy failed: %s", esp_err_to_name(err));
    }
    err = esp_ble_conn_update_params(conn_handle, &params);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Update connection params failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t enable_notifications(uint16_t conn_handle)
{
    uint8_t notify_en[2] = { 1, 0 };
    esp_ble_conn_data_t sub = {
        .type = BLE_CONN_UUID_TYPE_128,
        .uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_TX_UUID_BYTES } },
        .data = notify_en,
        .data_len = sizeof(notify_en),
    };

    return esp_ble_conn_subscribe_by_handle(conn_handle,
                                            ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);
}

static void on_disc_complete(uint16_t conn_handle)
{
    esp_ble_conn_uuid_t svc_uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_SERVICE_UUID_BYTES } };
    esp_ble_conn_uuid_t rx_uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_RX_UUID_BYTES } };
    esp_ble_conn_uuid_t tx_uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_TX_UUID_BYTES } };
    uint16_t rx_handle = 0;
    uint16_t tx_handle = 0;
    uint16_t mtu = BLE_ATT_MTU_DFLT;

    if (esp_ble_conn_get_peer_chr_handle(conn_handle,
                                         BLE_CONN_UUID_TYPE_128, svc_uuid,
                                         BLE_CONN_UUID_TYPE_128, rx_uuid,
                                         &rx_handle) != ESP_OK ||
            esp_ble_conn_get_peer_chr_handle(conn_handle,
                                             BLE_CONN_UUID_TYPE_128, svc_uuid,
                                             BLE_CONN_UUID_TYPE_128, tx_uuid,
                                             &tx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "File transfer data channel not found");
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) != ESP_OK ||
            mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }

    gatt_session_t *session = gatt_session_create(conn_handle, NULL,
                                                  rx_handle, tx_handle, mtu,
                                                  GATT_SESSION_ROLE_MASTER);
    if (!session) {
        ESP_LOGE(TAG, "Failed to create link session");
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    ble_session_callbacks_t callbacks;
    esp_err_t err = file_transfer_example_get_session_callbacks(session, &callbacks);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure link callbacks: %s", esp_err_to_name(err));
        teardown_session(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }
    session->callbacks = callbacks;

    uint32_t generation = 0;
    bool stored = false;
    portENTER_CRITICAL(&s_state_lock);
    if (!s_session) {
        s_session = session;
        generation = ++s_generation;
        stored = true;
    }
    portEXIT_CRITICAL(&s_state_lock);
    if (!stored) {
        ESP_LOGE(TAG, "session slot busy");
        teardown_session(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    notify_context_t *context = calloc(1, sizeof(*context));
    if (context) {
        context->session = session;
        context->generation = generation;
    }
    err = enable_notifications(conn_handle);
    if (err != ESP_OK || !context) {
        ESP_LOGE(TAG, "Failed to enable notifications: %s", esp_err_to_name(err));
        free(context);
        portENTER_CRITICAL(&s_state_lock);
        if (s_session == session) {
            s_session = NULL;
            ++s_generation;
        }
        portEXIT_CRITICAL(&s_state_lock);
        teardown_session(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    /* Defer FT init until ATT MTU exchange finishes (do not block the event loop). */
    if (xTaskCreate(activate_sender_link_task, "ft_tx_up", 4096,
                    context, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start sender activate task");
        free(context);
        portENTER_CRITICAL(&s_state_lock);
        if (s_session == session) {
            s_session = NULL;
            ++s_generation;
        }
        portEXIT_CRITICAL(&s_state_lock);
        teardown_session(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
    }
}

static void on_disconnected(uint16_t conn_handle)
{
    gatt_session_t *session = NULL;
    portENTER_CRITICAL(&s_state_lock);
    if (s_session && s_session->conn_handle == conn_handle) {
        session = s_session;
        s_session = NULL;
        ++s_generation;
    }
    if (session) {
        s_tearing_down = true;
    }
    s_scan_pending = true;
    portEXIT_CRITICAL(&s_state_lock);

    if (session &&
            xTaskCreate(teardown_task, "ft_link_down", 4096,
                        session, 4, NULL) != pdPASS) {
        portENTER_CRITICAL(&s_state_lock);
        s_teardown_pending = session;
        portEXIT_CRITICAL(&s_state_lock);
    }
    ESP_LOGI(TAG, "Disconnected, conn=%u", conn_handle);
}

static bool adv_has_service_uuid(const esp_ble_conn_scan_result_t *result)
{
    const uint8_t uuid[BLE_UUID128_VAL_LEN] = { FILE_TRANSFER_EXAMPLE_SERVICE_UUID_BYTES };
    const uint8_t ad_types[] = {
        ESP_BLE_CONN_ADV_TYPE_UUID128_INCOMP,
        ESP_BLE_CONN_ADV_TYPE_UUID128_COMPLETE,
    };

    for (size_t t = 0; t < sizeof(ad_types); t++) {
        const uint8_t *p = NULL;
        uint8_t len = 0;

        if (esp_ble_conn_parse_adv_data(result->adv_data, result->adv_data_len,
                                        ad_types[t], &p, &len) != ESP_OK) {
            continue;
        }
        for (uint8_t i = 0; i + BLE_UUID128_VAL_LEN <= len; i += BLE_UUID128_VAL_LEN) {
            if (memcmp(&p[i], uuid, sizeof(uuid)) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool scan_cb(const esp_ble_conn_scan_result_t *result, void *arg)
{
    (void)arg;

    if (!result || !adv_has_service_uuid(result)) {
        return false;
    }

    bool busy = false;
    portENTER_CRITICAL(&s_state_lock);
    busy = (s_session || s_tearing_down || s_teardown_pending);
    portEXIT_CRITICAL(&s_state_lock);
    if (busy) {
        return false;
    }

    ESP_LOGI(TAG, "Receiver demo found " MACSTR " rssi=%d; connecting",
             MAC2STR(result->addr), result->rssi);
    return true;
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
        portENTER_CRITICAL(&s_state_lock);
        s_scan_pending = true;
        portEXIT_CRITICAL(&s_state_lock);
        break;
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        ESP_LOGI(TAG, "Connected to " MACSTR " (conn=%u)",
                 MAC2STR(evt->connected.peer_addr), evt->connected.conn_handle);
        tune_connection(evt->connected.conn_handle);
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
    case ESP_BLE_CONN_EVENT_DISC_COMPLETE: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        on_disc_complete(evt->disc_complete.conn_handle);
        break;
    }
    case ESP_BLE_CONN_EVENT_CONNECT_FAILED:
        portENTER_CRITICAL(&s_state_lock);
        s_scan_pending = true;
        s_scan_retry_at = 0;
        portEXIT_CRITICAL(&s_state_lock);
        break;
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE: {
        /* gatt_session feeds Flux from the GAP listener; this event owns a
         * separate copy which the application must free. */
        esp_ble_conn_data_t *conn_data = event_data;
        if (conn_data) {
            free(conn_data->data);
            conn_data->data = NULL;
        }
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
    default:
        break;
    }
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
    esp_ble_conn_config_t config = {
        .device_name = "ESP-File-Transfer-Sender",
    };

    init_nvs();
    ESP_ERROR_CHECK(file_transfer_example_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_register_scan_callback(scan_cb, NULL));
    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        return;
    }

    ESP_LOGI(TAG, "Sender demo ready; waiting for receiver");

    while (true) {
        file_transfer_example_poll();
        process_pending_teardown();
        process_pending_scan();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
