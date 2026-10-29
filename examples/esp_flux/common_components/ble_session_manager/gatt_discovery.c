/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gatt_discovery.h"

static const char *TAG = "gatt_discovery";

gatt_discovery_t *gatt_discovery_start(uint16_t conn_handle, const ble_discovery_callbacks_t *callbacks)
{
    if (conn_handle == 0xFFFF) {
        return NULL;
    }

    gatt_discovery_t *discovery = calloc(1, sizeof(gatt_discovery_t));
    if (!discovery) {
        ESP_LOGE(TAG, "Failed to allocate discovery");
        return NULL;
    }

    discovery->conn_handle = conn_handle;
    discovery->state = GATT_DISC_STATE_IDLE;

    // Initialize service list
    SLIST_INIT(&discovery->services);

    if (callbacks) {
        memcpy(&discovery->callbacks, callbacks, sizeof(ble_discovery_callbacks_t));
        ESP_LOGI(TAG, "GATT discovery callbacks set");
    }

    ESP_LOGI(TAG, "Discovery created for conn_handle=%d", conn_handle);
    return discovery;
}

// Clean up descriptor list
static void gatt_discovery_cleanup_descriptors(gatt_characteristic_t *chr)
{
    gatt_descriptor_t *dsc;
    while ((dsc = SLIST_FIRST(&chr->dscs)) != NULL) {
        SLIST_REMOVE_HEAD(&chr->dscs, next);
        free(dsc);
    }
}

// Clean up characteristic list
static void gatt_discovery_cleanup_characteristics(gatt_service_t *svc)
{
    gatt_characteristic_t *chr;
    while ((chr = SLIST_FIRST(&svc->chrs)) != NULL) {
        SLIST_REMOVE_HEAD(&svc->chrs, next);
        // Clean up descriptors under characteristic
        gatt_discovery_cleanup_descriptors(chr);
        free(chr);
    }
}

// Clean up service list
static void gatt_discovery_cleanup_services(gatt_discovery_t *discovery)
{
    gatt_service_t *svc;
    while ((svc = SLIST_FIRST(&discovery->services)) != NULL) {
        SLIST_REMOVE_HEAD(&discovery->services, next);
        // Clean up characteristics and descriptors under service
        gatt_discovery_cleanup_characteristics(svc);
        free(svc);
    }
}

esp_err_t gatt_discovery_destroy(gatt_discovery_t *discovery)
{
    if (!discovery) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clean up all services, characteristics and descriptors
    gatt_discovery_cleanup_services(discovery);

    free(discovery);
    discovery = NULL;
    ESP_LOGI(TAG, "GATT discovery destroyed");

    return ESP_OK;
}

int gatt_discovery_service_is_empty(const gatt_service_t *svc)
{
    return svc->svc.end_handle <= svc->svc.start_handle;
}

uint16_t gatt_discovery_characteristic_end_handle(const gatt_service_t *svc, const gatt_characteristic_t *chr)
{
    const gatt_characteristic_t *next_chr;

    next_chr = SLIST_NEXT(chr, next);
    if (next_chr != NULL) {
        return next_chr->chr.def_handle - 1;
    } else {
        return svc->svc.end_handle;
    }
}

int gatt_discovery_characteristic_is_empty(const gatt_service_t *svc, const gatt_characteristic_t *chr)
{
    return gatt_discovery_characteristic_end_handle(svc, chr) <= chr->chr.val_handle;
}

static void gatt_discovery_complete(gatt_discovery_t *discovery, int rc)
{
    discovery->disc_prev_chr_val = 0;

    /* Notify caller that discovery has completed. */
    if (discovery->callbacks.discovery_complete_cb != NULL) {
        discovery->callbacks.discovery_complete_cb(discovery, rc, discovery->callbacks.arg);
    }
}

static gatt_service_t *gatt_discovery_service_find_range(gatt_discovery_t *discovery, uint16_t attr_handle)
{
    gatt_service_t *svc;

    SLIST_FOREACH(svc, &discovery->services, next) {
        if (svc->svc.start_handle <= attr_handle && svc->svc.end_handle >= attr_handle) {
            return svc;
        }
    }

    return NULL;
}

static gatt_service_t *gatt_discovery_find_service_prev(gatt_discovery_t *discovery, uint16_t service_start_handle)
{
    gatt_service_t *prev;
    gatt_service_t *svc;

    prev = NULL;
    SLIST_FOREACH(svc, &discovery->services, next) {
        if (svc->svc.start_handle >= service_start_handle) {
            break;
        }

        prev = svc;
    }

    return prev;
}

static gatt_service_t *gatt_discovery_find_service(gatt_discovery_t *discovery, uint16_t service_start_handle, gatt_service_t **out_prev)
{
    gatt_service_t *prev;
    gatt_service_t *svc;

    prev = gatt_discovery_find_service_prev(discovery, service_start_handle);
    if (prev == NULL) {
        svc = SLIST_FIRST(&discovery->services);
    } else {
        svc = SLIST_NEXT(prev, next);
    }

    if (svc != NULL && svc->svc.start_handle != service_start_handle) {
        svc = NULL;
    }

    if (out_prev != NULL) {
        *out_prev = prev;
    }
    return svc;
}

static int gatt_discovery_add_service(gatt_discovery_t *discovery, const struct ble_gatt_svc *gatt_svc)
{
    gatt_service_t *prev;
    gatt_service_t *svc;

    svc = gatt_discovery_find_service(discovery, gatt_svc->start_handle, &prev);
    if (svc != NULL) {
        /* Service already discovered. */
        return 0;
    }

    svc = calloc(1, sizeof(gatt_service_t));
    if (!svc) {
        ESP_LOGE(TAG, "Failed to allocate service");
        return BLE_HS_ENOMEM;
    }

    svc->svc = *gatt_svc;
    SLIST_INIT(&svc->chrs);
#if (MYNEWT_VAL(BLE_INCL_SVC_DISCOVERY) || MYNEWT_VAL(BLE_GATT_CACHING_INCLUDE_SERVICES))
    SLIST_INIT(&svc->incl_svc);
#endif

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&discovery->services, svc, next);
    } else {
        SLIST_INSERT_AFTER(prev, svc, next);
    }

    return 0;
}

static int gatt_discovery_service_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg)
{
    int rc;
    gatt_discovery_t *discovery = (gatt_discovery_t *)arg;
    if (conn_handle != discovery->conn_handle) {
        return 0;
    }

    switch (error->status) {
    case 0:
        rc = gatt_discovery_add_service(discovery, service);

        char buf[BLE_UUID_STR_LEN];
        ESP_LOGI(TAG, "Found service: %s, handle: 0x%04X-0x%04X", ble_uuid_to_str(&service->uuid.u, buf), service->start_handle, service->end_handle);

        if (service) {
            if (discovery->callbacks.service_found_cb) {
                discovery->callbacks.service_found_cb(discovery, conn_handle, service, discovery->callbacks.arg);
            }
        }
        break;

    case BLE_HS_EDONE:
        ESP_LOGI(TAG, "All services discovered");
        /* All services discovered; start discovering incs.*/
#if (MYNEWT_VAL(BLE_INCL_SVC_DISCOVERY) || MYNEWT_VAL(BLE_GATT_CACHING_INCLUDE_SERVICES))
        discovery->cur_svc = NULL;
        peer_disc_incs(peer);
#else
        /* All services discovered; start discovering characteristics. */
        if (discovery->disc_prev_chr_val > 0) {
            gatt_discovery_discover_all_characteristics(discovery);
        }
#endif
        rc = 0;
        break;

    default:
        rc = error->status;
        break;
    }

    if (rc != 0) {
        /* Error; abort discovery. */
        ESP_LOGE(TAG, "Service discovery failed: %d", rc);
        gatt_discovery_complete(discovery, rc);
    }

    return 0;
}

esp_err_t gatt_discovery_discover_all_services(gatt_discovery_t *discovery)
{
    if (!discovery) {
        return ESP_ERR_INVALID_ARG;
    }

    if (discovery->state != GATT_DISC_STATE_IDLE) {
        ESP_LOGW(TAG, "Discovery already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    discovery->disc_prev_chr_val = 1;

    int rc = ble_gattc_disc_all_svcs(discovery->conn_handle, gatt_discovery_service_cb, discovery);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start service discovery: %d", rc);
        return ESP_FAIL;
    }

    discovery->state = GATT_DISC_STATE_DISCOVERING_SERVICES;
    ESP_LOGI(TAG, "Service discovery started");
    return ESP_OK;
}

esp_err_t gatt_discovery_discover_service_by_uuid(gatt_discovery_t *discovery, const ble_uuid_t *uuid)
{
    if (!discovery || !uuid) {
        return ESP_ERR_INVALID_ARG;
    }

    if (discovery->state != GATT_DISC_STATE_IDLE) {
        ESP_LOGW(TAG, "Discovery already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    discovery->disc_prev_chr_val = 1;

    int rc = ble_gattc_disc_svc_by_uuid(discovery->conn_handle, uuid, gatt_discovery_service_cb, discovery);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start service discovery by UUID: %d", rc);
        return ESP_FAIL;
    }

    discovery->state = GATT_DISC_STATE_DISCOVERING_SERVICES;
    ESP_LOGI(TAG, "Service discovery by UUID type %d started", uuid->type);
    return ESP_OK;
}

static gatt_characteristic_t *gatt_discovery_find_characteristic_prev(const gatt_service_t *svc, uint16_t chr_val_handle)
{
    gatt_characteristic_t *prev;
    gatt_characteristic_t *chr;

    prev = NULL;
    SLIST_FOREACH(chr, &svc->chrs, next) {
        if (chr->chr.val_handle >= chr_val_handle) {
            break;
        }

        prev = chr;
    }

    return prev;
}

static gatt_characteristic_t *gatt_discovery_find_characteristic(const gatt_service_t *svc, uint16_t chr_val_handle, gatt_characteristic_t **out_prev)
{
    gatt_characteristic_t *prev;
    gatt_characteristic_t *chr;

    prev = gatt_discovery_find_characteristic_prev(svc, chr_val_handle);
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

static int gatt_discovery_add_characteristic(gatt_discovery_t *discovery, uint16_t svc_start_handle, const struct ble_gatt_chr *gatt_chr)
{
    gatt_service_t *svc;
    gatt_characteristic_t *prev;
    gatt_characteristic_t *chr;

    svc = gatt_discovery_find_service(discovery, svc_start_handle, NULL);
    if (svc == NULL) {
        /* Can't find service for discovered characteristic; this shouldn't
         * happen.
         */
        assert(0);
        return BLE_HS_EUNKNOWN;
    }

    chr = gatt_discovery_find_characteristic(svc, gatt_chr->def_handle, &prev);
    if (chr != NULL) {
        /* Characteristic already discovered. */
        return 0;
    }

    chr = calloc(1, sizeof(gatt_characteristic_t));
    if (!chr) {
        ESP_LOGE(TAG, "Failed to allocate characteristic");
        return BLE_HS_ENOMEM;
    }
    chr->chr = *gatt_chr;

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&svc->chrs, chr, next);
    } else {
        SLIST_NEXT(prev, next) = chr;
    }

    return 0;
}

static int gatt_discovery_characteristic_cb(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg)
{
    int rc;
    gatt_discovery_t *discovery = (gatt_discovery_t *)arg;
    if (conn_handle != discovery->conn_handle) {
        return 0;
    }

    switch (error->status) {
    case 0:
        rc = gatt_discovery_add_characteristic(discovery, discovery->cur_service->svc.start_handle, chr);

        char buf[BLE_UUID_STR_LEN];
        ESP_LOGI(TAG, "Found characteristics: %s, handle: 0x%04X-0x%04X", ble_uuid_to_str(&chr->uuid.u, buf), chr->def_handle, chr->val_handle);

        if (chr) {
            if (discovery->callbacks.characteristic_found_cb) {
                discovery->callbacks.characteristic_found_cb(discovery, conn_handle, chr, discovery->callbacks.arg);
            }
        }
        break;

    case BLE_HS_EDONE:
        ESP_LOGI(TAG, "Discovery characteristics completed, service handle: 0x%04X", discovery->cur_service->svc.start_handle);
        /* All characteristics in this service discovered; start discovering
         * characteristics in the next service.
         */
        if (discovery->disc_prev_chr_val > 0) {
            gatt_discovery_discover_all_characteristics(discovery);
        }
        rc = 0;
        break;

    default:
        rc = error->status;
        break;
    }

    if (rc != 0) {
        /* Error; abort discovery. */
        ESP_LOGE(TAG, "Characteristic discovery failed: %d", rc);
        gatt_discovery_complete(discovery, rc);
    }

    return rc;
}

esp_err_t gatt_discovery_discover_all_characteristics(gatt_discovery_t *discovery)
{
    int rc;
    gatt_service_t *svc;

    /* Search through the list of discovered service for the first service that
     * contains undiscovered characteristics.  Then, discover all
     * characteristics belonging to that service.
     */
    SLIST_FOREACH(svc, &discovery->services, next) {
        if (!gatt_discovery_service_is_empty(svc) && SLIST_EMPTY(&svc->chrs)) {
            discovery->cur_service = svc;
            rc = ble_gattc_disc_all_chrs(discovery->conn_handle, svc->svc.start_handle,
                                         svc->svc.end_handle, gatt_discovery_characteristic_cb, discovery);
            if (rc != 0) {
                ESP_LOGE(TAG, "Characteristic discovery all failed: %d", rc);
                gatt_discovery_complete(discovery, rc);
            }
            return ESP_OK;
        }
    }

    /* All characteristics discovered. */
    gatt_discovery_discover_all_characteristic_descriptors(discovery);

    ESP_LOGI(TAG, "All characteristics discovered");

    return ESP_OK;
}

static gatt_descriptor_t *gatt_discovery_find_descriptor_prev(const gatt_characteristic_t *chr, uint16_t dsc_handle)
{
    gatt_descriptor_t *prev;
    gatt_descriptor_t *dsc;

    prev = NULL;
    SLIST_FOREACH(dsc, &chr->dscs, next) {
        if (dsc->dsc.handle >= dsc_handle) {
            break;
        }

        prev = dsc;
    }

    return prev;
}

static gatt_descriptor_t *gatt_discovery_find_descriptor(const gatt_characteristic_t *chr, uint16_t dsc_handle, gatt_descriptor_t **out_prev)
{
    gatt_descriptor_t *prev;
    gatt_descriptor_t *dsc;

    prev = gatt_discovery_find_descriptor_prev(chr, dsc_handle);
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

static int gatt_discovery_add_descriptor(gatt_discovery_t *discovery, uint16_t chr_val_handle, const struct ble_gatt_dsc *gatt_dsc)
{
    gatt_service_t *svc;
    gatt_characteristic_t *chr;
    gatt_descriptor_t *prev;
    gatt_descriptor_t *dsc;

    svc = gatt_discovery_service_find_range(discovery, chr_val_handle);
    if (svc == NULL) {
        /* Can't find service for discovered descriptor; this shouldn't
         * happen.
         */
        assert(0);
        return BLE_HS_EUNKNOWN;
    }

    chr = gatt_discovery_find_characteristic(svc, chr_val_handle, NULL);
    if (chr == NULL) {
        /* Can't find characteristic for discovered descriptor; this shouldn't
         * happen.
         */
        assert(0);
        return BLE_HS_EUNKNOWN;
    }

    dsc = gatt_discovery_find_descriptor(chr, gatt_dsc->handle, &prev);
    if (dsc != NULL) {
        /* Descriptor already discovered. */
        return 0;
    }

    dsc = calloc(1, sizeof(gatt_descriptor_t));
    if (!dsc) {
        ESP_LOGE(TAG, "Failed to allocate descriptor");
        return BLE_HS_ENOMEM;
    }
    dsc->dsc = *gatt_dsc;

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&chr->dscs, dsc, next);
    } else {
        SLIST_NEXT(prev, next) = dsc;
    }

    return 0;
}

static int gatt_discovery_descriptor_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                                        uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    int rc;
    gatt_discovery_t *discovery = (gatt_discovery_t *)arg;
    if (conn_handle != discovery->conn_handle) {
        return 0;
    }

    switch (error->status) {
    case 0:
        rc = gatt_discovery_add_descriptor(discovery, chr_val_handle, dsc);

        char buf[BLE_UUID_STR_LEN];
        ESP_LOGI(TAG, "Found characteristics descriptors: %s, handle: 0x%04X", ble_uuid_to_str(&dsc->uuid.u, buf), dsc->handle);

        if (dsc) {
            if (discovery->callbacks.descriptor_found_cb) {
                discovery->callbacks.descriptor_found_cb(discovery, conn_handle, dsc, discovery->callbacks.arg);
            }
        }
        break;

    case BLE_HS_EDONE:
        ESP_LOGI(TAG, "Discovery descriptors completed, characteristic handle: 0x%04X", chr_val_handle);
        /* All descriptors in this characteristic discovered; start discovering
         * descriptors in the next characteristic.
         */
        if (discovery->disc_prev_chr_val > 0) {
            gatt_discovery_discover_all_characteristic_descriptors(discovery);
        }
        rc = 0;
        break;

    default:
        /* Error; abort discovery. */
        rc = error->status;
        break;
    }

    if (rc != 0) {
        /* Error; abort discovery. */
        ESP_LOGE(TAG, "Descriptor discovery failed: %d", rc);
        gatt_discovery_complete(discovery, rc);
    }

    return rc;
}

esp_err_t gatt_discovery_discover_all_characteristic_descriptors(gatt_discovery_t *discovery)
{
    int rc;
    gatt_service_t *svc;
    gatt_characteristic_t *chr;

    /* Search through the list of discovered characteristics for the first
     * characteristic that contains undiscovered descriptors.  Then, discover
     * all descriptors belonging to that characteristic.
     */
    SLIST_FOREACH(svc, &discovery->services, next) {
        SLIST_FOREACH(chr, &svc->chrs, next) {
            if (!gatt_discovery_characteristic_is_empty(svc, chr) && SLIST_EMPTY(&chr->dscs) &&
                    discovery->disc_prev_chr_val <= chr->chr.def_handle) {

                rc = ble_gattc_disc_all_dscs(discovery->conn_handle, chr->chr.val_handle,
                                             gatt_discovery_characteristic_end_handle(svc, chr), gatt_discovery_descriptor_cb, discovery);
                if (rc != 0) {
                    ESP_LOGE(TAG, "Descriptor discovery all failed: %d", rc);
                    gatt_discovery_complete(discovery, rc);
                }

                discovery->disc_prev_chr_val = chr->chr.val_handle;
                return ESP_OK;
            }
        }
    }

    /* All descriptors discovered. */
    gatt_discovery_complete(discovery, 0);

    ESP_LOGI(TAG, "All descriptors discovered");
    return ESP_OK;
}

gatt_service_t *gatt_discovery_find_service_by_uuid(gatt_discovery_t *discovery, const ble_uuid_t *uuid)
{
    if (!discovery || !uuid) {
        return NULL;
    }

    gatt_service_t *svc;

    SLIST_FOREACH(svc, &discovery->services, next) {
        if (ble_uuid_cmp(&svc->svc.uuid.u, uuid) == 0) {
            return svc;
        }
    }

    return NULL;
}

gatt_characteristic_t *gatt_discovery_find_characteristic_by_uuid(gatt_discovery_t *discovery,
                                                                  const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid)
{
    if (!discovery || !svc_uuid || !chr_uuid) {
        return NULL;
    }

    gatt_service_t *svc;
    gatt_characteristic_t *chr;

    svc = gatt_discovery_find_service_by_uuid(discovery, svc_uuid);
    if (svc == NULL) {
        return NULL;
    }

    SLIST_FOREACH(chr, &svc->chrs, next) {
        if (ble_uuid_cmp(&chr->chr.uuid.u, chr_uuid) == 0) {
            char buf[BLE_UUID_STR_LEN];
            ESP_LOGI(TAG, "Found characteristic by UUID %s", ble_uuid_to_str(&chr->chr.uuid.u, buf));
            return chr;
        }
    }

    return NULL;
}

gatt_descriptor_t *gatt_discovery_find_descriptor_by_uuid(gatt_discovery_t *discovery,
                                                          const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid, const ble_uuid_t *dsc_uuid)
{
    if (!discovery || !svc_uuid || !chr_uuid || !dsc_uuid) {
        return NULL;
    }

    gatt_characteristic_t *chr;
    gatt_descriptor_t *dsc;

    chr = gatt_discovery_find_characteristic_by_uuid(discovery, svc_uuid, chr_uuid);
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

uint16_t gatt_discovery_get_service_count(gatt_discovery_t *discovery)
{
    if (!discovery) {
        return 0;
    }

    uint16_t count = 0;
    gatt_service_t *svc;

    SLIST_FOREACH(svc, &discovery->services, next) {
        count++;
    }

    return count;
}

uint16_t gatt_discovery_get_characteristic_count(gatt_discovery_t *discovery, const gatt_service_t *svc)
{
    if (!discovery || !svc) {
        return 0;
    }

    uint16_t count = 0;
    gatt_characteristic_t *chr;

    SLIST_FOREACH(chr, &svc->chrs, next) {
        count++;
    }

    return count;
}

uint16_t gatt_discovery_get_descriptor_count(gatt_discovery_t *discovery, const gatt_characteristic_t *chr)
{
    if (!discovery || !chr) {
        return 0;
    }

    uint16_t count = 0;
    gatt_descriptor_t *dsc;

    SLIST_FOREACH(dsc, &chr->dscs, next) {
        count++;
    }

    return count;
}
