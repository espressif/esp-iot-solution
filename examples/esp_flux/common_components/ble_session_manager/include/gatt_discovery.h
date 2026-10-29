/* SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GATT_DISCOVERY_H
#define GATT_DISCOVERY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GATT_DISCOVERY_MAX_SERVICES 16
#define GATT_DISCOVERY_MAX_CHARACTERISTICS 64
#define GATT_DISCOVERY_MAX_DESCRIPTORS 128
#define GATT_UUID_LEN 16

typedef struct gatt_discovery gatt_discovery_t;
typedef struct gatt_service gatt_service_t;
typedef struct gatt_characteristic gatt_characteristic_t;
typedef struct gatt_descriptor gatt_descriptor_t;

typedef void (*discovery_complete_cb_t)(gatt_discovery_t *discovery, int status, void *arg);
typedef void (*service_found_cb_t)(gatt_discovery_t *discovery, uint16_t conn_handle, const struct ble_gatt_svc *svc, void *arg);
typedef void (*characteristic_found_cb_t)(gatt_discovery_t *discovery, uint16_t conn_handle, const struct ble_gatt_chr *chr, void *arg);
typedef void (*descriptor_found_cb_t)(gatt_discovery_t *discovery, uint16_t conn_handle, const struct ble_gatt_dsc *dsc, void *arg);

typedef enum {
    GATT_DISC_STATE_IDLE,
    GATT_DISC_STATE_DISCOVERING_SERVICES,
    GATT_DISC_STATE_DISCOVERING_CHARACTERISTICS,
    GATT_DISC_STATE_DISCOVERING_DESCRIPTORS,
    GATT_DISC_STATE_COMPLETED,
    GATT_DISC_STATE_FAILED
} gatt_disc_state_t;

struct __attribute__((packed)) gatt_descriptor {
    SLIST_ENTRY(gatt_descriptor) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(gatt_descriptor_list, gatt_descriptor);

struct __attribute__((packed)) gatt_characteristic {
    SLIST_ENTRY(gatt_characteristic) next;
    struct ble_gatt_chr chr;

    struct gatt_descriptor_list dscs;
};
SLIST_HEAD(gatt_characteristic_list, gatt_characteristic);

#if MYNEWT_VAL(BLE_INCL_SVC_DISCOVERY) || MYNEWT_VAL(BLE_GATT_CACHING_INCLUDE_SERVICES)
struct __attribute__((packed)) gatt_include_service {
    SLIST_ENTRY(gatt_include_service) next;
    struct ble_gatt_incl_svc svc;
};
SLIST_HEAD(gatt_include_service_list, gatt_include_service);
#endif

struct __attribute__((packed)) gatt_service {
    SLIST_ENTRY(gatt_service) next;
    struct ble_gatt_svc svc;
#if MYNEWT_VAL(BLE_INCL_SVC_DISCOVERY) || MYNEWT_VAL(BLE_GATT_CACHING_INCLUDE_SERVICES)
    struct gatt_include_service_list incl_svc;
#endif
    struct gatt_characteristic_list chrs;
};
SLIST_HEAD(gatt_service_list, gatt_service);

// BLE gatt discovery callback function structure
typedef struct __attribute__((packed))
{
    discovery_complete_cb_t discovery_complete_cb;     // Discovery complete callback
    service_found_cb_t service_found_cb;               // Service found callback
    characteristic_found_cb_t characteristic_found_cb; // Characteristic found callback
    descriptor_found_cb_t descriptor_found_cb;         // Descriptor found callback
    void *arg;                                         // User parameter
} ble_discovery_callbacks_t;

struct __attribute__((packed)) gatt_discovery {
    uint16_t conn_handle;
    gatt_disc_state_t state;

    /** List of discovered GATT services. */
    struct gatt_service_list services;

    /** Keeps track of where we are in the service discovery process. */
    uint16_t disc_prev_chr_val;
    struct gatt_service *cur_service;

    ble_discovery_callbacks_t callbacks;
};

gatt_discovery_t *gatt_discovery_start(uint16_t conn_handle, const ble_discovery_callbacks_t *callbacks);
esp_err_t gatt_discovery_destroy(gatt_discovery_t *discovery);

esp_err_t gatt_discovery_discover_all_services(gatt_discovery_t *discovery);
esp_err_t gatt_discovery_discover_service_by_uuid(gatt_discovery_t *discovery, const ble_uuid_t *uuid);
esp_err_t gatt_discovery_discover_all_characteristics(gatt_discovery_t *discovery);
esp_err_t gatt_discovery_discover_all_characteristic_descriptors(gatt_discovery_t *discovery);

gatt_service_t *gatt_discovery_find_service_by_uuid(gatt_discovery_t *discovery, const ble_uuid_t *uuid);
gatt_characteristic_t *gatt_discovery_find_characteristic_by_uuid(gatt_discovery_t *discovery, const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid);
gatt_descriptor_t *gatt_discovery_find_descriptor_by_uuid(gatt_discovery_t *discovery, const ble_uuid_t *svc_uuid, const ble_uuid_t *chr_uuid, const ble_uuid_t *dsc_uuid);

uint16_t gatt_discovery_get_service_count(gatt_discovery_t *discovery);
uint16_t gatt_discovery_get_characteristic_count(gatt_discovery_t *discovery, const gatt_service_t *svc);
uint16_t gatt_discovery_get_descriptor_count(gatt_discovery_t *discovery, const gatt_characteristic_t *chr);

#ifdef __cplusplus
}
#endif

#endif // GATT_DISCOVERY_H
