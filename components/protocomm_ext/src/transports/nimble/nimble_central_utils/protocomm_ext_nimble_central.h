/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/**
 * @file protocomm_ext_nimble_central.h
 * @brief Internal NimBLE peer database helpers for protocomm_ext BLE transport.
 *
 * Provides a lightweight GATT discovery cache used by the NimBLE central path.
 * Not part of the public protocomm_ext API.
 */

#include <stdint.h>
#include <sys/queue.h>

#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Length (in bytes) of a BLE device address. */
#define PEER_ADDR_VAL_SIZE 6

struct peer_dsc {
    SLIST_ENTRY(peer_dsc) next;
    struct ble_gatt_dsc dsc;
};
SLIST_HEAD(peer_dsc_list, peer_dsc);

struct peer_chr {
    SLIST_ENTRY(peer_chr) next;
    struct ble_gatt_chr chr;

    struct peer_dsc_list dscs;
};
SLIST_HEAD(peer_chr_list, peer_chr);
SLIST_HEAD(peer_svc_list, peer_svc);

struct peer_svc {
    SLIST_ENTRY(peer_svc) next;
    struct ble_gatt_svc svc;
    struct peer_chr_list chrs;
};

struct peer;

/**
 * @brief Callback invoked when peer discovery completes.
 *
 * @param peer   Peer handle
 * @param status 0 on success, otherwise a NimBLE error code
 * @param arg    User argument passed to discovery APIs
 */
typedef void peer_disc_fn(const struct peer *peer, int status, void *arg);

/**
 * @brief The callback function for the devices traversal.
 *
 * @param peer Peer being visited
 * @param arg  User argument
 * @return 0 to continue; non-zero to stop traversal
 */
typedef int peer_traverse_fn(const struct peer *peer, void *arg);

struct peer {
    SLIST_ENTRY(peer) next;
    uint16_t conn_handle;

    uint8_t peer_addr[PEER_ADDR_VAL_SIZE];

    /** List of discovered GATT services. */
    struct peer_svc_list svcs;

    /** Keeps track of where we are in the service discovery process. */
    uint16_t disc_prev_chr_val;
    struct peer_svc *cur_svc;

    /** Callback that gets executed when service discovery completes. */
    peer_disc_fn *disc_cb;
    void *disc_cb_arg;
};

void peer_traverse_all(peer_traverse_fn *trav_cb, void *arg);

int peer_disc_svc_by_uuid(uint16_t conn_handle, const ble_uuid_t *uuid,
                          peer_disc_fn *disc_cb, void *disc_cb_arg);

int peer_disc_all(uint16_t conn_handle, peer_disc_fn *disc_cb,
                  void *disc_cb_arg);

const struct peer_dsc *peer_dsc_find_uuid(const struct peer *peer,
                                          const ble_uuid_t *svc_uuid,
                                          const ble_uuid_t *chr_uuid,
                                          const ble_uuid_t *dsc_uuid);

const struct peer_chr *peer_chr_find_uuid(const struct peer *peer,
                                          const ble_uuid_t *svc_uuid,
                                          const ble_uuid_t *chr_uuid);

const struct peer_svc *peer_svc_find_uuid(const struct peer *peer, const ble_uuid_t *uuid);

int peer_delete(uint16_t conn_handle);

int peer_add(uint16_t conn_handle);

int peer_init(int max_peers, int max_svcs, int max_chrs, int max_dscs);

int peer_deinit(void);

struct peer *peer_find(uint16_t conn_handle);

#if MYNEWT_VAL(ENC_ADV_DATA)
int peer_set_addr(uint16_t conn_handle, uint8_t *peer_addr);
#endif

#ifdef __cplusplus
}
#endif
