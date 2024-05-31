/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "esp_log.h"
#include "esp_event.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_mac.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_nimble_hci.h"
#endif

#include "esp_ble_conn_mgr.h"
#include "esp_nimble.h"

static const char *TAG = "blecm_nimble";

/* Event source task related definitions */
ESP_EVENT_DEFINE_BASE(BLE_CONN_MGR_EVENTS);

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
/**
 * @brief   This structure maps handler which are used to BLE descriptor characteristics.
 */
typedef struct dsc_uuid_t {
    SLIST_ENTRY(dsc_uuid_t) next;
    struct ble_gatt_dsc dsc;
} dsc_uuid_t;
SLIST_HEAD(dsc_uuid_list, dsc_uuid_t);

/**
 * @brief   This structure maps handler which are used to BLE characteristics.
 */
typedef struct chr_uuid_t {
    SLIST_ENTRY(chr_uuid_t) next;
    struct ble_gatt_chr chr;

    struct dsc_uuid_list dscs;
} chr_uuid_t;
SLIST_HEAD(chr_uuid_list, chr_uuid_t);
#endif

/**
 * @brief   This structure maps handler required by UUID which are used to BLE services.
 */
typedef struct svc_uuid_t {
    SLIST_ENTRY(svc_uuid_t) next;
#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
    struct ble_gatt_svc gatt_svc;

    struct chr_uuid_list chrs;
#endif
    esp_ble_conn_svc_t svc;
} svc_uuid_t;
SLIST_HEAD(svc_uuid_list_t, svc_uuid_t);

/**
 * @brief   This structure maps handler required by UUID which are used to read or write.
 */
typedef struct attr_mbuf_t {
    SLIST_ENTRY(attr_mbuf_t) next;
    uint8_t *outbuf;                                    /* Data buffer as read or write for characteristics */
    uint16_t outlen;                                    /* Data length as read or write for characteristics */
    uint16_t attr_handle;                               /* Executed handle when characteristic is read or written */
    uint8_t type;                                       /* Type of the UUID */
    esp_ble_conn_uuid_t uuid;                           /* Universal UUID, to be used for any-UUID static allocation */
} attr_mbuf_t;
SLIST_HEAD(attr_mbuf_list, attr_mbuf_t);

/**
 * @brief   This structure maps handler required by event which are used to .
 */
typedef struct {
    esp_ble_conn_event_t event;
    uint16_t data_len;
    void *data;
    void *handle;
} esp_ble_conn_event_ctx_t;

/**
 * @brief   This structure maps handler required by session which are used to BLE connection management.
 */
typedef struct esp_ble_conn_session_t {
    uint8_t                    *device_name;            /* Name to be displayed to devices scanning */

    uint16_t                    gatt_mtu;               /* MTU set value */

    uint16_t                    adv_data_len;           /* Advertisement data length, it is also used by extended */
    uint16_t                    adv_rsp_data_len;       /* Scan responses data length, it is also used by extended */

    uint8_t                    *adv_data_buf;           /* Advertisement data buffer, it is also used by extended */
    uint8_t                    *adv_rsp_data_buf;       /* Scan responses data buffer, it is also used by extended */

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV)
    uint16_t                    per_adv_data_len;       /* Periodic advertisement data length */
    uint8_t                    *per_adv_data_buf;       /* Periodic advertisement data buffer */
#endif

    uint16_t                    nu_lookup_count;        /* Number of entries in the service lookup table */

    struct svc_uuid_list_t      uuid_list;              /* List required by UUID which are used to BLE services */
    struct attr_mbuf_list       mbuf_list;              /* List required by UUID which are used to BLE characteristics */

    uint16_t                    conn_handle;            /* Handle of the relevant connection */
    uint8_t                     own_addr_type;          /* Figure out address to use while advertising */

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
    uint16_t                    chrs_handle;            /* Handle of the characteristics discovery process */
    svc_uuid_t                 *svcs_handle;            /* Handle of the service discovery process */
#endif

    uint8_t                      gatt_db_count;         /* Number of entries in the gatt_db descriptor table */
    struct ble_gatt_svc_def     *gatt_db;               /* Descriptor table which consists the services and characteristics */

    SemaphoreHandle_t           semaphore;
    QueueHandle_t               queue;

    esp_ble_conn_nimble_cb_t    *connect_cb;            /* Client connect callback */
    esp_ble_conn_nimble_cb_t    *disconnect_cb;         /* Client disconnect callback */
    esp_ble_conn_nimble_cb_t    *set_mtu_cb;            /* MTU set callback */

    bool                        ble_sm_sc;              /* BLE Secure Connection flag */
    bool                        ble_bonding;            /* BLE bonding */
} __attribute__((packed)) esp_ble_conn_session_t;

static esp_ble_conn_session_t *s_conn_session = NULL;

#ifndef CONFIG_BLE_CONN_MGR_EXTENDED_ADV
static int esp_ble_conn_gap_event(struct ble_gap_event *event, void *arg);
#endif

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
static void esp_ble_conn_chr_uuid_del(chr_uuid_t *chr);

static void esp_ble_conn_disc_chrs(esp_ble_conn_session_t *conn_session);
static void esp_ble_conn_disc_dscs(esp_ble_conn_session_t *conn_session);

static esp_err_t esp_ble_conn_on_gatts_attr_value_set(uint16_t attr_handle, const ble_uuid_t *ble_uuid, uint16_t outlen, uint8_t *outbuf);

static void esp_ble_conn_disc_complete(esp_ble_conn_session_t *conn_session, int rc)
{
    conn_session->chrs_handle = 0;
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Error: Service discovery failed; rc=%d, conn_handle=%d", rc, conn_session->conn_handle);
        ble_gap_terminate(conn_session->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    } else {
        ESP_LOGI(TAG, "Service discovery complete; rc=%d, conn_handle=%d", rc, conn_session->conn_handle);
        esp_event_post(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_DISC_COMPLETE, NULL, 0, portMAX_DELAY);
    }
}

static int esp_ble_conn_svc_is_empty(const svc_uuid_t *svc)
{
    return svc->gatt_svc.end_handle <= svc->gatt_svc.start_handle;
}

static svc_uuid_t *esp_ble_conn_svc_find_uuid(esp_ble_conn_session_t *conn_session, const ble_uuid_t *uuid)
{
    svc_uuid_t *svc = NULL;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        if (ble_uuid_cmp(&svc->gatt_svc.uuid.u, uuid) == 0) {
            return svc;
        }
    }

    return NULL;
}

static svc_uuid_t *esp_ble_conn_svc_find_prev(esp_ble_conn_session_t *conn_session, uint16_t svc_start_handle)
{
    svc_uuid_t *prev = NULL;
    svc_uuid_t *svc = NULL;

    prev = NULL;
    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        if (svc->gatt_svc.start_handle >= svc_start_handle) {
            break;
        }

        prev = svc;
    }

    return prev;
}

static svc_uuid_t *esp_ble_conn_svc_find(esp_ble_conn_session_t *conn_session, uint16_t svc_start_handle, svc_uuid_t **out_prev)
{
    svc_uuid_t *prev = NULL;
    svc_uuid_t *svc = NULL;

    prev = esp_ble_conn_svc_find_prev(conn_session, svc_start_handle);
    if (prev == NULL) {
        svc = SLIST_FIRST(&conn_session->uuid_list);
    } else {
        svc = SLIST_NEXT(prev, next);
    }

    if (svc != NULL && svc->gatt_svc.start_handle != svc_start_handle) {
        svc = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }
    return svc;
}

static svc_uuid_t *esp_ble_conn_svc_find_range(esp_ble_conn_session_t *conn_session, uint16_t attr_handle)
{
    svc_uuid_t *svc = NULL;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        if (svc->gatt_svc.start_handle <= attr_handle &&
                svc->gatt_svc.end_handle >= attr_handle) {
            return svc;
        }
    }

    return NULL;
}

static esp_err_t esp_ble_conn_svc_add(esp_ble_conn_session_t *conn_session, const struct ble_gatt_svc *gatt_svc)
{
    svc_uuid_t *prev = NULL;
    svc_uuid_t *svc = NULL;

    svc = esp_ble_conn_svc_find(conn_session, gatt_svc->start_handle, &prev);
    if (svc != NULL) {
        ESP_LOGW(TAG, "Service already discovered, start handle=%d", gatt_svc->start_handle);
        return ESP_OK;
    }

    svc = calloc(1, sizeof(svc_uuid_t));
    if (svc == NULL) {
        ESP_LOGE(TAG, "Service discovered out of memory, start handle=%d", gatt_svc->start_handle);
        return ESP_ERR_NO_MEM;
    }
    svc->gatt_svc = *gatt_svc;
    SLIST_INIT(&svc->chrs);

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&conn_session->uuid_list, svc, next);
    } else {
        SLIST_INSERT_AFTER(prev, svc, next);
    }

    return ESP_OK;
}

static void esp_ble_conn_svc_uuid_del(svc_uuid_t *svc)
{
    chr_uuid_t *chr;

    while ((chr = SLIST_FIRST(&svc->chrs)) != NULL) {
        SLIST_REMOVE_HEAD(&svc->chrs, next);
        esp_ble_conn_chr_uuid_del(chr);
    }
}

static esp_err_t esp_ble_conn_svc_disced(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;
    esp_err_t rc = ESP_OK;

    assert(conn_session->conn_handle == conn_handle);

    switch (error->status) {
    case 0:
        rc = esp_ble_conn_svc_add(conn_session, service);
        break;

    case BLE_HS_EDONE:
        /* All services discovered; start discovering characteristics */
        if (conn_session->chrs_handle) {
            esp_ble_conn_disc_chrs(conn_session);
        }
        break;

    default:
        rc = error->status;
        break;
    }

    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Abort services discovered rc=%d", rc);
        esp_ble_conn_disc_complete(conn_session, rc);
    }

    return rc;
}

static void esp_ble_conn_disc_svcs(esp_ble_conn_session_t *conn_session)
{
    conn_session->chrs_handle = 1;
    esp_err_t rc = ble_gattc_disc_all_svcs(conn_session->conn_handle, esp_ble_conn_svc_disced, conn_session);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to discover services, rc=%d", rc);
        return;
    }
}

static uint16_t esp_ble_conn_chr_end_handle(const svc_uuid_t *svc, const chr_uuid_t *chr)
{
    const chr_uuid_t *next_chr = NULL;

    next_chr = SLIST_NEXT(chr, next);
    if (next_chr != NULL) {
        return next_chr->chr.def_handle - 1;
    } else {
        return svc->gatt_svc.end_handle;
    }
}

static int esp_ble_conn_chr_is_empty(const svc_uuid_t *svc, const chr_uuid_t *chr)
{
    return esp_ble_conn_chr_end_handle(svc, chr) <= chr->chr.val_handle;
}

static chr_uuid_t *esp_ble_conn_chr_find_uuid(esp_ble_conn_session_t *conn_session, const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid)
{
    svc_uuid_t *svc = NULL;
    chr_uuid_t *chr = NULL;

    svc = esp_ble_conn_svc_find_uuid(conn_session, svc_uuid);
    if (svc == NULL) {
        return NULL;
    }

    SLIST_FOREACH(chr, &svc->chrs, next) {
        if (ble_uuid_cmp(&chr->chr.uuid.u, chr_uuid) == 0) {
            return chr;
        }
    }

    return NULL;
}

static chr_uuid_t *esp_ble_conn_chr_uuid_find_prev(const svc_uuid_t *svc, uint16_t chr_val_handle)
{
    chr_uuid_t *prev = NULL;
    chr_uuid_t *chr = NULL;

    prev = NULL;
    SLIST_FOREACH(chr, &svc->chrs, next) {
        if (chr->chr.val_handle >= chr_val_handle) {
            break;
        }

        prev = chr;
    }

    return prev;
}

static chr_uuid_t *esp_ble_conn_chr_uuid_find(const svc_uuid_t *svc, uint16_t chr_val_handle, chr_uuid_t **out_prev)
{
    chr_uuid_t *prev = NULL;
    chr_uuid_t *chr = NULL;

    prev = esp_ble_conn_chr_uuid_find_prev(svc, chr_val_handle);
    if (prev == NULL) {
        chr = SLIST_FIRST(&svc->chrs);
    } else {
        chr = SLIST_NEXT(prev, next);
    }

    if (chr != NULL && chr->chr.val_handle != chr_val_handle) {
        chr = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }

    return chr;
}

static void esp_ble_conn_chr_uuid_del(chr_uuid_t *chr)
{
    dsc_uuid_t *dsc = NULL;

    while ((dsc = SLIST_FIRST(&chr->dscs)) != NULL) {
        SLIST_REMOVE_HEAD(&chr->dscs, next);
        free(dsc);
    }

    free(chr);
}

static esp_err_t esp_ble_conn_chr_uuid_add(esp_ble_conn_session_t *conn_session,  uint16_t svc_start_handle, const struct ble_gatt_chr *gatt_chr)
{
    chr_uuid_t *prev = NULL;
    chr_uuid_t *chr = NULL;
    svc_uuid_t *svc = NULL;

    svc = esp_ble_conn_svc_find(conn_session, svc_start_handle, NULL);
    if (svc == NULL) {
        ESP_LOGE(TAG, "Can't find service for discovered characteristic %d", svc_start_handle);
        return BLE_HS_EUNKNOWN;
    }

    chr = esp_ble_conn_chr_uuid_find(svc, gatt_chr->def_handle, &prev);
    if (chr != NULL) {
        ESP_LOGW(TAG, "Characteristic already discovered %d", gatt_chr->def_handle);
        return ESP_OK;
    }

    chr = calloc(1, sizeof(chr_uuid_t));
    if (chr == NULL) {
        ESP_LOGE(TAG, "Out of memory %d %d", svc_start_handle, gatt_chr->def_handle);
        return BLE_HS_ENOMEM;
    }
    chr->chr = *gatt_chr;
    SLIST_INIT(&chr->dscs);

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&svc->chrs, chr, next);
    } else {
        SLIST_INSERT_AFTER(prev, chr, next);
    }

    return 0;
}

static esp_err_t esp_ble_conn_chr_disced(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;
    esp_err_t rc = ESP_OK;

    assert(conn_session->conn_handle == conn_handle);

    switch (error->status) {
    case 0:
        rc = esp_ble_conn_chr_uuid_add(conn_session, conn_session->svcs_handle->gatt_svc.start_handle, chr);
        break;

    case BLE_HS_EDONE:
        /* All characteristics in this service discovered; start discovering characteristics in the next service */
        if (conn_session->chrs_handle) {
            esp_ble_conn_disc_chrs(conn_session);
        }
        break;

    default:
        rc = error->status;
        break;
    }

    if (rc != 0) {
        ESP_LOGE(TAG, "Abort characteristics in this service discovery rc=%d", rc);
        esp_ble_conn_disc_complete(conn_session, rc);
    }

    return rc;
}

static void esp_ble_conn_disc_chrs(esp_ble_conn_session_t *conn_session)
{
    svc_uuid_t *svc = NULL;
    esp_err_t rc = ESP_OK;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        if (!esp_ble_conn_svc_is_empty(svc) && SLIST_EMPTY(&svc->chrs)) {
            conn_session->svcs_handle = svc;
            rc = ble_gattc_disc_all_chrs(conn_session->conn_handle, svc->gatt_svc.start_handle, svc->gatt_svc.end_handle, esp_ble_conn_chr_disced, conn_session);
            if (rc != 0) {
                ESP_LOGE(TAG, "Abort the discovered service that contains undiscovered characteristics rc=%d", rc);
                esp_ble_conn_disc_complete(conn_session, rc);
            }
            return;
        }
    }

    /* All characteristics discovered, start discovering descriptors */
    esp_ble_conn_disc_dscs(conn_session);
}

static dsc_uuid_t *esp_ble_conn_dsc_find_uuid(esp_ble_conn_session_t *conn_session, const ble_uuid_t *svc_uuid,
                                              const ble_uuid_t *chr_uuid, const ble_uuid_t *dsc_uuid)
{
    chr_uuid_t *chr = NULL;
    dsc_uuid_t *dsc = NULL;

    chr = esp_ble_conn_chr_find_uuid(conn_session, svc_uuid, chr_uuid);
    if (chr == NULL) {
        return NULL;
    }

    SLIST_FOREACH(dsc, &chr->dscs, next) {
        if (ble_uuid_cmp(&dsc->dsc.uuid.u, dsc_uuid) == 0) {
            return dsc;
        }
    }

    return NULL;
}

static dsc_uuid_t *esp_ble_conn_dsc_uuid_find_prev(const chr_uuid_t *chr, uint16_t dsc_handle)
{
    dsc_uuid_t *prev = NULL;
    dsc_uuid_t *dsc = NULL;

    prev = NULL;
    SLIST_FOREACH(dsc, &chr->dscs, next) {
        if (dsc->dsc.handle >= dsc_handle) {
            break;
        }

        prev = dsc;
    }

    return prev;
}

static dsc_uuid_t *esp_ble_conn_dsc_uuid_find(const chr_uuid_t *chr, uint16_t dsc_handle, dsc_uuid_t **out_prev)
{
    dsc_uuid_t *prev = NULL;
    dsc_uuid_t *dsc = NULL;

    prev = esp_ble_conn_dsc_uuid_find_prev(chr, dsc_handle);
    if (prev == NULL) {
        dsc = SLIST_FIRST(&chr->dscs);
    } else {
        dsc = SLIST_NEXT(prev, next);
    }

    if (dsc != NULL && dsc->dsc.handle != dsc_handle) {
        dsc = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }
    return dsc;
}

static esp_err_t esp_ble_conn_dsc_uuid_add(esp_ble_conn_session_t *conn_session, uint16_t chr_val_handle, const struct ble_gatt_dsc *gatt_dsc)
{
    dsc_uuid_t *prev = NULL;
    dsc_uuid_t *dsc = NULL;
    svc_uuid_t *svc = NULL;
    chr_uuid_t *chr = NULL;

    svc = esp_ble_conn_svc_find_range(conn_session, chr_val_handle);
    if (svc == NULL) {
        ESP_LOGE(TAG, "Can't find service for discovered descriptor %d %d", chr_val_handle, gatt_dsc->handle);
        return BLE_HS_EUNKNOWN;
    }

    chr = esp_ble_conn_chr_uuid_find(svc, chr_val_handle, NULL);
    if (chr == NULL) {
        ESP_LOGE(TAG, "Can't find characteristic for discovered descriptor %d %d", chr_val_handle, gatt_dsc->handle);
        return BLE_HS_EUNKNOWN;
    }

    dsc = esp_ble_conn_dsc_uuid_find(chr, gatt_dsc->handle, &prev);
    if (dsc != NULL) {
        ESP_LOGW(TAG, "Descriptor already discovered %d %d", chr_val_handle, gatt_dsc->handle);
        return ESP_OK;
    }

    dsc = calloc(1, sizeof(dsc_uuid_t));
    if (dsc == NULL) {
        ESP_LOGE(TAG, "Out of memory %d %d", chr_val_handle, gatt_dsc->handle);
        return BLE_HS_ENOMEM;
    }
    dsc->dsc = *gatt_dsc;

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&chr->dscs, dsc, next);
    } else {
        SLIST_INSERT_AFTER(prev, dsc, next);
    }

    return ESP_OK;
}

static esp_err_t esp_ble_conn_dsc_disced(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;
    esp_err_t rc = ESP_OK;

    assert(conn_session->conn_handle == conn_handle);

    switch (error->status) {
    case 0:
        rc = esp_ble_conn_dsc_uuid_add(conn_session, chr_val_handle, dsc);
        break;

    case BLE_HS_EDONE:
        /* All descriptors in this characteristic discovered; start discovering
        * descriptors in the next characteristic.
        */
        if (conn_session->chrs_handle > 0) {
            esp_ble_conn_disc_dscs(conn_session);
        }
        break;

    default:
        rc = error->status;
        break;
    }

    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Abort descriptors in this characteristic discovery rc=%d", rc);
        esp_ble_conn_disc_complete(conn_session, rc);
    }

    return rc;
}

static void esp_ble_conn_disc_dscs(esp_ble_conn_session_t *conn_session)
{
    chr_uuid_t *chr = NULL;
    svc_uuid_t *svc = NULL;
    esp_err_t rc = ESP_OK;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        SLIST_FOREACH(chr, &svc->chrs, next) {
            if (!esp_ble_conn_chr_is_empty(svc, chr) &&
                    SLIST_EMPTY(&chr->dscs) &&
                    conn_session->chrs_handle <= chr->chr.def_handle) {

                rc = ble_gattc_disc_all_dscs(conn_session->conn_handle, chr->chr.val_handle, esp_ble_conn_chr_end_handle(svc, chr), esp_ble_conn_dsc_disced, conn_session);
                if (rc != ESP_OK) {
                    ESP_LOGE(TAG, "Abort discovered characteristics that contains undiscovered descriptors rc=%d", rc);
                    esp_ble_conn_disc_complete(conn_session, rc);
                }

                conn_session->chrs_handle = chr->chr.val_handle;
                return;
            }
        }
    }

    /* All descriptors discovered. */
    esp_ble_conn_disc_complete(conn_session, ESP_OK);
}

static void esp_ble_conn_scan(esp_ble_conn_session_t *conn_session)
{
    esp_err_t rc = ESP_OK;

    struct ble_gap_disc_params disc_params;

    /* Tell the controller to filter duplicates; we don't want to process
     * repeated advertisements from the same device.
     */
    disc_params.filter_duplicates = 1;

    /**
     * Perform a passive scan.  I.e., don't send follow-up scan requests to
     * each advertiser.
     */
    disc_params.passive = 1;

    /* Use defaults for the rest of the parameters. */
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(conn_session->own_addr_type, BLE_HS_FOREVER, &disc_params, esp_ble_conn_gap_event, conn_session);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery procedure; rc=%d", rc);
    }
}

static void esp_ble_conn_should_connect(struct ble_gap_disc_desc *disc)
{
    ble_addr_t *addr = NULL;
    esp_err_t rc = ESP_OK;
    struct ble_gap_conn_params conn_params = {
        .scan_itvl = 0x12,
        .scan_window = 0x11,
        .itvl_min = 25,
        .itvl_max = 26,
        .latency = 1,
        .supervision_timeout = 20,
        .min_ce_len = 3,
        .max_ce_len = 4
    };

    if (disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
            disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND) {
        ESP_LOGE(TAG, "The device has to be advertising connectability");
        return;
    }

    /* Scanning must be stopped before a connection can be initiated. */
    rc = ble_gap_disc_cancel();
    if (rc) {
        ESP_LOGE(TAG, "Failed to cancel scan; rc=%d", rc);
        return;
    }

    /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for timeout */
    addr = &disc->addr;

    rc = ble_gap_connect(s_conn_session->own_addr_type, addr, 30000, &conn_params, esp_ble_conn_gap_event, s_conn_session);
    if (rc) {
        ESP_LOGE(TAG, "Error: Failed to connect to device; addr_type=%d, addr="MACSTR", rc=%d", addr->type, MAC2STR(addr->val), rc);
        return;
    }
}

static esp_err_t esp_ble_conn_on_complete(esp_ble_conn_session_t *conn_session, uint16_t conn_handle, uint16_t attr_handle, struct os_mbuf *om)
{
    esp_err_t rc = ESP_OK;

    uint8_t *data_buf = NULL;
    uint16_t data_len = 0;

    svc_uuid_t *svc = NULL;
    chr_uuid_t *chr = NULL;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        chr = esp_ble_conn_chr_uuid_find(svc, attr_handle, &chr);
        if (chr) {
            break;
        }
    }

    if (!chr) {
        ESP_LOGE(TAG, "Incorrect attr_handle %d", attr_handle);
        return ESP_ERR_INVALID_ARG;
    }

    /* Save the length of entire data */
    data_len = OS_MBUF_PKTLEN(om);
    data_buf = calloc(1, data_len);
    if (data_buf == NULL) {
        ESP_LOGE(TAG, "Error allocating memory for characteristic value");
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    rc = ble_hs_mbuf_to_flat(om, data_buf, data_len, &data_len);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error getting data from memory buffers");
        free(data_buf);
        return BLE_ATT_ERR_UNLIKELY;
    }

    return esp_ble_conn_on_gatts_attr_value_set(attr_handle, &chr->chr.uuid.u, data_len, data_buf);
}

static esp_err_t esp_ble_conn_on_write(uint16_t conn_handle,
                 const struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr,
                 void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;

    ESP_LOGD(TAG, "Write complete; status=%d conn_handle=%d attr_handle=%d\n",
                error->status, conn_handle, attr->handle);
    if (!error->status) {
        xSemaphoreGive(conn_session->semaphore);
    }

    return ESP_OK;
}

static esp_err_t esp_ble_conn_on_read(uint16_t conn_handle,
                 const struct ble_gatt_error *error,
                 struct ble_gatt_attr *attr,
                 void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;

    switch (error->status) {
        case ESP_OK:
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d "
                        "attr_handle=%d len=%d value=", conn_handle,
                        attr->handle, OS_MBUF_PKTLEN(attr->om));

            if (!esp_ble_conn_on_complete(conn_session, conn_handle, attr->handle, attr->om)) {
                xSemaphoreGive(conn_session->semaphore);
            }
            break;

        case BLE_HS_EDONE:
            ESP_LOGI(TAG, "characteristic read complete\n");
            break;

        default:
            break;
    }

    return ESP_OK;
}
#endif

static esp_err_t esp_ble_conn_event_send(esp_ble_conn_session_t *conn_session, esp_ble_conn_event_t event, void *data, size_t data_len, void *handle)
{
    esp_ble_conn_event_ctx_t framework_event = {
        .event = event,
        .data = data,
        .data_len = data_len,
        .handle = handle,
    };

    BaseType_t ret = xQueueSend(conn_session->queue, &framework_event, 0);

    return ((ret == pdTRUE) ? ESP_OK : ESP_FAIL);
}

static esp_ble_conn_character_t *esp_ble_conn_find_character_with_uuid(const ble_uuid_t *ble_uuid)
{
    svc_uuid_t *svc_uuid = NULL;
    uint8_t type = 0;
    esp_ble_conn_uuid_t uuid;

    switch (ble_uuid->type) {
        case BLE_UUID_TYPE_16:
            type = BLE_CONN_UUID_TYPE_16;
            uuid.uuid16 = BLE_UUID16(ble_uuid)->value;
            break;
        case BLE_UUID_TYPE_32:
            type = BLE_CONN_UUID_TYPE_32;
            uuid.uuid32 = BLE_UUID32(ble_uuid)->value;
            break;
        case BLE_UUID_TYPE_128:
            type = BLE_CONN_UUID_TYPE_128;
            memcpy(uuid.uuid128, BLE_UUID128(ble_uuid)->value, BLE_UUID128_VAL_LEN);
            break;
        default:
            break;
    }

    SLIST_FOREACH(svc_uuid, &s_conn_session->uuid_list, next) {
        for (int i = 0; i < svc_uuid->svc.nu_lookup_count; i ++) {
            if (BLE_UUID_CMP(type, svc_uuid->svc.nu_lookup[i].uuid, uuid)) {
                return &svc_uuid->svc.nu_lookup[i];
            }
        }
    }

    return NULL;
}

static attr_mbuf_t *esp_ble_conn_find_attr_with_uuid(uint8_t type, esp_ble_conn_uuid_t uuid)
{
    attr_mbuf_t *attr_mbuf = NULL;

    SLIST_FOREACH(attr_mbuf, &s_conn_session->mbuf_list, next) {
        if (!attr_mbuf) {
            continue;
        }

        if (BLE_UUID_CMP(type, attr_mbuf->uuid, uuid)) {
            return attr_mbuf;
        }
    }

    return NULL;
}

static attr_mbuf_t *esp_ble_conn_find_attr_with_handle(uint16_t attr_handle)
{
    attr_mbuf_t *attr_mbuf = NULL;

    SLIST_FOREACH(attr_mbuf, &s_conn_session->mbuf_list, next) {
        if (attr_mbuf && attr_mbuf->attr_handle == attr_handle) {
            return attr_mbuf;
        }
    }

    return NULL;
}

static esp_err_t esp_ble_conn_on_gatts_attr_value_set(uint16_t attr_handle, const ble_uuid_t *ble_uuid, uint16_t outlen, uint8_t *outbuf)
{
    attr_mbuf_t *attr_mbuf = esp_ble_conn_find_attr_with_handle(attr_handle);
    uint8_t type = 0;
    esp_ble_conn_uuid_t uuid;

    switch (ble_uuid->type) {
        case BLE_UUID_TYPE_16:
            type = BLE_CONN_UUID_TYPE_16;
            uuid.uuid16 = BLE_UUID16(ble_uuid)->value;
            break;
        case BLE_UUID_TYPE_32:
            type = BLE_CONN_UUID_TYPE_32;
            uuid.uuid32 = BLE_UUID32(ble_uuid)->value;
            break;
        case BLE_UUID_TYPE_128:
            type = BLE_CONN_UUID_TYPE_128;
            memcpy(uuid.uuid128, BLE_UUID128(ble_uuid)->value, BLE_UUID128_VAL_LEN);
            break;
        default:
            break;
    }

    if (!attr_mbuf) {
        attr_mbuf = calloc(1, sizeof(attr_mbuf_t));
        if (!attr_mbuf) {
            ESP_LOGE(TAG, "Failed to allocate memory for storing outbuf and outlen");
            return ESP_ERR_NO_MEM;
        }
        SLIST_INSERT_HEAD(&s_conn_session->mbuf_list, attr_mbuf, next);
        attr_mbuf->attr_handle = attr_handle;
        attr_mbuf->type = type;
        memcpy(&attr_mbuf->uuid, &uuid, sizeof(uuid));
    } else {
        if (attr_mbuf->outbuf) {
            free(attr_mbuf->outbuf);
            attr_mbuf->outbuf = NULL;
        }
    }

    attr_mbuf->outbuf = outbuf;
    attr_mbuf->outlen = outlen;

    return ESP_OK;
}

static esp_err_t esp_ble_conn_on_gatts_attr_value_get(uint16_t attr_handle, uint16_t *outlen, uint8_t **outbuf)
{
    attr_mbuf_t *attr_mbuf = esp_ble_conn_find_attr_with_handle(attr_handle);

    if (!attr_mbuf) {
        ESP_LOGE(TAG, "Outbuf with handle %d not found", attr_handle);
        return ESP_ERR_NOT_FOUND;
    }

    *outbuf = attr_mbuf->outbuf;
    *outlen = attr_mbuf->outlen;

    return ESP_OK;
}

#if defined(CONFIG_BLE_CONN_MGR_EXTENDED_ADV)

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV)
static void esp_ble_conn_periodic_advertise(esp_ble_conn_session_t *conn_session)
{
    esp_err_t rc = ESP_OK;

    struct ble_gap_periodic_adv_params adv_params;
    struct os_mbuf *data = NULL;

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.include_tx_power = (CONFIG_BLE_CONN_MGR_PERIODIC_ADV_CAP & BIT(0));
    adv_params.itvl_min = 160;
    adv_params.itvl_max = 240;

    rc = ble_gap_periodic_adv_configure(conn_session->conn_handle, &adv_params);
    if (rc) {
        ESP_LOGE(TAG, "Configure periodic advertising error; rc=%d", rc);
        return;
    }

    if (conn_session->per_adv_data_buf) {
        data = os_msys_get_pkthdr(conn_session->per_adv_data_len, 0);
        if (!data) {
            ESP_LOGE(TAG, "Allocate memory for periodic adv data error!");
            return;
        }

        rc = os_mbuf_append(data, conn_session->per_adv_data_buf, conn_session->per_adv_data_len);
        if (rc){
            ESP_LOGE(TAG, "Append periodic adv data onto a mbuf error; rc=%d", rc);
            return;
        }

        rc = ble_gap_periodic_adv_set_data(conn_session->conn_handle, data);
        if (rc != 0) {
            ESP_LOGE(TAG, "Setting periodic adv data error; rc = %d", rc);
            return;
        }
    }

    rc = ble_gap_periodic_adv_start(conn_session->conn_handle);
    if (rc) {
        ESP_LOGE(TAG, "Start periodic advertising error; rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "Instance %u started (periodic)", conn_session->conn_handle);
}
#endif

static void esp_ble_conn_ext_advertise(esp_ble_conn_session_t *conn_session)
{
    esp_err_t rc = ESP_OK;

    struct ble_gap_ext_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    ble_addr_t addr;
    struct os_mbuf *data = NULL;

    /* For periodic we use instance with non-connectable advertising */
    memset (&adv_params, 0, sizeof(adv_params));

    /* advertise using random addr */
    adv_params.own_addr_type = BLE_OWN_ADDR_RANDOM;
    adv_params.primary_phy = BLE_HCI_LE_PHY_1M;
    adv_params.secondary_phy = BLE_HCI_LE_PHY_2M;
    adv_params.sid = 2;

    /* extended advertising capability */
    adv_params.connectable = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(0));
    adv_params.scannable = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(1));
    adv_params.directed = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(2));
    adv_params.high_duty_directed = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(3));
    adv_params.legacy_pdu = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(4));
    adv_params.anonymous = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(5));
    adv_params.include_tx_power = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(6));
    adv_params.scan_req_notif = (CONFIG_BLE_CONN_MGR_EXTENDED_ADV_CAP & BIT(7));

    /* configure instance 1 */
    rc = ble_gap_ext_adv_configure(conn_session->conn_handle, &adv_params, NULL, NULL, NULL);
    if (rc) {
        ESP_LOGE(TAG, "Configure extended advertising instance error; rc=%d", rc);
        return;
    }

    /* set random (NRPA) address for instance */
    rc = ble_hs_id_gen_rnd(1, &addr);
    if (rc) {
        ESP_LOGE(TAG, "Generates a new random address error; rc=%d", rc);
        return;
    }

    rc = ble_gap_ext_adv_set_addr(conn_session->conn_handle, &addr);
    if (rc) {
        ESP_LOGE(TAG, "Set random address for configured advertising instance error; rc=%d", rc);
        return;
    }

    memset(&fields, 0, sizeof(fields));
    fields.name = (const uint8_t *)ble_svc_gap_device_name();
    if (fields.name) {
        fields.name_len = strlen((const char *)fields.name);
    }

    if (conn_session->adv_data_buf) {
        data = os_msys_get_pkthdr(conn_session->adv_data_len, 0);
        if (!data) {
            ESP_LOGE(TAG, "Allocate memory for ext adv data error!");
            return;
        }

        rc = os_mbuf_append(data, conn_session->adv_data_buf, conn_session->adv_data_len);
        if (rc){
            ESP_LOGE(TAG, "Append ext adv data onto a mbuf error; rc=%d", rc);
            return;
        }
    } else {
        data = os_msys_get_pkthdr(BLE_HCI_MAX_ADV_DATA_LEN, 0);
        if (!data) {
            ESP_LOGE(TAG, "Allocate memory for ext adv data error!");
            return;
        }

        rc = ble_hs_adv_set_fields_mbuf(&fields, data);
        if (rc != 0) {
            ESP_LOGE(TAG, "Setting ext adv data error; rc = %d", rc);
            return;
        }
    }

    rc = ble_gap_ext_adv_set_data(conn_session->conn_handle, data);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error in setting ext adv data; rc = %d", rc);
        return;
    }

    if (adv_params.scannable && conn_session->adv_rsp_data_buf) {
        data = os_msys_get_pkthdr(conn_session->adv_rsp_data_len, 0);
        if (!data) {
            ESP_LOGE(TAG, "Allocate memory for ext adv rsp data error!");
            return;
        }

        rc = os_mbuf_append(data, conn_session->adv_rsp_data_buf, conn_session->adv_rsp_data_len);
        if (rc){
            ESP_LOGE(TAG, "Append ext adv rsp data onto a mbuf error; rc=%d", rc);
            return;
        }

        rc = ble_gap_ext_adv_rsp_set_data(conn_session->conn_handle, data);
        if (rc != 0) {
            ESP_LOGE(TAG, "Error in setting ext adv rsp data; rc = %d", rc);
            return;
        }
    }

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV)
    esp_ble_conn_periodic_advertise(conn_session);
#endif

    rc = ble_gap_ext_adv_start(conn_session->conn_handle, 0, 0);
    if (rc) {
        ESP_LOGE(TAG, "Start ext advertising instance, rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "Instance %u started (extended)", conn_session->conn_handle);
}
#endif

static void esp_ble_conn_advertise(esp_ble_conn_session_t *conn_session)
{
#if defined(CONFIG_BLE_CONN_MGR_EXTENDED_ADV)
    conn_session->conn_handle = 1;
    esp_ble_conn_ext_advertise(conn_session);
#else
    esp_err_t rc = ESP_OK;

    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */
    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (const uint8_t *)ble_svc_gap_device_name();
    if (fields.name) {
        fields.name_len = strlen((const char *)fields.name);
        fields.name_is_complete = 1;
    }

    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    if (conn_session->adv_data_buf) {
        rc = ble_gap_adv_set_data(conn_session->adv_data_buf, conn_session->adv_data_len);
        if (rc != 0) {
            ESP_LOGE(TAG, "Error in setting name; rc = %d", rc);
            return;
        }
    }

    if (conn_session->adv_rsp_data_buf) {
        rc = ble_gap_adv_rsp_set_data(conn_session->adv_rsp_data_buf, conn_session->adv_rsp_data_len);
        if (rc != 0) {
            ESP_LOGE(TAG, "Error in setting manufacturer; rc = %d", rc);
            return;
        }
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = 0x100;
    adv_params.itvl_max = 0x100;

    rc = ble_gap_adv_start(conn_session->own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, esp_ble_conn_gap_event, conn_session);
    if (rc != 0) {
        /* If BLE Host is disabled, it probably means device is already
         * provisioned in previous session. Avoid error prints for this case.*/
        if (rc == BLE_HS_EDISABLED) {
            ESP_LOGD(TAG, "BLE Host is disabled !!");
        } else {
            ESP_LOGE(TAG, "Error enabling advertisement; rc = %d", rc);
        }
        return;
    }
#endif
}

#ifndef CONFIG_BLE_CONN_MGR_EXTENDED_ADV
static int esp_ble_conn_gap_event(struct ble_gap_event *event, void *arg)
{
    esp_err_t rc = ESP_OK;
    esp_ble_conn_session_t *conn_session = arg;

    switch (event->type) {
#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
        case BLE_GAP_EVENT_DISC:
            ESP_LOGD(TAG, "BLE_GAP_EVENT_DISC");
            struct ble_hs_adv_fields fields;
            rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
            if (rc != ESP_OK) {
                return 0;
            } else {
                /* Try to connect to the advertiser if it looks interesting. */
                if (fields.name != NULL && !memcmp(fields.name, conn_session->device_name, fields.name_len)) {
                    esp_ble_conn_should_connect(&event->disc);
                }
            }
            break;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            break;
        case BLE_GAP_EVENT_NOTIFY_RX:
            ESP_LOGI(TAG, "received %s; conn_handle=%d attr_handle=%d "
                    "attr_len=%d\n",
                    event->notify_rx.indication ?
                    "indication" :
                    "notification",
                    event->notify_rx.conn_handle,
                    event->notify_rx.attr_handle,
                    OS_MBUF_PKTLEN(event->notify_rx.om));
            svc_uuid_t *svc = NULL;
            chr_uuid_t *chr = NULL;

            SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
                chr = esp_ble_conn_chr_uuid_find(svc, event->notify_rx.attr_handle, &chr);
                if (chr) {
                    break;
                }
            }

            if (!chr) {
                ESP_LOGE(TAG, "Incorrect attr_handle %d with %s received", event->notify_rx.attr_handle,
                              event->notify_rx.indication ? "indication" : "notification");
            } else {
                esp_ble_conn_data_t conn_data = {
                    .write_conn_id = event->notify_rx.conn_handle,
                };
                /* Save the length of entire data */
                conn_data.data_len = OS_MBUF_PKTLEN(event->notify_rx.om);
                conn_data.data = calloc(1, conn_data.data_len);
                if (conn_data.data == NULL) {
                    ESP_LOGE(TAG, "Error allocating memory for characteristic value");
                    return BLE_ATT_ERR_INSUFFICIENT_RES;
                }

                rc = ble_hs_mbuf_to_flat(event->notify_rx.om, conn_data.data, conn_data.data_len, &conn_data.data_len);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Error getting data from memory buffers");
                    free(conn_data.data);
                    return BLE_ATT_ERR_UNLIKELY;
                }

                conn_data.type = chr->chr.uuid.u.type;
                ble_uuid_flat(&chr->chr.uuid.u, &conn_data.uuid);
                rc = esp_ble_conn_on_gatts_attr_value_set(event->notify_rx.attr_handle, &chr->chr.uuid.u, conn_data.data_len, conn_data.data);
                esp_event_post(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_DATA_RECEIVE, &conn_data, sizeof(conn_data), portMAX_DELAY);
            }
            break;
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC)
        case BLE_GAP_EVENT_EXT_DISC:
            /* An advertisement report was received during GAP discovery. */
            ESP_LOGD(TAG, "BLE_GAP_EVENT_EXT_DISC");
            struct ble_gap_ext_disc_desc *disc = ((struct ble_gap_ext_disc_desc *)(&event->disc));
            if (disc->sid == 2 && !conn_session->ble_bonding) {
                conn_session->ble_bonding = true;
                const ble_addr_t addr;
                uint8_t adv_sid;
                struct ble_gap_periodic_sync_params params;
                memcpy((void *)&addr, (void *)&disc->addr, sizeof(disc->addr));
                memcpy(&adv_sid, &disc->sid, sizeof(disc->sid));
                params.skip = 10;
                params.sync_timeout = 1000;
                params.reports_disabled = (CONFIG_BLE_CONN_MGR_PERIODIC_SYNC_CAP & BIT(0));
                rc = ble_gap_periodic_adv_sync_create(&addr, adv_sid, &params, esp_ble_conn_gap_event, conn_session);
                if (rc) {
                    ESP_LOGE(TAG, "Performs the Synchronization procedure with periodic advertiser error; rc=%d", rc);
                }
            }
            break;
        case BLE_GAP_EVENT_PERIODIC_SYNC:
            ESP_LOGI(TAG, "BLE_GAP_EVENT_PERIODIC_SYNC");
            esp_ble_conn_periodic_sync_t periodic_sync = {
                .status = event->periodic_sync.status,
                .sync_handle = event->periodic_sync.sync_handle,
                .sid = event->periodic_sync.sid,
                .adv_phy = event->periodic_sync.adv_phy,
                .per_adv_ival = event->periodic_sync.per_adv_ival,
                .adv_clk_accuracy = event->periodic_sync.adv_clk_accuracy
            };
            memcpy(periodic_sync.adv_addr, event->periodic_sync.adv_addr.val, 6);
            conn_session->conn_handle = event->periodic_sync.sync_handle;
            esp_event_post(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_PERIODIC_SYNC, &periodic_sync, sizeof(periodic_sync), portMAX_DELAY);
            break;
        case BLE_GAP_EVENT_PERIODIC_REPORT:
            ESP_LOGD(TAG, "BLE_GAP_EVENT_PERIODIC_REPORT");
            esp_ble_conn_periodic_report_t periodic_report = {
                .sync_handle = event->periodic_report.sync_handle,
                .tx_power = event->periodic_report.tx_power,
                .rssi = event->periodic_report.rssi,
                .data_status = event->periodic_report.data_status,
                .data_length = event->periodic_report.data_length,
                .data = event->periodic_report.data
            };
            esp_event_post(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_PERIODIC_REPORT, &periodic_report, sizeof(esp_ble_conn_periodic_report_t), portMAX_DELAY);
            break;
        case BLE_GAP_EVENT_PERIODIC_SYNC_LOST:
            ESP_LOGD(TAG, "BLE_GAP_EVENT_PERIODIC_SYNC_LOST");
            esp_ble_conn_periodic_sync_lost_t periodic_sync_lost;
            memcpy(&periodic_sync_lost, &event->periodic_sync_lost, sizeof(esp_ble_conn_periodic_sync_lost_t));
            conn_session->ble_bonding = false;
            conn_session->conn_handle = 0;
            rc = ble_gap_periodic_adv_sync_terminate(event->periodic_sync_lost.sync_handle);
            if (rc) {
                ESP_LOGE(TAG, "Terminate synchronization procedure error; rc=%d", rc);
            }
            esp_event_post(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_PERIODIC_SYNC_LOST, &periodic_sync_lost, sizeof(esp_ble_conn_periodic_sync_lost_t), portMAX_DELAY);
            break;
#endif
#endif
        case BLE_GAP_EVENT_NOTIFY_TX:
            if (event->notify_tx.indication) {
                if ((event->notify_tx.status == BLE_HS_EDONE) || (event->notify_tx.status == BLE_HS_ETIMEOUT)) {
                    xSemaphoreGive(conn_session->semaphore);
                }
            } else {
                if (event->notify_tx.status == 0) {
                    xSemaphoreGive(conn_session->semaphore);
                }
            }
            break;
        case BLE_GAP_EVENT_CONNECT:
            /* A new connection was established or a connection attempt failed. */
            if (event->connect.status == 0) {
                struct ble_gap_conn_desc desc;
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                if (rc != ESP_OK) {
                    ESP_LOGE(TAG, "No open connection with the specified handle");
                } else {
                    conn_session->connect_cb(event, arg);
                }
            } else {
                /* Connection failed; resume advertising. */
                esp_ble_conn_advertise(conn_session);
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGD(TAG, "disconnect; reason=%d ", event->disconnect.reason);
            conn_session->disconnect_cb(event, arg);

            /* Connection terminated; resume advertising. */
            esp_ble_conn_advertise(conn_session);
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            esp_ble_conn_advertise(conn_session);
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            break;
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
            conn_session->set_mtu_cb(event, arg);
            break;
        default:
            break;
    }

    return rc;
}
#endif

static int esp_ble_conn_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    esp_err_t rc = ESP_OK;
    char buf[BLE_UUID_STR_LEN];

    uint8_t *data_buf = NULL;
    uint16_t data_len = 0;
    uint16_t data_buf_len = 0;

    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    esp_ble_conn_character_t *chr = esp_ble_conn_find_character_with_uuid(ctxt->chr->uuid);

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            ESP_LOGI(TAG, "Read attempted for characteristic UUID = %s, attr_handle = %d",
                            ble_uuid_to_str(ctxt->chr->uuid, buf), attr_handle);
            if (chr && chr->uuid_fn && !chr->uuid_fn(NULL, 0, &outbuf, &outlen, NULL)) {
                esp_ble_conn_on_gatts_attr_value_set(attr_handle, ctxt->chr->uuid, outlen, outbuf);
            }

            rc = esp_ble_conn_on_gatts_attr_value_get(attr_handle, &outlen, &outbuf);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to read characteristic with attr_handle = %d", attr_handle);
                return rc;
            }
            rc = os_mbuf_append(ctxt->om, outbuf, outlen);
            break;
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            /* If empty packet is received, return */
            if (ctxt->om->om_len == 0) {
                ESP_LOGD(TAG,"Empty packet");
                return ESP_OK;
            }

            /* Save the length of entire data */
            data_len = OS_MBUF_PKTLEN(ctxt->om);
            ESP_LOGI(TAG, "Write attempt for uuid = %s, attr_handle = %d, data_len = %d",
                            ble_uuid_to_str(ctxt->chr->uuid, buf), attr_handle, data_len);

            data_buf = calloc(1, data_len);
            if (data_buf == NULL) {
                ESP_LOGE(TAG, "Error allocating memory for characteristic value");
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }

            rc = ble_hs_mbuf_to_flat(ctxt->om, data_buf, data_len, &data_buf_len);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error getting data from memory buffers");
                free(data_buf);
                return BLE_ATT_ERR_UNLIKELY;
            }

            if (chr) {
                if (chr->uuid_fn) {
                    rc = chr->uuid_fn(data_buf, data_buf_len, &outbuf, &outlen, NULL);
                } else {
                    esp_ble_conn_event_send(s_conn_session, ESP_BLE_CONN_EVENT_DATA_RECEIVE, data_buf, data_buf_len, NULL);
                }
            } else {
                ESP_LOGE(TAG, "Error getting character from uuid buffers");
            }

            rc = esp_ble_conn_on_gatts_attr_value_set(attr_handle, ctxt->chr->uuid, outlen, outbuf);

            free(data_buf);
            data_buf = NULL;
            break;

        default:
            rc = BLE_ATT_ERR_UNLIKELY;
            break;
    }

    return rc;
}

static void esp_ble_conn_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void esp_ble_conn_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error loading address");
        return;
    }

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &s_conn_session->own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }

#if defined(CONFIG_BLE_CONN_MGR_ROLE_PERIPHERAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
    /* Begin advertising. */
    esp_ble_conn_advertise(s_conn_session);
#endif

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
    /* Begin scanning for a peripheral to connect to. */
    esp_ble_conn_scan(s_conn_session);
#endif
}

static void esp_ble_conn_on_gatts_register(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];
    uint8_t *outbuf = NULL;
    uint16_t outlen = 0;
    esp_ble_conn_character_t *chr = NULL;

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle,
                 ctxt->chr.val_handle);
        chr = esp_ble_conn_find_character_with_uuid(ctxt->chr.chr_def->uuid);
        if (chr) {
            if (chr->uuid_fn) {
                chr->uuid_fn(NULL, 0, &outbuf, &outlen, NULL);
            }
        } else {
            ESP_LOGI(TAG, "No characteristic(%s) found", ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf));
        }
        esp_ble_conn_on_gatts_attr_value_set(ctxt->chr.val_handle, ctxt->chr.chr_def->uuid, outlen, outbuf);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

static void esp_ble_conn_host_task(void *param)
{
    /* This function will return only when nimble_port_stop() is executed */
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();

    nimble_port_freertos_deinit();
}

static esp_err_t esp_ble_conn_gatts_init(esp_ble_conn_session_t *conn_session)
{
    esp_err_t rc = ESP_OK;
#if defined(CONFIG_BLE_CONN_MGR_ROLE_PERIPHERAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(conn_session->gatt_db);
    if (rc != ESP_OK) {
        return rc;
    }

    rc = ble_gatts_add_svcs(conn_session->gatt_db);
    if (rc != ESP_OK) {
        return rc;
    }
#endif

    return rc;
}

/* Function to add descriptor to characteristic. The value of descriptor is
 * filled with corresponding protocomm endpoint names. Characteristic address,
 * its serial no. and XXX 16 bit standard UUID for descriptor to be provided as
 * input parameters. Returns 0 on success and returns ESP_ERR_NO_MEM on
 * failure. */
static int
ble_gatt_add_char_dsc(struct ble_gatt_chr_def *characteristics, esp_ble_conn_character_t *nu_lookup, int index, uint16_t dsc_uuid)
{
    ble_uuid_t *uuid16 = BLE_UUID16_DECLARE(dsc_uuid);

    /* Allocate memory for 2 descriptors, the 2nd descriptor shall be all NULLs
     * to indicate End of Descriptors. */
    (characteristics + index)->descriptors = (struct ble_gatt_dsc_def *) calloc(2, sizeof(struct ble_gatt_dsc_def));
    if ((characteristics + index)->descriptors == NULL) {
        ESP_LOGE(TAG, "Error while allocating memory for characteristic descriptor");
        return ESP_ERR_NO_MEM;
    }

    (characteristics + index)->descriptors[0].uuid = (ble_uuid_t *) calloc(1, sizeof(ble_uuid16_t));
    if ((characteristics + index)->descriptors[0].uuid == NULL) {
        ESP_LOGE(TAG, "Error while allocating memory for characteristic descriptor");
        return ESP_ERR_NO_MEM;
    }

    memcpy((void *)(characteristics + index)->descriptors[0].uuid, uuid16, sizeof(ble_uuid16_t));
    (characteristics + index)->descriptors[0].att_flags = BLE_ATT_F_READ;
    (characteristics + index)->descriptors[0].access_cb = esp_ble_conn_access_cb;
    (characteristics + index)->descriptors[0].arg = (void *)nu_lookup[index].name;

    return ESP_OK;
}

/* Function to add characteristics to the service. For simplicity the
 * flags and access callbacks are same for all the characteristics. The Fn
 * requires pointer to characteristic of service and index of characteristic,
 * depending upon the index no. individual characteristics can be handled in
 * future. The fn also assumes that the required memory for all characteristics
 * is already allocated while adding corresponding service. Returns 0 on
 * success and returns ESP_ERR_NO_MEM on failure to add characteristic. */
static int
ble_gatt_add_characteristics(struct ble_gatt_chr_def *characteristics, esp_ble_conn_character_t *nu_lookup, int index)
{
    /* Allocate space for the characteristics UUID as well */
    ble_uuid_t *ble_uuid = NULL;
    uint8_t     ble_uuid_len = 0;
    ble_uuid_any_t uuid_any;
    switch (nu_lookup[index].type) {
        case BLE_CONN_UUID_TYPE_16:
            ble_uuid_init_from_buf(&uuid_any, &nu_lookup[index].uuid.uuid16, 2);
            ble_uuid = (ble_uuid_t *)&uuid_any.u16;
            ble_uuid_len = sizeof(ble_uuid16_t);
            break;
        case BLE_CONN_UUID_TYPE_32:
            ble_uuid_init_from_buf(&uuid_any, &nu_lookup[index].uuid.uuid32, 4);
            ble_uuid = (ble_uuid_t *)&uuid_any.u32;
            ble_uuid_len = sizeof(ble_uuid32_t);
            break;
        case BLE_CONN_UUID_TYPE_128:
            ble_uuid_init_from_buf(&uuid_any, nu_lookup[index].uuid.uuid128, BLE_UUID128_VAL_LEN);
            ble_uuid = (ble_uuid_t *)&uuid_any.u128;
            ble_uuid_len = sizeof(ble_uuid128_t);
            break;
        default:
            break;
    }
    (characteristics + index)->uuid = (ble_uuid_t *)calloc(1, ble_uuid_len);
    if ((characteristics + index)->uuid == NULL) {
        ESP_LOGE(TAG, "Error allocating memory for characteristic UUID");
        return ESP_ERR_NO_MEM;
    }

    memcpy((void *)(characteristics + index)->uuid, ble_uuid, ble_uuid_len);
    (characteristics + index)->access_cb = esp_ble_conn_access_cb;
    (characteristics + index)->flags = nu_lookup[index].flag;

    return ESP_OK;
}

/* Function to add primary service. It also allocates memory for the
 * characteristics. Returns 0 on success, returns ESP_ERR_NO_MEM on failure to
 * add service. */
static int
ble_gatt_add_primary_svcs(struct ble_gatt_svc_def *gatt_db_svcs, int char_count)
{
    gatt_db_svcs->type = BLE_GATT_SVC_TYPE_PRIMARY;

    /* Allocate (number of characteristics + 1) memory for characteristics, the
     * additional characteristic consist of all 0s indicating end of
     * characteristics */
    gatt_db_svcs->characteristics = (struct ble_gatt_chr_def *) calloc((char_count + 1), sizeof(struct ble_gatt_chr_def));
    if (gatt_db_svcs->characteristics == NULL) {
        ESP_LOGE(TAG, "Memory allocation for GATT characteristics failed");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static int
esp_ble_conn_populate_gatt_db(esp_ble_conn_session_t *conn_session)
{
    struct ble_gatt_svc_def *gatt_svr_svcs = NULL;

    /* Allocate (number of services + 1) memory for services, the additional services
     * consist of all 0s indicating end of services */
    gatt_svr_svcs = (struct ble_gatt_svc_def *) calloc(conn_session->nu_lookup_count + 1, sizeof(struct ble_gatt_svc_def));
    if (gatt_svr_svcs == NULL) {
        ESP_LOGE(TAG, "Error allocating memory for GATT services");
        return ESP_ERR_NO_MEM;
    }

    svc_uuid_t *svc_uuid = NULL;
    uint8_t         index = 0;
    SLIST_FOREACH(svc_uuid, &conn_session->uuid_list, next) {
        /* Allocate space for 1st service UUID as well*/
        ble_uuid_t *ble_uuid = NULL;
        uint8_t     ble_uuid_len = 0;
        ble_uuid_any_t uuid_any;
        switch (svc_uuid->svc.type) {
            case BLE_CONN_UUID_TYPE_16:
                ble_uuid_init_from_buf(&uuid_any, &svc_uuid->svc.uuid.uuid16, 2);
                ble_uuid = (ble_uuid_t *)&uuid_any.u16;
                ble_uuid_len = sizeof(ble_uuid16_t);
                break;
            case BLE_CONN_UUID_TYPE_32:
                ble_uuid_init_from_buf(&uuid_any, &svc_uuid->svc.uuid.uuid32, 4);
                ble_uuid = (ble_uuid_t *)&uuid_any.u32;
                ble_uuid_len = sizeof(ble_uuid32_t);
                break;
            case BLE_CONN_UUID_TYPE_128:
                ble_uuid_init_from_buf(&uuid_any, svc_uuid->svc.uuid.uuid128, BLE_UUID128_VAL_LEN);
                ble_uuid = (ble_uuid_t *)&uuid_any.u128;
                ble_uuid_len = sizeof(ble_uuid128_t);
                break;
            default:
                break;
        }
        gatt_svr_svcs[index].uuid = (ble_uuid_t *) calloc(1, ble_uuid_len);
        if (gatt_svr_svcs[index].uuid == NULL) {
            ESP_LOGE(TAG, "Error allocating memory for GATT service UUID");
            return ESP_ERR_NO_MEM;
        }

        /* Prepare UUID for primary service from config service UUID. */
        memcpy((void *)gatt_svr_svcs[index].uuid, ble_uuid, ble_uuid_len);

        /* GATT: Add primary service. */
        int rc = ble_gatt_add_primary_svcs(&gatt_svr_svcs[index], svc_uuid->svc.nu_lookup_count);
        if (rc != 0) {
            ESP_LOGE(TAG, "Error adding primary service !!!");
            return rc;
        }

        for (int i = 0 ; i < svc_uuid->svc.nu_lookup_count; i++) {
            /* GATT: Add characteristics to the service at index no. i*/
            rc = ble_gatt_add_characteristics((void *)gatt_svr_svcs[index].characteristics, svc_uuid->svc.nu_lookup, i);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error adding GATT characteristic !!!");
                return rc;
            }
            /* GATT: Add user description to characteristic no. i*/
            rc = ble_gatt_add_char_dsc((void *)gatt_svr_svcs[index].characteristics, svc_uuid->svc.nu_lookup, i, BLE_GATT_UUID_CHAR_DSC);
            if (rc != 0) {
                ESP_LOGE(TAG, "Error adding GATT Descriptor !!");
                return rc;
            }
        }

        index ++;
    }

    conn_session->gatt_db = gatt_svr_svcs;

    return ESP_OK;
}

static void ble_gatt_svc_free(struct ble_gatt_svc_def *gatt_db_svcs, uint8_t nu_loopup_count)
{
    /* Free up gatt_db memory if exists */
    if (gatt_db_svcs->characteristics) {
        for (int i = 0; i < nu_loopup_count; i++) {
            if ((gatt_db_svcs->characteristics + i)->descriptors) {
                if ((gatt_db_svcs->characteristics + i)->descriptors->uuid) {
                    free((void *)(gatt_db_svcs->characteristics + i)->descriptors->uuid);
                    (gatt_db_svcs->characteristics + i)->descriptors->uuid = NULL;
                }

                free((gatt_db_svcs->characteristics + i)->descriptors);
            }

            if ((gatt_db_svcs->characteristics + i)->uuid) {
                free((void *)(gatt_db_svcs->characteristics + i)->uuid);
            }
        }

        free((void *)(gatt_db_svcs->characteristics));
        gatt_db_svcs->characteristics = NULL;
    }

    if (gatt_db_svcs->uuid) {
        free((void *)gatt_db_svcs->uuid);
        gatt_db_svcs->uuid = NULL;
    }
}

static void esp_ble_conn_connect_cb(struct ble_gap_event *event, void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;

    conn_session->conn_handle = event->connect.conn_handle;

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
    /* Perform service discovery. */
    esp_ble_conn_disc_svcs(conn_session);
#endif

    esp_ble_conn_event_send(conn_session, ESP_BLE_CONN_EVENT_CONNECTED, NULL, 0, NULL);
    esp_event_post(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
}

static void esp_ble_conn_disconnect_cb(struct ble_gap_event *event, void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;

    /* Clear conn_handle value */
    conn_session->conn_handle = 0;

    esp_ble_conn_event_send(conn_session, ESP_BLE_CONN_EVENT_DISCONNECTED, NULL, 0, NULL);
    esp_event_post(BLE_CONN_MGR_EVENTS, ESP_BLE_CONN_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
}

static void esp_ble_conn_set_mtu_cb(struct ble_gap_event *event, void *arg)
{
    esp_ble_conn_session_t *conn_session = arg;

    conn_session->gatt_mtu = event->mtu.value;
}

static void esp_ble_conn_del_svc(esp_ble_conn_svc_t *svc)
{
    if (svc->nu_lookup) {
        for (uint8_t i = 0; i < svc->nu_lookup_count; i++) {
            if (svc->nu_lookup[i].name) {
                free((void *)svc->nu_lookup[i].name);
                svc->nu_lookup[i].name = NULL;
            }
        }

        free(svc->nu_lookup);
        svc->nu_lookup = NULL;
    }
}

esp_err_t esp_ble_conn_init(esp_ble_conn_config_t *config)
{
    if (!config || s_conn_session) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_ble_conn_session_t *conn_session = (esp_ble_conn_session_t *)calloc(1, sizeof(esp_ble_conn_session_t));
    if (!conn_session) {
        return ESP_ERR_NO_MEM;
    }

    conn_session->semaphore = xSemaphoreCreateBinary();
    if (!conn_session->semaphore) {
        ESP_LOGE(TAG, "Allocate memory for BLE semaphore error!");
        goto ble_init_error;
    }

    conn_session->queue = xQueueCreate(1, sizeof(esp_ble_conn_event_ctx_t));
    if (!conn_session->queue) {
        ESP_LOGE(TAG, "Allocate memory for BLE queue error!");
        goto ble_init_error;
    }

    /* Store BLE device name internally */
    conn_session->device_name = (uint8_t *)strndup((const char *)config->device_name, strlen((const char *)config->device_name));
    if (!conn_session->device_name) {
        ESP_LOGE(TAG, "Allocate memory for BLE device name error!");
        goto ble_init_error;
    }

    conn_session->gatt_mtu = GATT_DEF_BLE_MTU_SIZE;

#if defined(CONFIG_BLE_CONN_MGR_EXTENDED_ADV)
    if (config->extended_adv_data) {
        conn_session->adv_data_len = MIN(config->extended_adv_len, CONFIG_BT_NIMBLE_EXT_ADV_MAX_SIZE);
        conn_session->adv_data_buf = calloc(1, conn_session->adv_data_len);
        if (conn_session->adv_data_buf == NULL) {
            ESP_LOGE(TAG, "Allocate memory for extended advertisement data error!");
            goto ble_init_error;
        }
        memcpy(conn_session->adv_data_buf,  config->extended_adv_data, conn_session->adv_data_len);
    }

    if (config->extended_adv_rsp_data) {
        conn_session->adv_rsp_data_len = MIN(config->extended_adv_rsp_len, CONFIG_BT_NIMBLE_EXT_ADV_MAX_SIZE);
        conn_session->adv_rsp_data_buf = calloc(1, conn_session->adv_rsp_data_len);
        if (conn_session->adv_rsp_data_buf == NULL) {
            ESP_LOGE(TAG, "Allocate memory for extended advertisement response data error!");
            goto ble_init_error;
        }
        memcpy(conn_session->adv_rsp_data_buf,  config->extended_adv_rsp_data, conn_session->adv_rsp_data_len);
    }

#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV)
    if (config->periodic_adv_data) {
        conn_session->per_adv_data_len = MIN(config->periodic_adv_len, CONFIG_BT_NIMBLE_EXT_ADV_MAX_SIZE);
        conn_session->per_adv_data_buf = calloc(1, conn_session->per_adv_data_len);
        if (conn_session->per_adv_data_buf == NULL) {
            ESP_LOGE(TAG, "Allocate memory for periodic advertisement data error!");
            goto ble_init_error;
        }
        memcpy(conn_session->per_adv_data_buf,  config->periodic_adv_data, conn_session->per_adv_data_len);
    }
#endif

#else
    /* Store BLE announce data internally */
    esp_nimble_maps_t announce_data[] = {
        {
            .type = BLE_CONN_DATA_NAME_COMPLETE,
            .length = strlen((const char *)conn_session->device_name),
            .value = (uint8_t *)conn_session->device_name,
        }
    };

    for (uint8_t i = 0; i < (sizeof(announce_data)/sizeof(announce_data[0])); i++) {
        /* Add extra bytes required per entry, i.e.
         * length (1 byte) + type (1 byte) = 2 bytes */
        conn_session->adv_data_len += announce_data[i].length + 2;
    }

    if (conn_session->adv_data_len > BLE_ADV_DATA_LEN_MAX) {
        ESP_LOGE(TAG, "Advertisement data too long = %d bytes", conn_session->adv_data_len);
        goto ble_init_error;
    }

    /* Allocate memory for the raw advertisement data */
    conn_session->adv_data_buf = calloc(1, conn_session->adv_data_len);
    if (conn_session->adv_data_buf == NULL) {
        ESP_LOGE(TAG, "Allocate memory for raw advertisement data error!");
        goto ble_init_error;
    }

    /* Form the raw advertisement data using above entries */
    for (uint8_t i = 0, len = 0; i < (sizeof(announce_data)/sizeof(announce_data[0])); i++) {
        conn_session->adv_data_buf[len++] = announce_data[i].length + 1; // + 1 byte for type
        conn_session->adv_data_buf[len++] = announce_data[i].type;
        memcpy(&conn_session->adv_data_buf[len], announce_data[i].value, announce_data[i].length);
        len += announce_data[i].length;
    }

    /* Store BLE scan response data internally */
    esp_nimble_maps_t scan_resp_data[] = {
        {
            .type   = BLE_CONN_DATA_MANUFACTURER_DATA,
            .length = sizeof(config->broadcast_data),
            .value = config->broadcast_data,
        }
    };

    /* Get the total raw scan response data length required for above entries */
    for (int i = 0; i < (sizeof(scan_resp_data)/sizeof(scan_resp_data[0])); i++) {
        /* Add extra bytes required per entry, i.e.
         * length (1 byte) + type (1 byte) = 2 bytes */
        conn_session->adv_rsp_data_len += scan_resp_data[i].length + 2;
    }

    if (conn_session->adv_rsp_data_len > BLE_SCAN_RSP_DATA_LEN_MAX) {
        ESP_LOGE(TAG, "Scan response data too long = %d bytes", conn_session->adv_rsp_data_len);
        goto ble_init_error;
    }

    /* Allocate memory for the raw scan response data */
    conn_session->adv_rsp_data_buf = calloc(1, conn_session->adv_rsp_data_len);
    if (conn_session->adv_rsp_data_buf == NULL) {
        ESP_LOGE(TAG, "Allocate memory for raw response data!");
        goto ble_init_error;
    }

    /* Form the raw scan response data using above entries */
    for (uint8_t i = 0, len = 0; i < (sizeof(scan_resp_data)/sizeof(scan_resp_data[0])); i++) {
        conn_session->adv_rsp_data_buf[len++] = scan_resp_data[i].length + 1; // + 1 byte for type
        conn_session->adv_rsp_data_buf[len++] = scan_resp_data[i].type;
        memcpy(&conn_session->adv_rsp_data_buf[len], scan_resp_data[i].value, scan_resp_data[i].length);
        len += scan_resp_data[i].length;
    }
#endif

    SLIST_INIT(&conn_session->uuid_list);
    SLIST_INIT(&conn_session->mbuf_list);

    conn_session->connect_cb = esp_ble_conn_connect_cb;
    conn_session->disconnect_cb = esp_ble_conn_disconnect_cb;
    conn_session->set_mtu_cb = esp_ble_conn_set_mtu_cb;

    s_conn_session = conn_session;

    ESP_LOGI(TAG, "BLE Connection Management: v%d.%d.%d\n", BLE_CONN_MGR_VER_MAJOR, BLE_CONN_MGR_VER_MINOR, BLE_CONN_MGR_VER_PATCH);

    return ESP_OK;

ble_init_error:
    esp_ble_conn_deinit();
    return ESP_FAIL;
}

esp_err_t esp_ble_conn_deinit(void)
{
    svc_uuid_t *svc_uuid = NULL;
    attr_mbuf_t *attr_mbuf = NULL;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (!conn_session) {
        return ESP_ERR_INVALID_ARG;
    }

    if (conn_session->device_name) {
        free(conn_session->device_name);
        conn_session->device_name = NULL;
    }

    if (conn_session->adv_data_buf) {
        free(conn_session->adv_data_buf);
        conn_session->adv_data_buf = NULL;
    }

    if (conn_session->adv_rsp_data_buf) {
        free(conn_session->adv_rsp_data_buf);
        conn_session->adv_rsp_data_buf = NULL;
    }

    if (conn_session->semaphore) {
        vSemaphoreDelete(conn_session->semaphore);
        conn_session->semaphore = NULL;
    }

    if (conn_session->queue) {
        vQueueDelete(conn_session->queue);
        conn_session->queue = NULL;
    }

    if (conn_session->gatt_db) {
        struct ble_gatt_svc_def *gatt_svr_svcs = conn_session->gatt_db;
        for (int i = 0; i < conn_session->nu_lookup_count; i ++ ) {
            SLIST_FOREACH(svc_uuid, &conn_session->uuid_list, next) {
                ble_uuid_t *ble_uuid = NULL;
                ble_uuid_any_t uuid_any;
                switch (svc_uuid->svc.type) {
                    case BLE_CONN_UUID_TYPE_16:
                        ble_uuid_init_from_buf(&uuid_any, &svc_uuid->svc.uuid.uuid16, 2);
                        ble_uuid = (ble_uuid_t *)&uuid_any.u16;
                        break;
                    case BLE_CONN_UUID_TYPE_32:
                        ble_uuid_init_from_buf(&uuid_any, &svc_uuid->svc.uuid.uuid32, 4);
                        ble_uuid = (ble_uuid_t *)&uuid_any.u32;
                        break;
                    case BLE_CONN_UUID_TYPE_128:
                        ble_uuid_init_from_buf(&uuid_any, svc_uuid->svc.uuid.uuid128, BLE_UUID128_VAL_LEN);
                        ble_uuid = (ble_uuid_t *)&uuid_any.u128;
                        break;
                    default:
                        break;
                }

                if (!ble_uuid_cmp(gatt_svr_svcs[i].uuid, ble_uuid)) {
                    ble_gatt_svc_free(&gatt_svr_svcs[i], svc_uuid->svc.nu_lookup_count);
                    break;
                }
            }
        }
        free(conn_session->gatt_db);
        conn_session->gatt_db = NULL;
    }

    while (!SLIST_EMPTY(&conn_session->uuid_list)) {
        svc_uuid = SLIST_FIRST(&conn_session->uuid_list);
        SLIST_REMOVE_HEAD(&conn_session->uuid_list, next);
        esp_ble_conn_del_svc(&svc_uuid->svc);
#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL) || defined(CONFIG_BLE_CONN_MGR_ROLE_BOTH)
        esp_ble_conn_svc_uuid_del(svc_uuid);
#endif
        free(svc_uuid);
        svc_uuid = NULL;
    }

    while (!SLIST_EMPTY(&conn_session->mbuf_list)) {
        attr_mbuf = SLIST_FIRST(&conn_session->mbuf_list);
        SLIST_REMOVE_HEAD(&conn_session->mbuf_list, next);
        if (attr_mbuf->outbuf) {
            free(attr_mbuf->outbuf);
            attr_mbuf->outbuf = NULL;
        }
        free(attr_mbuf);
        attr_mbuf = NULL;
    }

    free(conn_session);
    conn_session = NULL;

    s_conn_session = conn_session;

    return ESP_OK;
}

esp_err_t esp_ble_conn_start(void)
{
    esp_err_t rc = ESP_OK;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (!conn_session) {
        return ESP_ERR_INVALID_ARG;
    }

    if (esp_ble_conn_populate_gatt_db(conn_session) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid GATT database count");
        return ESP_FAIL;
    }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
#endif
    nimble_port_init();

    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = esp_ble_conn_on_reset;
    ble_hs_cfg.sync_cb =  esp_ble_conn_on_sync;
    ble_hs_cfg.gatts_register_cb = esp_ble_conn_on_gatts_register;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Initialize security manager configuration in NimBLE host  */
#ifdef CONFIG_BLE_CONN_MGR_SM
    ble_hs_cfg.sm_io_cap = CONFIG_BLE_CONN_MGR_SM_IO_TYPE;;
    ble_hs_cfg.sm_oob_data_flag = (CONFIG_BLE_CONN_MGR_SM_CAP & BIT(0));
    ble_hs_cfg.sm_bonding = (CONFIG_BLE_CONN_MGR_SM_CAP & BIT(1));
    ble_hs_cfg.sm_mitm = (CONFIG_BLE_CONN_MGR_SM_CAP & BIT(2));
    ble_hs_cfg.sm_sc = (CONFIG_BLE_CONN_MGR_SM_CAP & BIT(3));
    ble_hs_cfg.sm_our_key_dist = CONFIG_BLE_CONN_MGR_SM_KEY_DIST;
    ble_hs_cfg.sm_their_key_dist = CONFIG_BLE_CONN_MGR_SM_KEY_DIST;
#else
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO; /* Just Works */
    ble_hs_cfg.sm_bonding = conn_session->ble_bonding;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = conn_session->ble_sm_sc;

    /* Distribute LTK and IRK */
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
#endif

    rc = esp_ble_conn_gatts_init(conn_session);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error init gatt service");
        return rc;
    }

    /* Set device name, configure response data to be sent while advertising */
    rc = ble_svc_gap_device_name_set((const char *)conn_session->device_name);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting device name");
        return rc;
    }

    /* XXX Need to have template for store */
    ble_store_config_init();
    nimble_port_freertos_init(esp_ble_conn_host_task);

    return ESP_OK;
}

esp_err_t esp_ble_conn_stop(void)
{
    esp_err_t ret = ESP_OK;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (conn_session != NULL ) {
#if defined(CONFIG_BLE_CONN_MGR_EXTENDED_ADV) || defined(CONFIG_BLE_CONN_MGR_PERIODIC_SYNC)
#if defined(CONFIG_BLE_CONN_MGR_EXTENDED_ADV)
#if defined(CONFIG_BLE_CONN_MGR_PERIODIC_ADV)
        ret = ble_gap_periodic_adv_stop(conn_session->conn_handle);
        if (ret) {
            ESP_LOGD(TAG, "Error in stopping periodic advertisement with err code = %d", ret);
        }
#endif

        ret = ble_gap_ext_adv_stop(conn_session->conn_handle);
        if (ret) {
            ESP_LOGD(TAG, "Error in stopping ext advertisement with err code = %d", ret);
        }
        ret = ble_gap_ext_adv_remove(conn_session->conn_handle);
        if (ret) {
            ESP_LOGD(TAG, "Error in remove ext advertisement with err code = %d", ret);
        }
#else
        ret = ble_gap_periodic_adv_sync_terminate(conn_session->conn_handle);
        if (ret) {
            ESP_LOGD(TAG, "Error in terminate synchronization procedure with err code = %d", ret);
        }
#endif
#else
        ret = ble_gap_adv_stop();
        if (ret) {
            ESP_LOGD(TAG, "Error in stopping advertisement with err code = %d", ret);
        }
#endif

        ret = nimble_port_stop();
        if (ret == ESP_OK) {
            nimble_port_deinit();
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
            esp_nimble_hci_and_controller_deinit();
#endif
        }

        return ret;
    }

    return ESP_ERR_INVALID_ARG;
}

esp_err_t esp_ble_conn_set_mtu(uint16_t mtu)
{
    esp_ble_conn_session_t  *conn_session = s_conn_session;
    if (!conn_session) {
        return ESP_ERR_INVALID_ARG;
    }

    conn_session->gatt_mtu = mtu;

    return ESP_OK;
}

esp_err_t esp_ble_conn_connect(void)
{
    esp_ble_conn_session_t  *conn_session = s_conn_session;
    esp_ble_conn_event_ctx_t event_ctx = {
        .event = ESP_BLE_CONN_EVENT_UNKNOWN
    };

    if (!conn_session) {
        return ESP_ERR_INVALID_ARG;
    }

    xQueueReceive(conn_session->queue, &event_ctx, pdMS_TO_TICKS(100));
    if (event_ctx.event != ESP_BLE_CONN_EVENT_CONNECTED) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t esp_ble_conn_disconnect(void)
{
    esp_ble_conn_session_t  *conn_session = s_conn_session;
    esp_ble_conn_event_ctx_t event_ctx = {
        .event = ESP_BLE_CONN_EVENT_UNKNOWN
    };

    if (!conn_session) {
        return ESP_ERR_INVALID_ARG;
    }

    xQueueReceive(conn_session->queue, &event_ctx, pdMS_TO_TICKS(100));
    if (event_ctx.event != ESP_BLE_CONN_EVENT_DISCONNECTED) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t esp_ble_conn_notify(const esp_ble_conn_data_t *inbuff)
{
    esp_err_t rc = ESP_OK;
    attr_mbuf_t *attr_mbuf = NULL;
    struct os_mbuf *om = NULL;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (!conn_session) {
        return ESP_ERR_INVALID_ARG;
    }

    attr_mbuf = esp_ble_conn_find_attr_with_uuid(inbuff->type, inbuff->uuid);
    if (!attr_mbuf) {
        return ESP_ERR_INVALID_ARG;
    }

    om = ble_hs_mbuf_from_flat(inbuff->data, inbuff->data_len);
    rc = ble_gattc_notify_custom(conn_session->conn_handle, attr_mbuf->attr_handle, om);
    if (!rc) {
        ESP_LOGD(TAG, "Notify sent, attr_handle = %d", attr_mbuf->attr_handle);
        return (xSemaphoreTake(conn_session->semaphore, pdMS_TO_TICKS(CONFIG_BLE_CONN_MGR_WAIT_DURATION * 1000)) != pdPASS) ? ESP_ERR_TIMEOUT : ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error in sending notify, rc = %d", rc);
    }

    return rc;
}

esp_err_t esp_ble_conn_read(esp_ble_conn_data_t *inbuff)
{
    esp_err_t rc = ESP_OK;

    ble_uuid_t *ble_uuid = NULL;
    ble_uuid_any_t uuid_any;
    char buf[BLE_UUID_STR_LEN];

    attr_mbuf_t *attr_mbuf = NULL;
    esp_ble_conn_session_t  *conn_session = s_conn_session;

    if (!conn_session || !inbuff) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (inbuff->type) {
        case BLE_CONN_UUID_TYPE_16:
            ble_uuid_init_from_buf(&uuid_any, &inbuff->uuid.uuid16, 2);
            ble_uuid = (ble_uuid_t *)&uuid_any.u16;
            break;
        case BLE_CONN_UUID_TYPE_32:
            ble_uuid_init_from_buf(&uuid_any, &inbuff->uuid.uuid32, 4);
            ble_uuid = (ble_uuid_t *)&uuid_any.u32;
            break;
        case BLE_CONN_UUID_TYPE_128:
            ble_uuid_init_from_buf(&uuid_any, inbuff->uuid.uuid128, BLE_UUID128_VAL_LEN);
            ble_uuid = (ble_uuid_t *)&uuid_any.u128;
            break;
        default:
            break;
    }

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL)
    svc_uuid_t *svc = NULL;
    chr_uuid_t *chr = NULL;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        chr = esp_ble_conn_chr_find_uuid(conn_session, &svc->gatt_svc.uuid.u, ble_uuid);
        if (chr) {
            break;
        }
    }

    if (!chr) {
        ESP_LOGE(TAG, "Incorrect uuid %s in read", ble_uuid_to_str(ble_uuid, buf));
        return ESP_ERR_INVALID_ARG;
    }

    rc = ble_gattc_read(conn_session->conn_handle, chr->chr.val_handle, esp_ble_conn_on_read, conn_session);
    if (rc) {
        ESP_LOGE(TAG, "Error in reading data, rc = %d", rc);
        return rc;
    }

    if (xSemaphoreTake(conn_session->semaphore, pdMS_TO_TICKS(CONFIG_BLE_CONN_MGR_WAIT_DURATION * 1000)) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }
#endif
    attr_mbuf = esp_ble_conn_find_attr_with_uuid(inbuff->type, inbuff->uuid);
    if (!attr_mbuf) {
        ESP_LOGE(TAG, "Incorrect uuid %s in read", ble_uuid_to_str(ble_uuid, buf));
        return ESP_ERR_INVALID_ARG;
    }

    if (!inbuff->data) {
        inbuff->data = attr_mbuf->outbuf;
        inbuff->data_len = attr_mbuf->outlen;
    } else {
        inbuff->data_len = MIN(attr_mbuf->outlen, inbuff->data_len);
        memcpy(inbuff->data, attr_mbuf->outbuf, inbuff->data_len);
    }

    return rc;
}

esp_err_t esp_ble_conn_write(const esp_ble_conn_data_t *inbuff)
{
    esp_err_t rc = ESP_OK;

    ble_uuid_t *ble_uuid = NULL;
    ble_uuid_any_t uuid_any;
    char buf[BLE_UUID_STR_LEN];
    uint16_t chr_val_handle = 0;
    struct os_mbuf *om = NULL;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (!conn_session || !inbuff) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (inbuff->type) {
        case BLE_CONN_UUID_TYPE_16:
            ble_uuid_init_from_buf(&uuid_any, &inbuff->uuid.uuid16, 2);
            ble_uuid = (ble_uuid_t *)&uuid_any.u16;
            break;
        case BLE_CONN_UUID_TYPE_32:
            ble_uuid_init_from_buf(&uuid_any, &inbuff->uuid.uuid32, 4);
            ble_uuid = (ble_uuid_t *)&uuid_any.u32;
            break;
        case BLE_CONN_UUID_TYPE_128:
            ble_uuid_init_from_buf(&uuid_any, inbuff->uuid.uuid128, BLE_UUID128_VAL_LEN);
            ble_uuid = (ble_uuid_t *)&uuid_any.u128;
            break;
        default:
            break;
    }

    om = ble_hs_mbuf_from_flat(inbuff->data, inbuff->data_len);

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL)
    svc_uuid_t *svc = NULL;
    chr_uuid_t *chr = NULL;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        chr = esp_ble_conn_chr_find_uuid(conn_session, &svc->gatt_svc.uuid.u, ble_uuid);
        if (chr) {
            break;
        }
    }

    if (!chr) {
        ESP_LOGE(TAG, "Incorrect uuid %s in sending", ble_uuid_to_str(ble_uuid, buf));
        return ESP_ERR_INVALID_ARG;
    }
    chr_val_handle = chr->chr.val_handle;
    rc = ble_gattc_write(conn_session->conn_handle, chr_val_handle, om, esp_ble_conn_on_write, conn_session);
#else
    attr_mbuf_t *attr_mbuf = esp_ble_conn_find_attr_with_uuid(inbuff->type, inbuff->uuid);
    if (!attr_mbuf) {
        ESP_LOGE(TAG, "Incorrect uuid %s in sending", ble_uuid_to_str(ble_uuid, buf));
        return ESP_ERR_INVALID_ARG;
    }
    chr_val_handle = attr_mbuf->attr_handle;
    rc = ble_gattc_indicate_custom(conn_session->conn_handle, chr_val_handle, om);
#endif

    if (!rc) {
        ESP_LOGD(TAG, "Sent data, attr_handle = %d", chr_val_handle);
        return (xSemaphoreTake(conn_session->semaphore, pdMS_TO_TICKS(CONFIG_BLE_CONN_MGR_WAIT_DURATION * 1000)) != pdPASS) ? ESP_ERR_TIMEOUT : ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error in sending data, rc = %d", rc);
    }

    return rc;
}

esp_err_t esp_ble_conn_subscribe(esp_ble_conn_desc_t desc, const esp_ble_conn_data_t *inbuff)
{
    esp_err_t rc = ESP_OK;

    ble_uuid_t *ble_uuid = NULL;
    ble_uuid_any_t uuid_any;
    char buf[BLE_UUID_STR_LEN];
    uint16_t handle = 0;
    struct os_mbuf *om = NULL;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (!conn_session || !inbuff) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (inbuff->type) {
        case BLE_CONN_UUID_TYPE_16:
            ble_uuid_init_from_buf(&uuid_any, &inbuff->uuid.uuid16, 2);
            ble_uuid = (ble_uuid_t *)&uuid_any.u16;
            break;
        case BLE_CONN_UUID_TYPE_32:
            ble_uuid_init_from_buf(&uuid_any, &inbuff->uuid.uuid32, 4);
            ble_uuid = (ble_uuid_t *)&uuid_any.u32;
            break;
        case BLE_CONN_UUID_TYPE_128:
            ble_uuid_init_from_buf(&uuid_any, inbuff->uuid.uuid128, BLE_UUID128_VAL_LEN);
            ble_uuid = (ble_uuid_t *)&uuid_any.u128;
            break;
        default:
            break;
    }

    om = ble_hs_mbuf_from_flat(inbuff->data, inbuff->data_len);

#if defined(CONFIG_BLE_CONN_MGR_ROLE_CENTRAL)
    svc_uuid_t *svc = NULL;
    dsc_uuid_t *dsc = NULL;

    SLIST_FOREACH(svc, &conn_session->uuid_list, next) {
        dsc = esp_ble_conn_dsc_find_uuid(conn_session, &svc->gatt_svc.uuid.u, ble_uuid, BLE_UUID16_DECLARE(desc));
        if (dsc) {
            break;
        }
    }

    if (!dsc) {
        ESP_LOGE(TAG, "Incorrect uuid %s in descriptors", ble_uuid_to_str(ble_uuid, buf));
        return ESP_ERR_INVALID_ARG;
    }

    handle = dsc->dsc.handle;
    rc = ble_gattc_write(conn_session->conn_handle, dsc->dsc.handle, om, esp_ble_conn_on_write, conn_session);
#else
    attr_mbuf_t *attr_mbuf = esp_ble_conn_find_attr_with_uuid(inbuff->type, inbuff->uuid);
    if (!attr_mbuf) {
        ESP_LOGE(TAG, "Incorrect uuid %s in descriptors", ble_uuid_to_str(ble_uuid, buf));
        return ESP_ERR_INVALID_ARG;
    }
    handle = attr_mbuf->attr_handle;
    rc = ble_gattc_indicate_custom(conn_session->conn_handle, handle, om);
#endif

    if (!rc) {
        ESP_LOGD(TAG, "Descriptors data, attr_handle = %d", handle);
        return (xSemaphoreTake(conn_session->semaphore, pdMS_TO_TICKS(CONFIG_BLE_CONN_MGR_WAIT_DURATION * 1000)) != pdPASS) ? ESP_ERR_TIMEOUT : ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error in descriptors data, rc = %d", rc);
    }

    return rc;
}

esp_err_t esp_ble_conn_add_svc(const esp_ble_conn_svc_t *svc)
{
    svc_uuid_t *svc_uuid = NULL;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (!conn_session || !svc || BLE_UUID_TYPE(svc->type)) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < svc->nu_lookup_count; i ++) {
        if (BLE_UUID_TYPE(svc->nu_lookup[i].type)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    svc_uuid = calloc(1, sizeof(svc_uuid_t));
    if (!svc_uuid) {
        return ESP_ERR_NO_MEM;
    }

    svc_uuid->svc.nu_lookup = calloc(svc->nu_lookup_count, sizeof(esp_ble_conn_character_t));
    if (!svc_uuid->svc.nu_lookup) {
        free(svc_uuid);
        return ESP_ERR_NO_MEM;
    }

    svc_uuid->svc.type = svc->type;
    svc_uuid->svc.nu_lookup_count = svc->nu_lookup_count;
    memcpy(&svc_uuid->svc.uuid, &svc->uuid, sizeof(svc->uuid));
    for (uint8_t i = 0; i < svc->nu_lookup_count; i ++) {
        svc_uuid->svc.nu_lookup[i].name = strdup(svc->nu_lookup[i].name);
        svc_uuid->svc.nu_lookup[i].type = svc->nu_lookup[i].type;
        memcpy(&svc_uuid->svc.nu_lookup[i].uuid, &svc->nu_lookup[i].uuid, sizeof(svc->nu_lookup[i].uuid));
        svc_uuid->svc.nu_lookup[i].flag = svc->nu_lookup[i].flag;
        svc_uuid->svc.nu_lookup[i].uuid_fn = svc->nu_lookup[i].uuid_fn;
    }
    SLIST_INSERT_HEAD(&conn_session->uuid_list, svc_uuid, next);

    conn_session->nu_lookup_count ++;

    return ESP_OK;
}

esp_err_t esp_ble_conn_remove_svc(const esp_ble_conn_svc_t *svc)
{
    svc_uuid_t *svc_uuid = NULL;
    esp_ble_conn_session_t *conn_session = s_conn_session;

    if (!conn_session || !svc || BLE_UUID_TYPE(svc->type)) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < svc->nu_lookup_count; i ++) {
        if (BLE_UUID_TYPE(svc->nu_lookup[i].type)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    while (!SLIST_EMPTY(&conn_session->uuid_list)) {
        svc_uuid = SLIST_FIRST(&conn_session->uuid_list);
        if (BLE_UUID_CMP(svc->type, svc_uuid->svc.uuid, svc->uuid)) {
            SLIST_REMOVE_HEAD(&conn_session->uuid_list, next);
            esp_ble_conn_del_svc(&svc_uuid->svc);
            free(svc_uuid);

            conn_session->nu_lookup_count --;
            break;
        }
    }

    return ESP_OK;
}
