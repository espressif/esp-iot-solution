/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_att.h"

#include "esp_ble_conn_mgr.h"
#include "esp_flux_transport.h"
#include "file_transfer_example_common.h"
#include "flux_gatt_session.h"

static const char *TAG = "ft_receiver_demo";

static gatt_session_t *s_session;
static gatt_session_t *s_teardown_pending;
static bool s_tearing_down;
static bool s_adv_pending;
static bool s_notify_enabled;
static portMUX_TYPE s_session_lock = portMUX_INITIALIZER_UNLOCKED;
static uint16_t s_rx_handle;
static uint16_t s_tx_handle;

static esp_ble_conn_character_t s_ft_chars[] = {
    {
        .name = "ft_rx",
        .type = BLE_CONN_UUID_TYPE_128,
        .flag = BLE_CONN_GATT_CHR_WRITE | BLE_CONN_GATT_CHR_WRITE_NO_RSP,
        .uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_RX_UUID_BYTES } },
        .uuid_fn = NULL,
    },
    {
        .name = "ft_tx",
        .type = BLE_CONN_UUID_TYPE_128,
        .flag = BLE_CONN_GATT_CHR_NOTIFY,
        .uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_TX_UUID_BYTES } },
        .uuid_fn = NULL,
    },
};

static const esp_ble_conn_svc_t s_ft_svc = {
    .type = BLE_CONN_UUID_TYPE_128,
    .uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_SERVICE_UUID_BYTES } },
    .nu_lookup_count = sizeof(s_ft_chars) / sizeof(s_ft_chars[0]),
    .nu_lookup = s_ft_chars,
};

/* Legacy advertising is 31 bytes. Flags plus the 128-bit service UUID fit in
 * the connectable packet; the device name does not, so it stays in the scan
 * response. The sender matches the UUID on the connectable report. */
static const uint8_t s_adv_data[] = {
    0x02, ESP_BLE_CONN_ADV_TYPE_FLAGS, 0x06,
    BLE_UUID128_VAL_LEN + 1, ESP_BLE_CONN_ADV_TYPE_UUID128_COMPLETE,
    FILE_TRANSFER_EXAMPLE_SERVICE_UUID_BYTES,
};

static void set_scan_response(void)
{
    const char *name = FILE_TRANSFER_EXAMPLE_DEVICE_NAME;
    const size_t name_len = strlen(name);
    uint8_t rsp[ESP_BLE_CONN_ADV_DATA_MAX_LEN];

    if (name_len + 2 > sizeof(rsp)) {
        ESP_LOGE(TAG, "Device name does not fit in a scan response");
        return;
    }
    rsp[0] = (uint8_t)(name_len + 1);
    rsp[1] = ESP_BLE_CONN_ADV_TYPE_NAME_COMPLETE;
    memcpy(&rsp[2], name, name_len);
    ESP_ERROR_CHECK(esp_ble_conn_scan_rsp_data_set(rsp, name_len + 2));
}

static void start_advertising(void)
{
    esp_err_t err = esp_ble_conn_adv_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to start advertising: %s", esp_err_to_name(err));
    }
}

static void cache_local_chr_handles(void)
{
    esp_ble_conn_uuid_t svc_uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_SERVICE_UUID_BYTES } };
    esp_ble_conn_uuid_t rx_uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_RX_UUID_BYTES } };
    esp_ble_conn_uuid_t tx_uuid = { .uuid128 = { FILE_TRANSFER_EXAMPLE_TX_UUID_BYTES } };
    uint16_t rx_handle = 0;
    uint16_t tx_handle = 0;

    if (esp_ble_conn_get_chr_handle(BLE_CONN_UUID_TYPE_128, svc_uuid,
                                    BLE_CONN_UUID_TYPE_128, rx_uuid,
                                    0, &rx_handle) != ESP_OK ||
            esp_ble_conn_get_chr_handle(BLE_CONN_UUID_TYPE_128, svc_uuid,
                                        BLE_CONN_UUID_TYPE_128, tx_uuid,
                                        0, &tx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Data channel handles are not available");
        return;
    }

    s_rx_handle = rx_handle;
    s_tx_handle = tx_handle;
    ESP_LOGI(TAG, "Data channel ready, rx=0x%04X, tx=0x%04X", rx_handle, tx_handle);
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
    bool notify_enabled = false;
    esp_err_t err = ESP_FAIL;

    for (int i = 0; i < 40; i++) {
        bool alive = false;
        portENTER_CRITICAL(&s_session_lock);
        alive = (s_session == session && !session->was_disconnected &&
                 !session->destroy_scheduled && !session->destroying);
        notify_enabled = s_notify_enabled;
        portEXIT_CRITICAL(&s_session_lock);
        if (!alive) {
            vTaskDelete(NULL);
            return;
        }
        if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) == ESP_OK &&
                mtu >= need_mtu && notify_enabled) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (mtu < need_mtu) {
        ESP_LOGW(TAG, "MTU still low (%u); trying FT init anyway", (unsigned)mtu);
    }
    if (!notify_enabled) {
        ESP_LOGW(TAG, "Peer has not subscribed yet; trying FT init anyway");
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
            (void)esp_ble_conn_disconnect_by_handle(conn_handle);
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
            (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        }
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Receiver link ready, conn=%u, mtu=%u",
             conn_handle, (unsigned)mtu);
    vTaskDelete(NULL);
}

static void on_connected(uint16_t conn_handle)
{
    if (!s_rx_handle || !s_tx_handle) {
        ESP_LOGE(TAG, "Data channel handles are not ready");
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    uint16_t mtu = BLE_ATT_MTU_DFLT;
    if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) != ESP_OK ||
            mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }
    gatt_session_t *session = gatt_session_create(
                                  conn_handle, NULL,
                                  s_tx_handle, s_rx_handle, mtu,
                                  GATT_SESSION_ROLE_SLAVE);
    if (!session) {
        ESP_LOGE(TAG, "Failed to create link session");
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    portENTER_CRITICAL(&s_session_lock);
    if (s_session || s_teardown_pending || s_tearing_down) {
        portEXIT_CRITICAL(&s_session_lock);
        ESP_LOGE(TAG, "session slot busy");
        gatt_session_schedule_destroy(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
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
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
    }
}

static void on_disconnected(uint16_t conn_handle)
{
    gatt_session_t *session = NULL;
    portENTER_CRITICAL(&s_session_lock);
    if (s_session && s_session->conn_handle == conn_handle) {
        session = s_session;
        s_session = NULL;
    }
    if (session) {
        s_tearing_down = true;
    }
    s_notify_enabled = false;
    portEXIT_CRITICAL(&s_session_lock);

    if (session &&
            xTaskCreate(teardown_task, "ft_link_down", 4096,
                        session, 4, NULL) != pdPASS) {
        portENTER_CRITICAL(&s_session_lock);
        s_teardown_pending = session;
        portEXIT_CRITICAL(&s_session_lock);
    }
    ESP_LOGI(TAG, "Disconnected, conn=%u", conn_handle);
    /* Advertising restarts after teardown completes (see process_pending_teardown). */
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
        const uint8_t tx_uuid[BLE_UUID128_VAL_LEN] = { FILE_TRANSFER_EXAMPLE_TX_UUID_BYTES };

        if (!cccd || cccd->uuid_type != BLE_CONN_UUID_TYPE_128 ||
                memcmp(cccd->uuid.uuid128, tx_uuid, sizeof(tx_uuid)) != 0) {
            break;
        }
        portENTER_CRITICAL(&s_session_lock);
        s_notify_enabled = cccd->notify_enable;
        portEXIT_CRITICAL(&s_session_lock);
        ESP_LOGI(TAG, "CCCD notify=%d for conn=%u",
                 cccd->notify_enable, cccd->conn_handle);
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
        .device_name = FILE_TRANSFER_EXAMPLE_DEVICE_NAME,
    };

    init_nvs();
    ESP_ERROR_CHECK(file_transfer_example_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_adv_data_set(s_adv_data, sizeof(s_adv_data)));
    set_scan_response();
    ESP_ERROR_CHECK(esp_ble_conn_add_svc(&s_ft_svc));
    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        return;
    }

    ESP_LOGI(TAG, "Receiver demo ready");

    while (true) {
        file_transfer_example_poll();
        process_pending_teardown();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
