/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE central: discover ESP-GMP-OTA peripheral and push firmware from SPIFFS.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "host/ble_att.h"

#include "esp_ble_conn_mgr.h"
#include "flux_gatt_session.h"
#include "esp_gmp.h"
#include "esp_gmp_flux.h"
#include "esp_gmp_os.h"
#include "esp_gmp_ota_host.h"
#include "gmp_ble_uuids.h"
#include "gmp_session_teardown.h"
#include "sdkconfig.h"

static const char *TAG = "gmp_ota_cli";

#define MAX_CONNECTIONS CONFIG_BLE_CONN_MGR_MAX_CONNECTIONS
#define GMP_OTA_FIRMWARE_PATH "/spiffs/ota_image.bin"

#define LL_PACKET_TIME      2120
#define LL_PACKET_LENGTH    251
#define OPTIMAL_MTU_SIZE    517

static gatt_session_t *g_session;
static bool g_conn_params_ready;
static bool g_notify_enabled;
static bool g_ota_runner_scheduled;
static uint32_t g_conn_generation;
static gatt_session_t *g_teardown_pending[MAX_CONNECTIONS];
static portMUX_TYPE g_state_lock = portMUX_INITIALIZER_UNLOCKED;

typedef struct {
    gatt_session_t *session;
    uint32_t generation;
} conn_ready_fallback_arg_t;

static void maybe_start_ota(gatt_session_t *session);

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
    esp_gmp_flux_link_unregister(session);
}

static void ota_runner_set_scheduled(bool scheduled)
{
    portENTER_CRITICAL(&g_state_lock);
    g_ota_runner_scheduled = scheduled;
    portEXIT_CRITICAL(&g_state_lock);
}

static bool ota_runner_is_scheduled(void)
{
    bool scheduled;

    portENTER_CRITICAL(&g_state_lock);
    scheduled = g_ota_runner_scheduled;
    portEXIT_CRITICAL(&g_state_lock);
    return scheduled;
}

static bool session_is_current(gatt_session_t *session)
{
    bool current;

    portENTER_CRITICAL(&g_state_lock);
    current = (g_session == session);
    portEXIT_CRITICAL(&g_state_lock);
    return current;
}

static bool teardown_pending_push(gatt_session_t *session)
{
    bool stored = false;

    portENTER_CRITICAL(&g_state_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (!g_teardown_pending[i]) {
            g_teardown_pending[i] = session;
            stored = true;
            break;
        }
    }
    portEXIT_CRITICAL(&g_state_lock);
    return stored;
}

static gatt_session_t *teardown_pending_pop(void)
{
    gatt_session_t *session = NULL;

    portENTER_CRITICAL(&g_state_lock);
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (g_teardown_pending[i]) {
            session = g_teardown_pending[i];
            g_teardown_pending[i] = NULL;
            break;
        }
    }
    portEXIT_CRITICAL(&g_state_lock);
    return session;
}

static esp_err_t enable_tx_notify(uint16_t conn_handle)
{
    uint8_t value[2] = { 1, 0 };
    esp_ble_conn_data_t sub = {
        .type = BLE_CONN_UUID_TYPE_128,
        .uuid = { .uuid128 = { GMP_BLE_UUID_CHR_TX_BYTES } },
        .data = value,
        .data_len = sizeof(value),
    };

    return esp_ble_conn_subscribe_by_handle(conn_handle, ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);
}

static esp_err_t spiffs_firmware_read(void *ctx, size_t offset, uint8_t *buf, size_t len, size_t *out_len)
{
    FILE *fp = (FILE *)ctx;

    if (fseek(fp, (long)offset, SEEK_SET) != 0) {
        return ESP_FAIL;
    }

    size_t n = fread(buf, 1, len, fp);
    if (n != len) {
        return ESP_FAIL;
    }

    *out_len = n;
    return ESP_OK;
}

static esp_err_t mount_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 4,
        .format_if_mount_failed = false,
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
        return err;
    }

    size_t total = 0;
    size_t used = 0;
    esp_spiffs_info(conf.partition_label, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: total=%zu used=%zu", total, used);
    return ESP_OK;
}

static void ota_runner_task(void *arg)
{
    gatt_session_t *session = (gatt_session_t *)arg;

    vTaskDelay(pdMS_TO_TICKS(500));

    if (!session_is_current(session)) {
        ota_runner_set_scheduled(false);
        vTaskDelete(NULL);
        return;
    }

    struct stat st;
    if (stat(GMP_OTA_FIRMWARE_PATH, &st) != 0 || st.st_size <= 0) {
        ESP_LOGW(TAG, "Firmware not found at %s (see spiffs/README)", GMP_OTA_FIRMWARE_PATH);
        ota_runner_set_scheduled(false);
        vTaskDelete(NULL);
        return;
    }

    FILE *fp = fopen(GMP_OTA_FIRMWARE_PATH, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open %s", GMP_OTA_FIRMWARE_PATH);
        ota_runner_set_scheduled(false);
        vTaskDelete(NULL);
        return;
    }

    size_t image_len = (size_t)st.st_size;
    ESP_LOGI(TAG, "Streaming firmware from SPIFFS: %zu bytes", image_len);

    esp_err_t err = esp_gmp_ota_host_upload_stream(session, image_len, spiffs_firmware_read, fp);
    fclose(fp);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "OTA upload complete");
    } else if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "OTA upload cancelled");
    } else {
        ESP_LOGE(TAG, "OTA upload failed: %s", esp_err_to_name(err));
    }

    ota_runner_set_scheduled(false);
    vTaskDelete(NULL);
}

static void wait_ota_runner_done(void)
{
    while (ota_runner_is_scheduled()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void teardown_session(gatt_session_t *session)
{
    if (!session) {
        return;
    }

    wait_ota_runner_done();
    if (gmp_session_teardown(session, teardown_gmp_session) != ESP_OK) {
        ESP_LOGE(TAG, "failed to schedule session destroy conn=%d", session->conn_handle);
    }
}

static void teardown_session_task(void *arg)
{
    teardown_session((gatt_session_t *)arg);
    vTaskDelete(NULL);
}

static void maybe_start_ota(gatt_session_t *session)
{
    bool should_start = false;

    portENTER_CRITICAL(&g_state_lock);
    if (g_conn_params_ready && g_notify_enabled && !g_ota_runner_scheduled && session) {
        g_ota_runner_scheduled = true;
        should_start = true;
    }
    portEXIT_CRITICAL(&g_state_lock);

    if (!should_start) {
        return;
    }

    BaseType_t ok = xTaskCreate(ota_runner_task, "gmp_ota", 8192, session, 5, NULL);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create OTA runner");
        ota_runner_set_scheduled(false);
        return;
    }
}

static void conn_ready_fallback_task(void *arg)
{
    conn_ready_fallback_arg_t *fallback = (conn_ready_fallback_arg_t *)arg;
    bool should_start = false;

    vTaskDelay(pdMS_TO_TICKS(2000));
    portENTER_CRITICAL(&g_state_lock);
    if (!g_conn_params_ready && g_session == fallback->session &&
            g_conn_generation == fallback->generation) {
        g_conn_params_ready = true;
        should_start = true;
    }
    portEXIT_CRITICAL(&g_state_lock);

    if (should_start) {
        maybe_start_ota(fallback->session);
    }
    free(fallback);
    vTaskDelete(NULL);
}

static void process_pending_teardowns(void)
{
    gatt_session_t *session = NULL;

    while ((session = teardown_pending_pop()) != NULL) {
        teardown_session(session);
    }
}

static void start_scan(void)
{
    esp_err_t err = esp_ble_conn_scan_start();

    /* ESP_ERR_INVALID_STATE means a scan or connect procedure is already running. */
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(err));
    }
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

static void on_disc_complete(uint16_t conn_handle)
{
    esp_ble_conn_uuid_t svc_uuid = { .uuid128 = { GMP_BLE_UUID_SVC_BYTES } };
    esp_ble_conn_uuid_t rx_uuid = { .uuid128 = { GMP_BLE_UUID_CHR_RX_BYTES } };
    esp_ble_conn_uuid_t tx_uuid = { .uuid128 = { GMP_BLE_UUID_CHR_TX_BYTES } };
    uint16_t rx_handle = 0;
    uint16_t tx_handle = 0;

    if (esp_ble_conn_get_peer_chr_handle(conn_handle,
                                         BLE_CONN_UUID_TYPE_128, svc_uuid,
                                         BLE_CONN_UUID_TYPE_128, rx_uuid,
                                         &rx_handle) != ESP_OK ||
            esp_ble_conn_get_peer_chr_handle(conn_handle,
                                             BLE_CONN_UUID_TYPE_128, svc_uuid,
                                             BLE_CONN_UUID_TYPE_128, tx_uuid,
                                             &tx_handle) != ESP_OK) {
        ESP_LOGE(TAG, "GMP RX/TX characteristics not found");
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    uint16_t mtu = BLE_ATT_MTU_DFLT;
    if (esp_ble_conn_get_mtu_by_handle(conn_handle, &mtu) != ESP_OK || mtu < BLE_ATT_MTU_DFLT) {
        mtu = BLE_ATT_MTU_DFLT;
    }

    gatt_session_t *session = gatt_session_create(conn_handle, NULL,
                                                  rx_handle, tx_handle,
                                                  mtu, GATT_SESSION_ROLE_MASTER);
    if (!session) {
        ESP_LOGE(TAG, "gatt_session_create failed");
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    if (setup_gmp_callbacks(session) != ESP_OK) {
        ESP_LOGE(TAG, "setup GMP callbacks failed");
        gatt_session_schedule_destroy(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    if (setup_gmp_session(session) != ESP_OK) {
        ESP_LOGE(TAG, "GMP Flux registration failed");
        gatt_session_schedule_destroy(session);
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }

    uint32_t generation = 0;
    portENTER_CRITICAL(&g_state_lock);
    g_session = session;
    g_conn_params_ready = false;
    g_notify_enabled = false;
    g_conn_generation++;
    generation = g_conn_generation;
    portEXIT_CRITICAL(&g_state_lock);
    ESP_LOGI(TAG, "GMP client session conn=%d mtu=%u", conn_handle, (unsigned)mtu);

    esp_err_t err = enable_tx_notify(conn_handle);
    bool should_start = false;
    bool current = false;
    portENTER_CRITICAL(&g_state_lock);
    if (g_session == session && g_conn_generation == generation) {
        current = true;
        if (err == ESP_OK) {
            g_notify_enabled = true;
            should_start = g_conn_params_ready;
        } else {
            g_session = NULL;
            g_conn_params_ready = false;
            g_notify_enabled = false;
            g_conn_generation++;
        }
    }
    portEXIT_CRITICAL(&g_state_lock);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CCCD write failed conn=%d: %s", conn_handle, esp_err_to_name(err));
        if (current) {
            teardown_gmp_session(session);
            gatt_session_schedule_destroy(session);
        }
        (void)esp_ble_conn_disconnect_by_handle(conn_handle);
        return;
    }
    if (!current) {
        return;
    }

    conn_ready_fallback_arg_t *fallback = calloc(1, sizeof(*fallback));
    if (fallback) {
        fallback->session = session;
        fallback->generation = generation;
        if (xTaskCreate(conn_ready_fallback_task, "conn_fb", 2048, fallback, 3, NULL) != pdPASS) {
            free(fallback);
        }
    }

    if (should_start) {
        maybe_start_ota(session);
    }
}

static void on_conn_params_updated(uint16_t conn_handle, uint16_t itvl)
{
    gatt_session_t *session = NULL;

    portENTER_CRITICAL(&g_state_lock);
    if (g_session && g_session->conn_handle == conn_handle) {
        session = g_session;
        g_conn_params_ready = true;
    }
    portEXIT_CRITICAL(&g_state_lock);

    if (!session) {
        return;
    }

    ESP_LOGI(TAG, "Connection params ready (interval=%.2f ms)", itvl * 1.25f);
    maybe_start_ota(session);
}

static void on_disconnected(uint16_t conn_handle)
{
    gatt_session_t *session = NULL;

    portENTER_CRITICAL(&g_state_lock);
    if (g_session && g_session->conn_handle == conn_handle) {
        session = g_session;
        g_session = NULL;
        g_conn_params_ready = false;
        g_notify_enabled = false;
        g_conn_generation++;
    }
    portEXIT_CRITICAL(&g_state_lock);

    if (session) {
        esp_gmp_ota_host_cancel();
        if (xTaskCreate(teardown_session_task, "gmp_teardown", 4096, session, 4, NULL) != pdPASS) {
            if (!teardown_pending_push(session)) {
                ESP_LOGE(TAG, "failed to schedule teardown for conn=%d", conn_handle);
            }
        }
    }
    ESP_LOGI(TAG, "Disconnected conn=%d", conn_handle);

    start_scan();
}

static bool adv_has_service_uuid(const esp_ble_conn_scan_result_t *result)
{
    const uint8_t uuid[BLE_UUID128_VAL_LEN] = { GMP_BLE_UUID_SVC_BYTES };
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

    /* Match the service UUID, not the advertised name: another device may use
     * the same name. Active scanning lets NimBLE report the scan response,
     * where the server publishes this UUID. */
    if (!result || !adv_has_service_uuid(result)) {
        return false;
    }
    if (!session_is_current(NULL)) {
        /* Already linked to a GMP OTA device. */
        return false;
    }

    ESP_LOGI(TAG, "Found GMP OTA device " MACSTR ", connecting...", MAC2STR(result->addr));
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
        start_scan();
        break;
    case ESP_BLE_CONN_EVENT_CONNECTED: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        ESP_LOGI(TAG, "Connected to " MACSTR " (conn=%d)",
                 MAC2STR(evt->connected.peer_addr), evt->connected.conn_handle);
        tune_connection(evt->connected.conn_handle);
        break;
    }
    case ESP_BLE_CONN_EVENT_CONNECT_FAILED: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        ESP_LOGW(TAG, "Connection to " MACSTR " failed (status=%d); resuming scan",
                 MAC2STR(evt->connect_failed.peer_addr), evt->connect_failed.status);
        if (session_is_current(NULL)) {
            start_scan();
        }
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
    case ESP_BLE_CONN_EVENT_CONN_PARAM_UPDATE: {
        esp_ble_conn_event_data_t *evt = event_data;

        if (!evt) {
            break;
        }
        on_conn_params_updated(evt->conn_param_update.conn_handle,
                               evt->conn_param_update.params.itvl_min);
        break;
    }
    case ESP_BLE_CONN_EVENT_DATA_RECEIVE: {
        /* gatt_session feeds Flux directly from the GAP event. This separate
         * ble_conn_mgr event owns a copied buffer that the application must free. */
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

void app_main(void)
{
    esp_err_t ret;
    esp_ble_conn_config_t config = {
        .device_name = "gmp-ota-client",
    };
    /* The server advertises its service UUID in the connectable advertisement. */
    esp_ble_conn_scan_params_t scan_params = {
        .own_addr_type = ESP_BLE_CONN_ADDR_PUBLIC,
        .passive = true,
        .filter_duplicates = true,
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
    ESP_ERROR_CHECK(mount_spiffs());

    esp_gmp_init();
    /* OS profile owns GRP_OS and forwards CAP READ_RSP to the OTA host waiter. */
    ESP_ERROR_CHECK(esp_gmp_os_init());
    ESP_ERROR_CHECK(esp_gmp_ota_host_init(NULL));

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID,
                                               app_ble_conn_event_handler, NULL));
    ESP_ERROR_CHECK(esp_ble_conn_init(&config));
    ESP_ERROR_CHECK(esp_ble_conn_scan_params_set(&scan_params));
    ESP_ERROR_CHECK(esp_ble_conn_register_scan_callback(scan_cb, NULL));
    if (esp_ble_conn_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE connection manager");
        esp_ble_conn_stop();
        esp_ble_conn_deinit();
        return;
    }

    ESP_LOGI(TAG, "Scanning for %s ...", GMP_BLE_DEVICE_NAME);

    while (1) {
        esp_gmp_poll();
        process_pending_teardowns();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
