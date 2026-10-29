/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>

#include "esp_gmp_ft.h"
#include "esp_gmp_flux.h"
#include "esp_gmp_os.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"

#include "file_transfer_example_common.h"
#include "file_transfer_example_internal.h"

static const char *TAG = "ft_example";
static SemaphoreHandle_t s_runtime_mutex;
static esp_gmp_link_t s_link;
static bool s_file_transfer_ready;

static const char *event_name(esp_file_transfer_event_id_t event_id)
{
    switch (event_id) {
    case ESP_FT_EVENT_STARTED:
        return "started";
    case ESP_FT_EVENT_META_SENT:
        return "metadata sent";
    case ESP_FT_EVENT_META_RECEIVED:
        return "metadata received";
    case ESP_FT_EVENT_PEER_ACCEPTED:
        return "peer accepted";
    case ESP_FT_EVENT_PEER_REJECTED:
        return "peer rejected";
    case ESP_FT_EVENT_PROGRESS:
        return "progress";
    case ESP_FT_EVENT_VERIFYING:
        return "verifying";
    case ESP_FT_EVENT_COMPLETED:
        return "completed";
    case ESP_FT_EVENT_FAILED:
        return "failed";
    case ESP_FT_EVENT_CANCELLED:
        return "cancelled";
    default:
        return "unknown";
    }
}

static void on_file_transfer_event(const esp_file_transfer_event_t *event, void *ctx)
{
    (void)ctx;

    if (event->event_id == ESP_FT_EVENT_PROGRESS) {
        ESP_LOGI(TAG, "Progress: %u%% (%" PRIu64 "/%" PRIu64 ") %s",
                 event->percent, event->bytes_transferred, event->total_bytes,
                 event->file_name ? event->file_name : "");
        return;
    }

    if (event->event_id == ESP_FT_EVENT_COMPLETED) {
        ESP_LOGI(TAG, "Transfer complete: %s, saved=%s, bytes=%" PRIu64,
                 event->file_name ? event->file_name : "",
                 event->saved_name ? event->saved_name : "-",
                 event->bytes_transferred);
        return;
    }

    if (event->event_id == ESP_FT_EVENT_FAILED ||
            event->event_id == ESP_FT_EVENT_PEER_REJECTED) {
        ESP_LOGE(TAG, "Transfer %s: %s, reason=0x%04x, detail=0x%x",
                 event_name(event->event_id),
                 event->file_name ? event->file_name : "",
                 event->reason_code, (unsigned)event->detail);
        return;
    }

    ESP_LOGI(TAG, "Transfer %s: %s, reason=0x%04x",
             event_name(event->event_id),
             event->file_name ? event->file_name : "",
             event->reason_code);
}

static void on_session_complete(gatt_session_t *session, uint8_t stream_id,
                                esp_err_t status, const uint8_t *data, uint32_t size)
{
    (void)stream_id;
    (void)data;
    (void)size;

    /* Transport errors are also notified via GMP link events to FT. */
    if (status != ESP_OK) {
        ble_gap_terminate(session->conn_handle,
                          BLE_ERR_REM_USER_CONN_TERM);
    }
}

static void on_session_error(gatt_session_t *session, esp_err_t error)
{
    (void)error;
    ble_gap_terminate(session->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

bool file_transfer_example_runtime_lock(void)
{
    return s_runtime_mutex &&
           xSemaphoreTake(s_runtime_mutex, portMAX_DELAY) == pdTRUE;
}

void file_transfer_example_runtime_unlock(void)
{
    xSemaphoreGive(s_runtime_mutex);
}

bool file_transfer_example_runtime_ready_locked(void)
{
    return s_file_transfer_ready;
}

esp_gmp_link_t file_transfer_example_runtime_link_locked(void)
{
    return s_link;
}

esp_err_t file_transfer_example_init(void)
{
    esp_err_t err = file_transfer_example_storage_init();
    if (err != ESP_OK) {
        return err;
    }

    s_runtime_mutex = xSemaphoreCreateMutex();
    if (!s_runtime_mutex) {
        return ESP_ERR_NO_MEM;
    }

    esp_gmp_init();
    ESP_ERROR_CHECK(esp_gmp_os_init());

    err = file_transfer_example_console_init();
    if (err != ESP_OK) {
        return err;
    }
    return ESP_OK;
}

esp_err_t file_transfer_example_get_session_callbacks(
    gatt_session_t *session, ble_session_callbacks_t *callbacks)
{
    if (!session || !callbacks) {
        return ESP_ERR_INVALID_ARG;
    }

    const ble_session_callbacks_t user_callbacks = {
        .session_complete_cb = on_session_complete,
        .data_received_cb = NULL,
        .progress_cb = NULL,
        .error_cb = on_session_error,
        .arg = NULL,
    };
    return esp_gmp_flux_get_gatt_callbacks(session, callbacks, &user_callbacks);
}

esp_err_t file_transfer_example_link_up(gatt_session_t *session)
{
    flux_session_t *flux = gatt_session_get_flux(session);
    if (!session || !flux) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!file_transfer_example_runtime_lock()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_file_transfer_ready) {
        file_transfer_example_runtime_unlock();
        return ESP_FT_ERR_BUSY;
    }

    esp_err_t err = esp_gmp_flux_link_register(session, flux);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "flux_link_register failed: 0x%x (%s)",
                 (unsigned)err, esp_err_to_name(err));
        file_transfer_example_runtime_unlock();
        return err;
    }

    size_t effective = esp_gmp_max_payload_effective(session);
    ESP_LOGI(TAG, "link_up: mtu_eff_payload=%u", (unsigned)effective);

    const esp_file_transfer_config_t config = {
        .recv_dir = FILE_TRANSFER_EXAMPLE_RECV_DIR,
        .gmp_link = session,
        .max_file_size = FILE_TRANSFER_EXAMPLE_MAX_FILE_SIZE,
        .block_size = 0,
        .event_cb = on_file_transfer_event,
        .event_ctx = NULL,
    };
    err = esp_file_transfer_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_file_transfer_init failed: 0x%x (%s) payload=%u",
                 (unsigned)err, esp_err_to_name(err), (unsigned)effective);
        esp_gmp_flux_link_unregister(session);
        file_transfer_example_runtime_unlock();
        return err;
    }

    s_link = session;
    s_file_transfer_ready = true;
    file_transfer_example_runtime_unlock();
    ESP_LOGI(TAG, "File transfer link ready");
    return ESP_OK;
}

void file_transfer_example_link_down(gatt_session_t *session)
{
    if (!session || !file_transfer_example_runtime_lock()) {
        return;
    }

    if (s_file_transfer_ready && s_link == session) {
        /* Unregister first so profiles see LINK_DOWN and abort cleanly. */
        esp_gmp_flux_link_unregister(session);
        esp_err_t err = esp_file_transfer_deinit();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "File transfer deinit failed: %s",
                     esp_err_to_name(err));
            /* Keep ready/link so a later retry can tear down cleanly. */
        } else {
            s_file_transfer_ready = false;
            s_link = NULL;
        }
    } else {
        esp_gmp_flux_link_unregister(session);
    }
    file_transfer_example_runtime_unlock();
    ESP_LOGI(TAG, "File transfer link down");
}

bool file_transfer_example_link_ready(void)
{
    if (!file_transfer_example_runtime_lock()) {
        return false;
    }
    bool ready = s_file_transfer_ready;
    file_transfer_example_runtime_unlock();
    return ready;
}

void file_transfer_example_poll(void)
{
    esp_gmp_poll();
}
