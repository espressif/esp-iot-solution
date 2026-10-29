/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <sys/queue.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>
#include <protocomm_ext_nimble_central.h>
#include <protocomm_ext_transports.h>
#include <protocomm_ext_nimble.h>

static const char *TAG = "protocomm_ext_transport_nimble";

#define CONNECT_TIMEOUT_MS      30000
#define WRITE_READ_TIMEOUT_MS   15000
#define LL_PACKET_TIME          2120
#define LL_PACKET_LENGTH        251
#define PROTOCOMM_BLE_MAX_RSP   2048
#define BLE_MTU_SIZE            512

/* Provided by NimBLE store config; not declared in public headers. */
void ble_store_config_init(void);

static bool g_nimble_initialized = false;

typedef struct {
    char *name;
    uint16_t uuid;
} nimble_name_uuid_t;

typedef struct {
    uint16_t conn_handle;
    bool is_connected;
    SemaphoreHandle_t sync_sem;
    uint8_t *read_data;
    ssize_t read_data_len;
    esp_err_t last_error;
    nimble_name_uuid_t *nu_lookup;
    size_t nu_lookup_count;
} protocomm_nimble_handle_t;

struct scanned_device {
    ble_addr_t addr;                    /*!< Device address */
    uint8_t name[32];                   /*!< Device name */
    uint8_t name_len;                   /*!< Length of device name */
    uint8_t mfg_data[32];               /*!< Manufacturing data */
    uint8_t mfg_data_len;               /*!< Length of manufacturing data */
    SLIST_ENTRY(scanned_device) next;   /*!< Linked list entry */
};

/**
 * Head of the scanned devices list
 */
static SLIST_HEAD(scanned_device_list, scanned_device) scanned_devices = SLIST_HEAD_INITIALIZER(scanned_devices);

/**
 * Reset callback
 */
static void nimble_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

/**
 * Sync callback
 */
static void nimble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure address; rc=%d", rc);
    }
}

/**
 * Host task
 */
static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * Match BLE addresses by public/random value only (ignore type).
 * ADV and SCAN_RSP can report different addr types for the same peer.
 */
static bool nimble_addr_val_eq(const ble_addr_t *a, const ble_addr_t *b)
{
    return a && b && memcmp(a->val, b->val, sizeof(a->val)) == 0;
}

/**
 * Add a new scanned device to the list
 */
static void nimble_add_scanned_device(const ble_addr_t *addr, const uint8_t *name, uint8_t name_len, const uint8_t *mfg_data, uint8_t mfg_data_len, bool need_create_new_device)
{
    struct scanned_device *device = NULL;
    /* Check if device already exists in the list */
    SLIST_FOREACH(device, &scanned_devices, next) {
        if (nimble_addr_val_eq(&device->addr, addr)) {
            /* Prefer the addr type from the report that carries the Local Name. */
            if (name && name_len > 0) {
                device->addr = *addr;
                device->name_len = (name_len > sizeof(device->name)) ? sizeof(device->name) : name_len;
                memset(device->name, 0, sizeof(device->name));
                memcpy(device->name, name, device->name_len);
                ESP_LOGD(TAG, "Updated scanned device: %02x:%02x:%02x:%02x:%02x:%02x, name: %.*s",
                         device->addr.val[5], device->addr.val[4], device->addr.val[3],
                         device->addr.val[2], device->addr.val[1], device->addr.val[0],
                         device->name_len, device->name);
            }
            if (mfg_data && mfg_data_len > 0) {
                device->mfg_data_len = (mfg_data_len > sizeof(device->mfg_data)) ? sizeof(device->mfg_data) : mfg_data_len;
                memset(device->mfg_data, 0, sizeof(device->mfg_data));
                memcpy(device->mfg_data, mfg_data, device->mfg_data_len);
            }
            return;
        }
    }

    if (!need_create_new_device) {
        return;
    }
    /* Device not found, create new entry */
    device = calloc(1, sizeof(struct scanned_device));
    if (device == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scanned device");
        return;
    }

    /* Initialize device structure */
    memcpy(&device->addr, addr, sizeof(ble_addr_t));

    if (name && name_len > 0) {
        device->name_len = (name_len > sizeof(device->name)) ? sizeof(device->name) : name_len;
        memcpy(device->name, name, device->name_len);
    } else {
        device->name_len = 0;
    }

    if (mfg_data && mfg_data_len > 0) {
        device->mfg_data_len = (mfg_data_len > sizeof(device->mfg_data)) ? sizeof(device->mfg_data) : mfg_data_len;
        memcpy(device->mfg_data, mfg_data, device->mfg_data_len);
    } else {
        device->mfg_data_len = 0;
    }

    /* Add to the list */
    SLIST_INSERT_HEAD(&scanned_devices, device, next);

    /* Quiet scan path: the example prints prefix-filtered peers itself. */
    if (device->name_len > 0) {
        ESP_LOGD(TAG, "Added scanned device: %02x:%02x:%02x:%02x:%02x:%02x, name: %.*s",
                 device->addr.val[5], device->addr.val[4], device->addr.val[3],
                 device->addr.val[2], device->addr.val[1], device->addr.val[0],
                 device->name_len, (const char *)device->name);
    }
}

/**
 * Clear the scanned devices list
 */
static void nimble_clear_scanned_devices(void)
{
    struct scanned_device *device = NULL;
    while (!SLIST_EMPTY(&scanned_devices)) {
        device = SLIST_FIRST(&scanned_devices);
        SLIST_REMOVE_HEAD(&scanned_devices, next);
        free(device);
    }
}

/* Discovery complete callback */
static void on_disc_complete(const struct peer *peer, int status, void *arg)
{
    protocomm_nimble_handle_t *handle = (protocomm_nimble_handle_t *)arg;

    if (status != 0) {
        ESP_LOGE(TAG, "Service discovery failed; status=%d conn_handle=%d", status, peer->conn_handle);
        handle->last_error = ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "Service discovery complete; status=%d conn_handle=%d", status, peer->conn_handle);
        handle->last_error = ESP_OK;
    }

    if (handle && handle->sync_sem) {
        xSemaphoreGive(handle->sync_sem);
    }
}

/**
 * Parse the advertising data
 */
static void nimble_parse_adv_data(void *disc)
{
    int rc = 0;
    struct ble_hs_adv_fields fields = {0};
    int event_type = 0;
#if CONFIG_BT_NIMBLE_EXT_ADV
    struct ble_gap_ext_disc_desc *disc_desc = (struct ble_gap_ext_disc_desc *)disc;
    event_type = disc_desc->legacy_event_type;
#else
    struct ble_gap_disc_desc *disc_desc = (struct ble_gap_disc_desc *)disc;
    event_type = disc_desc->event_type;
#endif

    if (event_type == BLE_HCI_ADV_RPT_EVTYPE_ADV_IND || event_type == BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        rc = ble_hs_adv_parse_fields(&fields, disc_desc->data, disc_desc->length_data);
        if (rc != 0) {
            return;
        }
        /*
         * Always track by address. ESP provisioning (protocomm_nimble) typically puts the
         * complete Local Name only in the scan response, while ADV carries the 128-bit service UUID.
         */
        nimble_add_scanned_device(&disc_desc->addr, fields.name, fields.name_len, fields.mfg_data, fields.mfg_data_len, true);
    } else if (event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
        rc = ble_hs_adv_parse_fields(&fields, disc_desc->data, disc_desc->length_data);
        if (rc != 0) {
            return;
        }
        /* Create missing peers from SCAN_RSP when ADV had no name (ESP wifi_prov / network_provisioning). */
        nimble_add_scanned_device(&disc_desc->addr, fields.name, fields.name_len, fields.mfg_data, fields.mfg_data_len, true);
    }

}

/* GAP event callback */
static int nimble_gap_event(struct ble_gap_event *event, void *arg)
{
    protocomm_nimble_handle_t *handle = (protocomm_nimble_handle_t *)arg;
    struct ble_gap_conn_desc desc = {0};
    int rc = 0;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* Connection established */
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "Connection established; conn_handle=%d", event->connect.conn_handle);
            /* Set packet length in controller for better throughput */
            rc = ble_hs_hci_util_set_data_len(event->connect.conn_handle, LL_PACKET_LENGTH, LL_PACKET_TIME);
            if (rc != 0) {
                ESP_LOGE(TAG, "Set packet length failed; rc = %d", rc);
            }
            /* Set preferred MTU for better throughput */
            rc = ble_att_set_preferred_mtu(BLE_MTU_SIZE);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to set preferred MTU; rc = %d", rc);
            }
            /* Negotiate MTU for better throughput */
            rc = ble_gattc_exchange_mtu(event->connect.conn_handle, NULL, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to negotiate MTU; rc = %d", rc);
            }
            /* Find the connection descriptor */
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to find connection descriptor; rc = %d", rc);
                if (handle) {
                    handle->last_error = ESP_FAIL;
                }
            } else if (handle) {
                handle->conn_handle = event->connect.conn_handle;
                handle->is_connected = true;
                handle->last_error = ESP_OK;

                /* Add peer */
                rc = peer_add(event->connect.conn_handle);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Failed to add peer; rc=%d conn_handle=%d", rc, event->connect.conn_handle);
                    handle->last_error = ESP_FAIL;
                }
            }
        } else {
            ESP_LOGE(TAG, "Connection failed; status=%d conn_handle=%d", event->connect.status, event->connect.conn_handle);
            if (handle) {
                handle->last_error = ESP_FAIL;
            }
        }

        if (handle && handle->sync_sem) {
            xSemaphoreGive(handle->sync_sem);
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        /* Connection disconnected */
        ESP_LOGI(TAG, "Disconnect; reason=%d conn_handle=%d",
                 event->disconnect.reason,
                 event->disconnect.conn.conn_handle);
        if (handle) {
            peer_delete(event->disconnect.conn.conn_handle);
            handle->is_connected = false;
            handle->conn_handle = 0;
            handle->last_error = ESP_FAIL;
            if (handle->sync_sem) {
                xSemaphoreGive(handle->sync_sem);
            }
        }
        break;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        /* Discovery complete */
        ESP_LOGI(TAG, "discovery complete; reason=%d", event->disc_complete.reason);
        if (handle && handle->sync_sem) {
            xSemaphoreGive(handle->sync_sem);
        }
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption change event */
        ESP_LOGI(TAG, "Encryption change event; status=%d conn_handle=%d", event->enc_change.status, event->enc_change.conn_handle);
        break;

    case BLE_GAP_EVENT_MTU:
        /* MTU update event */
        ESP_LOGI(TAG, "MTU update event; conn_handle=%d mtu=%d", event->mtu.conn_handle, event->mtu.value);
        break;
#if CONFIG_BT_NIMBLE_EXT_ADV
    case BLE_GAP_EVENT_EXT_DISC:
        nimble_parse_adv_data(&event->ext_disc);
        break;
#else
    case BLE_GAP_EVENT_DISC:
        nimble_parse_adv_data(&event->disc);
        break;
#endif
    default:
        break;
    }
    return 0;
}

/* GATT write callback (used so send_data can wait before issuing a follow-up read). */
static int gattc_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                          struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    (void)attr;
    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)arg;

    /* write / write_long complete with 0 or BLE_HS_EDONE. */
    if (error->status == 0 || error->status == BLE_HS_EDONE) {
        nimble_handle->last_error = ESP_OK;
        ESP_LOGD(TAG, "Write complete; status=%d", error->status);
    } else {
        ESP_LOGE(TAG, "Write failed; status=%d", error->status);
        nimble_handle->last_error = ESP_FAIL;
    }

    if (nimble_handle && nimble_handle->sync_sem) {
        xSemaphoreGive(nimble_handle->sync_sem);
    }
    return 0;
}

/** Append one GATT read fragment into the handle buffer (for read_long). */
static esp_err_t gattc_append_read(protocomm_nimble_handle_t *h, struct ble_gatt_attr *attr)
{
    if (!attr || !attr->om) {
        return ESP_OK;
    }
    uint16_t chunk = OS_MBUF_PKTLEN(attr->om);
    if (chunk == 0) {
        return ESP_OK;
    }
    if ((size_t)h->read_data_len + chunk > PROTOCOMM_BLE_MAX_RSP) {
        ESP_LOGE(TAG, "Read response exceeds %d bytes", PROTOCOMM_BLE_MAX_RSP);
        return ESP_ERR_NO_MEM;
    }
    uint8_t *nb = realloc(h->read_data, (size_t)h->read_data_len + chunk);
    if (!nb) {
        return ESP_ERR_NO_MEM;
    }
    h->read_data = nb;
    os_mbuf_copydata(attr->om, 0, chunk, h->read_data + h->read_data_len);
    h->read_data_len += chunk;
    return ESP_OK;
}

/* GATT read / read_long callback */
static int gattc_read_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                         struct ble_gatt_attr *attr, void *arg)
{
    (void)conn_handle;
    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)arg;

    if (error->status == BLE_HS_EDONE) {
        nimble_handle->last_error = ESP_OK;
        ESP_LOGI(TAG, "Read complete; len=%d", (int)nimble_handle->read_data_len);
        if (nimble_handle->sync_sem) {
            xSemaphoreGive(nimble_handle->sync_sem);
        }
        return 0;
    }

    if (error->status != 0) {
        ESP_LOGE(TAG, "Read failed; status=%d", error->status);
        nimble_handle->last_error = ESP_FAIL;
        if (nimble_handle->sync_sem) {
            xSemaphoreGive(nimble_handle->sync_sem);
        }
        return 0;
    }

    /* Intermediate (read_long) or single-shot (read) payload. */
    if (gattc_append_read(nimble_handle, attr) != ESP_OK) {
        nimble_handle->last_error = ESP_ERR_NO_MEM;
        if (nimble_handle->sync_sem) {
            xSemaphoreGive(nimble_handle->sync_sem);
        }
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    /*
     * Plain ble_gattc_read finishes with status==0 and no EDONE follow-up.
     * read_long uses intermediate status==0 then a final BLE_HS_EDONE.
     * Heuristic: if attr offset stays 0 and we only do read_long, EDONE always comes.
     * We always use read_long below, so do not give the semaphore here.
     */
    return 0;
}

/**
 * Resolve endpoint name via application-registered nu_lookup, or accept a
 * 16-bit hex string ("ff53" / "0xff53"). Names must be registered with
 * protocomm_ext_set_config_endpoint() by higher-level components.
 */
static esp_err_t nimble_ep_name_to_uuid16(protocomm_nimble_handle_t *h,
                                          const char *ep_name, uint16_t *out_uuid)
{
    if (!h || !ep_name || !out_uuid) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < h->nu_lookup_count; i++) {
        if (h->nu_lookup[i].name && strcmp(ep_name, h->nu_lookup[i].name) == 0) {
            *out_uuid = h->nu_lookup[i].uuid;
            return ESP_OK;
        }
    }

    const char *hex = ep_name;
    if (hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
        hex += 2;
    }
    if (*hex == '\0') {
        ESP_LOGE(TAG, "Empty hex UUID");
        return ESP_ERR_INVALID_ARG;
    }
    for (const char *p = hex; *p; p++) {
        if (!isxdigit((unsigned char) * p)) {
            ESP_LOGE(TAG, "Unknown BLE endpoint name (register via "
                     "protocomm_ext_set_config_endpoint, or pass 16-bit hex UUID): %s",
                     ep_name);
            return ESP_ERR_INVALID_ARG;
        }
    }

    uint16_t uuid_val = 0;
    if (sscanf(hex, "%hx", &uuid_val) != 1) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_uuid = uuid_val;
    return ESP_OK;
}

static void nimble_free_nu_lookup(protocomm_nimble_handle_t *h)
{
    if (!h || !h->nu_lookup) {
        return;
    }
    for (size_t i = 0; i < h->nu_lookup_count; i++) {
        free(h->nu_lookup[i].name);
    }
    free(h->nu_lookup);
    h->nu_lookup = NULL;
    h->nu_lookup_count = 0;
}

static esp_err_t nimble_set_config_endpoint(protocomm_ext_transport_handle_t handle,
                                            const char *endpoint_name,
                                            uint16_t uuid)
{
    if (!handle || !endpoint_name) {
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_nimble_handle_t *h = (protocomm_nimble_handle_t *)handle;

    for (size_t i = 0; i < h->nu_lookup_count; i++) {
        if (h->nu_lookup[i].name && strcmp(h->nu_lookup[i].name, endpoint_name) == 0) {
            h->nu_lookup[i].uuid = uuid;
            return ESP_OK;
        }
    }

    char *name_copy = strdup(endpoint_name);
    if (!name_copy) {
        return ESP_ERR_NO_MEM;
    }

    nimble_name_uuid_t *table = realloc(h->nu_lookup,
                                        (h->nu_lookup_count + 1) * sizeof(*table));
    if (!table) {
        free(name_copy);
        return ESP_ERR_NO_MEM;
    }

    table[h->nu_lookup_count].name = name_copy;
    table[h->nu_lookup_count].uuid = uuid;
    h->nu_lookup = table;
    h->nu_lookup_count += 1;
    return ESP_OK;
}

/** Match discovered char UUID: peer uses 128-bit base with short UUID in bytes 12–13. */
static bool nimble_uuid_matches_short(const ble_uuid_t *uuid, uint16_t short_uuid)
{
    if (!uuid) {
        return false;
    }
    if (uuid->type == BLE_UUID_TYPE_16) {
        return BLE_UUID16(uuid)->value == short_uuid;
    }
    if (uuid->type == BLE_UUID_TYPE_128) {
        uint16_t extracted = 0;
        memcpy(&extracted, &BLE_UUID128(uuid)->value[12], sizeof(extracted));
        return extracted == short_uuid;
    }
    return false;
}

/* Tear down an established GAP connection and wait for DISCONNECT cleanup. */
static void nimble_drop_connection(protocomm_nimble_handle_t *h)
{
    if (!h || !h->is_connected) {
        return;
    }
    int rc = ble_gap_terminate(h->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    if (rc != 0) {
        ESP_LOGW(TAG, "ble_gap_terminate rc=%d; dropping local peer state", rc);
        peer_delete(h->conn_handle);
        h->is_connected = false;
        h->conn_handle = 0;
        return;
    }
    if (h->sync_sem) {
        (void)xSemaphoreTake(h->sync_sem, pdMS_TO_TICKS(3000));
    }
    if (h->is_connected) {
        peer_delete(h->conn_handle);
        h->is_connected = false;
        h->conn_handle = 0;
    }
}

static esp_err_t nimble_init(protocomm_ext_transport_handle_t *handle, const void *config)
{
    if (g_nimble_initialized) {
        ESP_LOGI(TAG, "NimBLE already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate handle */
    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)calloc(1, sizeof(protocomm_nimble_handle_t));
    if (nimble_handle == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for handle");
        return ESP_ERR_NO_MEM;
    }

    /* Create sync semaphore */
    nimble_handle->sync_sem = xSemaphoreCreateBinary();
    if (nimble_handle->sync_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        free(nimble_handle);
        return ESP_ERR_NO_MEM;
    }

    /* Initialize NimBLE port */
    int rc = nimble_port_init();
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble; rc=%d", rc);
        vSemaphoreDelete(nimble_handle->sync_sem);
        free(nimble_handle);
        return ESP_FAIL;
    }

    /* Configure host callback */
    ble_hs_cfg.reset_cb = nimble_on_reset;
    ble_hs_cfg.sync_cb = nimble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
#ifdef CONFIG_BT_NIMBLE_GAP_SERVICE
    /* Set device name */
    rc = ble_svc_gap_device_name_set("protocomm_ext");
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name; rc=%d", rc);
    }
#endif
    /* Initialize storage configuration */
    ble_store_config_init();

    /* Initialize peer management */
    rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to init peer; rc=%d", rc);
        nimble_port_deinit();
        vSemaphoreDelete(nimble_handle->sync_sem);
        free(nimble_handle);
        return ESP_FAIL;
    }

    /* Start NimBLE host task */
    esp_err_t host_err = esp_nimble_enable(nimble_host_task);
    if (host_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start NimBLE host task: %s", esp_err_to_name(host_err));
        peer_deinit();
        nimble_port_deinit();
        vSemaphoreDelete(nimble_handle->sync_sem);
        free(nimble_handle);
        return host_err;
    }

    *handle = (protocomm_ext_transport_handle_t)nimble_handle;
    g_nimble_initialized = true;
    ESP_LOGI(TAG, "NimBLE initialized successfully");

    return ESP_OK;
}

static esp_err_t nimble_deinit(protocomm_ext_transport_handle_t handle)
{
    if (handle == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid handle or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)handle;

    /* Wait for DISCONNECT so peer_delete runs before peer_deinit(). */
    nimble_drop_connection(nimble_handle);

    /* Stop and clean up NimBLE */
    int rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    } else {
        ESP_LOGE(TAG, "Failed to stop nimble port; rc=%d", rc);
    }

    /* Clean up peer management */
    peer_deinit();

    /* Drop any devices retained from a previous scan. */
    nimble_clear_scanned_devices();

    /* Clean up resources */
    if (nimble_handle->sync_sem) {
        vSemaphoreDelete(nimble_handle->sync_sem);
    }

    if (nimble_handle->read_data) {
        free(nimble_handle->read_data);
    }

    nimble_free_nu_lookup(nimble_handle);

    free(nimble_handle);
    g_nimble_initialized = false;

    ESP_LOGI(TAG, "NimBLE cleaned up");
    return ESP_OK;
}

static esp_err_t nimble_connect(protocomm_ext_transport_handle_t handle, const void *config)
{
    if (handle == NULL || config == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid arguments or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)handle;
    const ble_addr_t *peer_addr = (const ble_addr_t *)config;

    /* Get local address type */
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to determine address type; rc=%d", rc);
        return ESP_FAIL;
    }

    /* Connect to device */
    ESP_LOGI(TAG, "Connecting to device: %02x:%02x:%02x:%02x:%02x:%02x",
             peer_addr->val[5], peer_addr->val[4], peer_addr->val[3],
             peer_addr->val[2], peer_addr->val[1], peer_addr->val[0]);

    rc = ble_gap_connect(own_addr_type, peer_addr, CONNECT_TIMEOUT_MS,
                         NULL, nimble_gap_event, nimble_handle);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to connect; rc=%d", rc);
        return ESP_FAIL;
    }

    /* Wait for connection to complete */
    if (xSemaphoreTake(nimble_handle->sync_sem, pdMS_TO_TICKS(CONNECT_TIMEOUT_MS + 2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Connection timeout");
        ble_gap_conn_cancel();
        return ESP_ERR_TIMEOUT;
    }

    if (!nimble_handle->is_connected || nimble_handle->last_error != ESP_OK) {
        ESP_LOGE(TAG, "Connection failed");
        return ESP_FAIL;
    }

    /* Perform service discovery */
    struct peer *peer = peer_find(nimble_handle->conn_handle);
    if (peer == NULL) {
        ESP_LOGE(TAG, "Peer not found");
        nimble_drop_connection(nimble_handle);
        return ESP_FAIL;
    }

    rc = peer_disc_all(nimble_handle->conn_handle, on_disc_complete, nimble_handle);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to discover services; rc=%d", rc);
        nimble_drop_connection(nimble_handle);
        return ESP_FAIL;
    }

    /* Wait for service discovery to complete */
    if (xSemaphoreTake(nimble_handle->sync_sem, pdMS_TO_TICKS(10000)) != pdTRUE) {
        ESP_LOGE(TAG, "Service discovery timeout");
        nimble_drop_connection(nimble_handle);
        return ESP_ERR_TIMEOUT;
    }

    if (nimble_handle->last_error != ESP_OK) {
        ESP_LOGE(TAG, "Service discovery failed");
        nimble_drop_connection(nimble_handle);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Connected to device successfully");
    return ESP_OK;
}

static esp_err_t nimble_disconnect(protocomm_ext_transport_handle_t handle)
{
    if (handle == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid handle or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)handle;

    if (!nimble_handle->is_connected) {
        ESP_LOGW(TAG, "Not connected");
        return ESP_OK;
    }

    nimble_drop_connection(nimble_handle);
    ESP_LOGI(TAG, "Disconnected successfully");
    return ESP_OK;
}

static esp_err_t nimble_send_data(protocomm_ext_transport_handle_t handle, const char *ep_name,
                                  const uint8_t *data, ssize_t data_len,
                                  uint8_t **out_data, ssize_t *out_data_len)
{
    /* Empty write (data_len == 0) is valid: NimBLE peer caches response on write. */
    if (!handle || !ep_name || data_len < 0 || (data_len > 0 && !data) || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid arguments or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    if (!out_data || !out_data_len) {
        ESP_LOGE(TAG, "Invalid output arguments");
        return ESP_ERR_INVALID_ARG;
    }

    *out_data = NULL;
    *out_data_len = 0;

    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)handle;

    if (!nimble_handle->is_connected) {
        ESP_LOGE(TAG, "Not connected");
        return ESP_ERR_INVALID_STATE;
    }

    struct peer *peer = peer_find(nimble_handle->conn_handle);
    if (peer == NULL) {
        ESP_LOGE(TAG, "Peer not found");
        return ESP_FAIL;
    }

    uint16_t uuid_val = 0;
    esp_err_t err = nimble_ep_name_to_uuid16(nimble_handle, ep_name, &uuid_val);
    if (err != ESP_OK) {
        return err;
    }

    const struct peer_chr *chr = NULL;
    const struct peer_svc *svc;
    SLIST_FOREACH(svc, &peer->svcs, next) {
        const struct peer_chr *temp_chr;
        SLIST_FOREACH(temp_chr, &svc->chrs, next) {
            if (nimble_uuid_matches_short(&temp_chr->chr.uuid.u, uuid_val)) {
                chr = temp_chr;
                break;
            }
        }
        if (chr != NULL) {
            break;
        }
    }

    if (chr == NULL) {
        ESP_LOGE(TAG, "Characteristic not found: %s (0x%04x)", ep_name, uuid_val);
        return ESP_ERR_NOT_FOUND;
    }

    /* Drain any leftover sync token before write/read handshake. */
    while (xSemaphoreTake(nimble_handle->sync_sem, 0) == pdTRUE) {
    }

    /*
     * Payloads larger than (ATT_MTU - 3) must use Write Long (Prepare Write).
     * Sec2 command0 is typically ~400 bytes while MTU is often 256.
     */
    uint16_t mtu = ble_att_mtu(nimble_handle->conn_handle);
    uint16_t max_single = (mtu > 3) ? (uint16_t)(mtu - 3) : 20;
    int rc;

    if (data_len == 0) {
        rc = ble_gattc_write_flat(nimble_handle->conn_handle, chr->chr.val_handle,
                                  "", 0, gattc_write_cb, nimble_handle);
    } else if ((uint16_t)data_len <= max_single) {
        rc = ble_gattc_write_flat(nimble_handle->conn_handle, chr->chr.val_handle,
                                  data, (uint16_t)data_len, gattc_write_cb, nimble_handle);
    } else {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data, (uint16_t)data_len);
        if (!om) {
            ESP_LOGE(TAG, "Failed to alloc mbuf for long write (%d bytes)", (int)data_len);
            return ESP_ERR_NO_MEM;
        }
        rc = ble_gattc_write_long(nimble_handle->conn_handle, chr->chr.val_handle,
                                  0, om, gattc_write_cb, nimble_handle);
        /* om is consumed by the stack whether call succeeds or fails. */
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to write characteristic; rc=%d len=%d mtu=%u",
                 rc, (int)data_len, (unsigned)mtu);
        return ESP_FAIL;
    }

    if (xSemaphoreTake(nimble_handle->sync_sem, pdMS_TO_TICKS(WRITE_READ_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Write timeout");
        return ESP_ERR_TIMEOUT;
    }
    if (nimble_handle->last_error != ESP_OK) {
        ESP_LOGE(TAG, "Write failed");
        return nimble_handle->last_error;
    }
    ESP_LOGI(TAG, "Write complete; ep=%s uuid=0x%04x len=%d mtu=%u",
             ep_name, uuid_val, (int)data_len, (unsigned)mtu);

    if (nimble_handle->read_data) {
        free(nimble_handle->read_data);
        nimble_handle->read_data = NULL;
        nimble_handle->read_data_len = 0;
    }

    /* Read Long so Sec2 / version JSON larger than MTU still complete. */
    rc = ble_gattc_read_long(nimble_handle->conn_handle, chr->chr.val_handle, 0,
                             gattc_read_cb, nimble_handle);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to read characteristic; rc=%d", rc);
        return ESP_FAIL;
    }

    if (xSemaphoreTake(nimble_handle->sync_sem, pdMS_TO_TICKS(WRITE_READ_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Read timeout");
        free(nimble_handle->read_data);
        nimble_handle->read_data = NULL;
        nimble_handle->read_data_len = 0;
        return ESP_ERR_TIMEOUT;
    }

    if (nimble_handle->last_error != ESP_OK) {
        ESP_LOGE(TAG, "Read failed");
        esp_err_t err = nimble_handle->last_error;
        free(nimble_handle->read_data);
        nimble_handle->read_data = NULL;
        nimble_handle->read_data_len = 0;
        return err;
    }

    *out_data = nimble_handle->read_data;
    *out_data_len = nimble_handle->read_data_len;
    nimble_handle->read_data = NULL;
    nimble_handle->read_data_len = 0;

    return ESP_OK;
}

int esp_protocomm_ext_nimble_get_scanned_device_count(protocomm_ext_transport_handle_t handle)
{
    if (handle == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid arguments or NimBLE not initialized");
        return 0;
    }

    struct scanned_device *device = NULL;
    int count = 0;
    SLIST_FOREACH(device, &scanned_devices, next) {
        count++;
    }
    return count;
}

esp_err_t esp_protocomm_ext_nimble_get_scanned_device_info(protocomm_ext_transport_handle_t handle, int index, protocomm_ext_nimble_scanned_device_info_t *info)
{
    if (handle == NULL || info == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid arguments or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    memset(info, 0, sizeof(*info));

    struct scanned_device *device = NULL;
    int current_index = 0;

    SLIST_FOREACH(device, &scanned_devices, next) {
        if (current_index == index) {
            info->addr = device->addr;
            if (device->name_len > 0) {
                info->name = calloc(1, device->name_len + 1);
                if (info->name == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for name");
                    return ESP_ERR_NO_MEM;
                }
                memcpy(info->name, device->name, device->name_len);
                info->name_len = device->name_len;
            }
            if (device->mfg_data_len > 0) {
                info->mfg_data = calloc(1, device->mfg_data_len);
                if (info->mfg_data == NULL) {
                    ESP_LOGE(TAG, "Failed to allocate memory for mfg_data");
                    free(info->name);
                    info->name = NULL;
                    info->name_len = 0;
                    return ESP_ERR_NO_MEM;
                }
                memcpy(info->mfg_data, device->mfg_data, device->mfg_data_len);
                info->mfg_data_len = device->mfg_data_len;
            }
            return ESP_OK;
        }
        current_index++;
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t esp_protocomm_ext_nimble_start_scan(protocomm_ext_transport_handle_t handle, const protocomm_ext_nimble_scan_config_t *config)
{
    if (handle == NULL || config == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid arguments or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    /* ble_gap_disc() takes int32_t duration_ms; reject values that would wrap. */
    if (config->scan_timeout_ms > (uint32_t)INT32_MAX) {
        ESP_LOGE(TAG, "scan_timeout_ms %u exceeds INT32_MAX", (unsigned)config->scan_timeout_ms);
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)handle;

    /* Clear the scanned devices */
    nimble_clear_scanned_devices();

    uint8_t own_addr_type = 0;
    struct ble_gap_disc_params disc_params = {0};

    /* Figure out address to use while advertising (no privacy for now) */
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return ESP_FAIL;
    }

    /*
     * Do not filter duplicates at the controller: ESP provisioning peers put the
     * Local Name in SCAN_RSP; duplicate filtering can drop that follow-up report.
     */
    disc_params.filter_duplicates = 0;

    /* Active scan so we receive scan responses (device name). */
    disc_params.passive = 0;
    disc_params.disable_observer_mode = 0;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0x0;
    disc_params.window = 0x0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, config->scan_timeout_ms, &disc_params, nimble_gap_event, nimble_handle);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
        return ESP_FAIL;
    }

    /* Wait for scan finished */
    if (xSemaphoreTake(nimble_handle->sync_sem, pdMS_TO_TICKS(config->scan_timeout_ms + 1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Scan timeout");
        return ESP_ERR_TIMEOUT;
    }

    int named = 0;
    int total = 0;
    struct scanned_device *d = NULL;
    SLIST_FOREACH(d, &scanned_devices, next) {
        total++;
        if (d->name_len > 0) {
            named++;
        }
    }
    /* Caller (example) prints the filtered selection list. */
    ESP_LOGI(TAG, "Scan finished: %d named / %d tracked peers", named, total);
    return ESP_OK;
}

esp_err_t esp_protocomm_ext_nimble_stop_scan(protocomm_ext_transport_handle_t handle)
{
    if (handle == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid arguments or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    int rc = ble_gap_disc_cancel();
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "ble_gap_disc_cancel returned rc=%d", rc);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_protocomm_ext_nimble_find_chr_uuid(protocomm_ext_transport_handle_t handle, const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid)
{
    if (handle == NULL || !g_nimble_initialized) {
        ESP_LOGE(TAG, "Invalid arguments or NimBLE not initialized");
        return ESP_ERR_INVALID_ARG;
    }

    if (svc_uuid == NULL || chr_uuid == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    protocomm_nimble_handle_t *nimble_handle = (protocomm_nimble_handle_t *)handle;

    struct peer *peer = peer_find(nimble_handle->conn_handle);
    if (peer == NULL) {
        ESP_LOGE(TAG, "Peer not found");
        return ESP_ERR_NOT_FOUND;
    }

    const struct peer_chr *chr = peer_chr_find_uuid(peer, svc_uuid, chr_uuid);
    if (chr == NULL) {
        ESP_LOGE(TAG, "Characteristic not found");
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

const protocomm_ext_transport_t protocomm_ext_transport_nimble = {
    .init = nimble_init,
    .deinit = nimble_deinit,
    .connect = nimble_connect,
    .disconnect = nimble_disconnect,
    .send_data = nimble_send_data,
    .set_config_endpoint = nimble_set_config_endpoint,
};
