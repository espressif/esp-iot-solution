/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_bt_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t hid_conn_id;
    bool sec_conn;
    bool runtime_config_mode;
    bool mouse_notify_enabled;
    bool abs_mouse_notify_enabled;
    bool key_notify_enabled;
    bool cc_notify_enabled;
    bool peer_bonded;
    bool cccd_seen_this_connection;
    bool cccd_restored_from_nvs;
    bool has_peer;
    esp_bd_addr_t peer_bda;
    uint8_t peer_addr_type;
} airmouse_ble_handle_t;

static inline bool airmouse_ble_mouse_ready(const airmouse_ble_handle_t *h)
{
    return h != NULL && h->sec_conn && h->mouse_notify_enabled &&
           !h->runtime_config_mode;
}

esp_err_t airmouse_ble_init(void);
esp_err_t airmouse_ble_deinit(void);
airmouse_ble_handle_t *airmouse_ble_hid_get_ctx(void);

#ifdef __cplusplus
}
#endif
