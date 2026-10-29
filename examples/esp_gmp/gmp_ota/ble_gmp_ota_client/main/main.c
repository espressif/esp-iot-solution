/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * NimBLE central: discover ESP-GMP-OTA peripheral and push firmware from SPIFFS.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#include "ble_manager.h"
#include "gatt_discovery.h"
#include "flux_gatt_session.h"
#include "esp_gmp.h"
#include "esp_gmp_flux.h"
#include "esp_gmp_os.h"
#include "esp_gmp_ota_host.h"
#include "gmp_ble_uuids.h"
#include "gmp_session_teardown.h"
#include "sdkconfig.h"

static const char *TAG = "gmp_ota_cli";

#define MAX_CONNECTIONS CONFIG_BLE_MANAGER_MAX_CONNECTIONS
#define GMP_OTA_FIRMWARE_PATH "/spiffs/ota_image.bin"

static ble_manager_t *g_ble_manager;
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

typedef struct {
    gatt_session_t *session;
    uint32_t generation;
} tx_notify_write_ctx_t;

static const ble_uuid128_t gmp_svc_uuid = GMP_BLE_UUID_SVC;
static const ble_uuid128_t gmp_chr_rx_uuid = GMP_BLE_UUID_CHR_RX;
static const ble_uuid128_t gmp_chr_tx_uuid = GMP_BLE_UUID_CHR_TX;
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

static int tx_notify_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg)
{
    tx_notify_write_ctx_t *ctx = (tx_notify_write_ctx_t *)arg;
    bool should_start = false;
    bool valid_session = false;
    int status = error ? error->status : BLE_HS_EUNKNOWN;

    (void)attr;
    if (!ctx) {
        return 0;
    }

    portENTER_CRITICAL(&g_state_lock);
    if (g_session == ctx->session &&
            g_conn_generation == ctx->generation &&
            g_session &&
            g_session->conn_handle == conn_handle) {
        valid_session = true;
        if (status == 0) {
            g_notify_enabled = true;
            should_start = g_conn_params_ready;
        }
    }
    portEXIT_CRITICAL(&g_state_lock);

    if (!valid_session) {
        free(ctx);
        return 0;
    }

    if (status != 0) {
        ESP_LOGE(TAG, "CCCD write callback failed conn=%d status=%d", conn_handle, status);
        ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        free(ctx);
        return 0;
    }

    if (should_start) {
        maybe_start_ota(ctx->session);
    }
    free(ctx);
    return 0;
}

static esp_err_t enable_tx_notify(gatt_discovery_t *discovery, uint16_t tx_val_handle, gatt_session_t *session, uint32_t generation)
{
    (void)tx_val_handle;
    gatt_descriptor_t *dsc = gatt_discovery_find_descriptor_by_uuid(
                                 discovery, &gmp_svc_uuid.u, &gmp_chr_tx_uuid.u,
                                 BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16));
    if (!dsc) {
        ESP_LOGE(TAG, "CCCD not found for TX");
        return ESP_ERR_NOT_FOUND;
    }

    tx_notify_write_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        ESP_LOGE(TAG, "no memory for CCCD write callback context");
        return ESP_ERR_NO_MEM;
    }
    ctx->session = session;
    ctx->generation = generation;

    uint8_t value[2] = {1, 0};
    int rc = ble_gattc_write_flat(discovery->conn_handle, dsc->dsc.handle, value, sizeof(value), tx_notify_write_cb, ctx);
    if (rc != 0) {
        free(ctx);
        ESP_LOGE(TAG, "CCCD write failed rc=%d", rc);
        return ESP_FAIL;
    }
    return ESP_OK;
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

static void gatt_discovery_complete_cb(gatt_discovery_t *discovery, int status, void *arg)
{
    (void)arg;

    if (status != ESP_OK) {
        ESP_LOGE(TAG, "discovery failed conn=%d status=%d", discovery->conn_handle, status);
        ble_gap_terminate(discovery->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    gatt_characteristic_t *chr_rx = gatt_discovery_find_characteristic_by_uuid(
                                        discovery, &gmp_svc_uuid.u, &gmp_chr_rx_uuid.u);
    gatt_characteristic_t *chr_tx = gatt_discovery_find_characteristic_by_uuid(
                                        discovery, &gmp_svc_uuid.u, &gmp_chr_tx_uuid.u);

    if (!chr_rx || !chr_tx) {
        ESP_LOGE(TAG, "GMP RX/TX characteristics not found");
        ble_gap_terminate(discovery->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    uint16_t mtu = BLE_ATT_MTU_DFLT;
    ble_manager_get_mtu_by_conn(g_ble_manager, discovery->conn_handle, &mtu);

    gatt_session_t *session = gatt_session_create(discovery->conn_handle, NULL,
                                                  chr_rx->chr.val_handle, chr_tx->chr.val_handle,
                                                  mtu, GATT_SESSION_ROLE_MASTER);
    if (!session) {
        ESP_LOGE(TAG, "gatt_session_create failed");
        ble_gap_terminate(discovery->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    if (setup_gmp_callbacks(session) != ESP_OK) {
        ESP_LOGE(TAG, "setup GMP callbacks failed");
        gatt_session_destroy(session);
        ble_gap_terminate(discovery->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
        return;
    }

    if (setup_gmp_session(session) != ESP_OK) {
        ESP_LOGE(TAG, "GMP Flux registration failed");
        gatt_session_destroy(session);
        ble_gap_terminate(discovery->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
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
    ESP_LOGI(TAG, "GMP client session conn=%d mtu=%u", discovery->conn_handle, (unsigned)mtu);

    if (enable_tx_notify(discovery, chr_tx->chr.val_handle, session, generation) != ESP_OK) {
        portENTER_CRITICAL(&g_state_lock);
        if (g_session == session && g_conn_generation == generation) {
            g_session = NULL;
            g_conn_params_ready = false;
            g_notify_enabled = false;
            g_conn_generation++;
        }
        portEXIT_CRITICAL(&g_state_lock);
        teardown_gmp_session(session);
        gatt_session_destroy(session);
        ble_gap_terminate(discovery->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        gatt_discovery_destroy(discovery);
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

    gatt_discovery_destroy(discovery);
}

static void ble_manager_conn_update_cb(ble_manager_t *manager, ble_connection_t *conn, void *arg)
{
    (void)manager;
    (void)arg;

    gatt_session_t *session = NULL;
    portENTER_CRITICAL(&g_state_lock);
    if (g_session && g_session->conn_handle == conn->conn_handle) {
        session = g_session;
        g_conn_params_ready = true;
    }
    portEXIT_CRITICAL(&g_state_lock);

    if (!session) {
        return;
    }

    ESP_LOGI(TAG, "Connection params ready (interval=%.2f ms)", conn->conn_interval * 1.25f);
    maybe_start_ota(session);
}

static void ble_manager_scan_cb(ble_manager_t *manager, uint8_t type, const struct ble_gap_disc_desc *result, void *arg)
{
    (void)manager;
    (void)arg;

    if (type != BLE_GAP_EVENT_DISC) {
        return;
    }

    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, result->data, result->length_data) != 0) {
        return;
    }

    bool match = false;

    if (fields.name && fields.name_len > 0) {
        if (fields.name_len == strlen(GMP_BLE_DEVICE_NAME) &&
                memcmp(fields.name, GMP_BLE_DEVICE_NAME, fields.name_len) == 0) {
            match = true;
        }
    }

    for (uint8_t i = 0; i < fields.num_uuids128 && !match; i++) {
        if (gmp_ble_uuid128_eq(&fields.uuids128[i], &gmp_svc_uuid)) {
            match = true;
        }
    }

    if (!match) {
        return;
    }

    ESP_LOGI(TAG, "Found GMP OTA device, connecting...");
    ble_manager_connect(g_ble_manager, &result->addr, 24, 24, 0, 300);
}

static void ble_manager_connect_cb(ble_manager_t *manager, uint8_t type, ble_connection_t *conn, int status, void *arg)
{
    (void)manager;
    (void)status;
    (void)arg;

    if (type != BLE_EVENT_TYPE_CONNECT_COMPLETE && type != BLE_EVENT_TYPE_RECONNECT_COMPLETE) {
        return;
    }

    ble_discovery_callbacks_t dcb = {
        .discovery_complete_cb = gatt_discovery_complete_cb,
        .service_found_cb = NULL,
        .characteristic_found_cb = NULL,
        .descriptor_found_cb = NULL,
        .arg = NULL,
    };

    gatt_discovery_t *discovery = gatt_discovery_start(conn->conn_handle, &dcb);
    if (!discovery) {
        ESP_LOGE(TAG, "gatt_discovery_start failed");
        ble_gap_terminate(conn->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    gatt_discovery_discover_service_by_uuid(discovery, &gmp_svc_uuid.u);
}

static void ble_manager_disconnect_cb(ble_manager_t *manager, ble_connection_t *conn, int status, void *arg)
{
    (void)manager;
    (void)status;
    (void)arg;

    gatt_session_t *session = NULL;
    portENTER_CRITICAL(&g_state_lock);
    if (g_session && g_session->conn_handle == conn->conn_handle) {
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
                ESP_LOGE(TAG, "failed to schedule teardown for conn=%d", conn->conn_handle);
            }
        }
    }

    ble_manager_start_scan(g_ble_manager, 0);
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_store_config_init(void);

static void nimble_host_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
}

static void nimble_host_config(void)
{
    ble_hs_cfg.reset_cb = NULL;
    ble_hs_cfg.sync_cb = nimble_host_on_sync;
    ble_hs_cfg.store_status_cb = NULL;
    ble_store_config_init();
}

void app_main(void)
{
    esp_err_t ret;

    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    esp_log_level_set("ble_manager", ESP_LOG_WARN);
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

    ESP_ERROR_CHECK(nimble_port_init());
    nimble_host_config();

    ble_svc_gap_device_name_set("gmp-ota-client");

    nimble_port_freertos_init(nimble_host_task);

    ble_manager_callbacks_t mgr_cbs = {
        .ble_adv_cb = NULL,
        .ble_scan_cb = ble_manager_scan_cb,
        .ble_connect_cb = ble_manager_connect_cb,
        .ble_disconnect_cb = ble_manager_disconnect_cb,
        .ble_conn_update_cb = ble_manager_conn_update_cb,
        .arg = NULL,
    };
    g_ble_manager = ble_manager_init(&mgr_cbs);
    if (!g_ble_manager) {
        ESP_LOGE(TAG, "ble_manager_init failed");
        return;
    }

    ble_manager_start_scan(g_ble_manager, 0);
    ESP_LOGI(TAG, "Scanning for %s ...", GMP_BLE_DEVICE_NAME);

    while (1) {
        esp_gmp_poll();
        process_pending_teardowns();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
