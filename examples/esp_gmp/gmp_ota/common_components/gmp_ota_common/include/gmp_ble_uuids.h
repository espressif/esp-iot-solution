/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ESP-GMP BLE UUIDs (SPEC §10).
 */

#pragma once

#include <string.h>
#include "host/ble_uuid.h"

#define GMP_BLE_DEVICE_NAME "ESP-GMP-OTA"

/* a0eeffc0-504f-4b53-b62f-0a0000000001 */
#define GMP_BLE_UUID_SVC \
    BLE_UUID128_INIT(0x01, 0x00, 0x00, 0x00, 0x0a, 0x2f, 0xb6, 0x53, 0x4b, 0x4f, 0x50, 0xc0, 0xff, 0xee, 0xa0)

/* ...0002 RX (Write) */
#define GMP_BLE_UUID_CHR_RX \
    BLE_UUID128_INIT(0x02, 0x00, 0x00, 0x00, 0x0a, 0x2f, 0xb6, 0x53, 0x4b, 0x4f, 0x50, 0xc0, 0xff, 0xee, 0xa0)

/* ...0003 TX (Notify) */
#define GMP_BLE_UUID_CHR_TX \
    BLE_UUID128_INIT(0x03, 0x00, 0x00, 0x00, 0x0a, 0x2f, 0xb6, 0x53, 0x4b, 0x4f, 0x50, 0xc0, 0xff, 0xee, 0xa0)

static inline bool gmp_ble_uuid128_eq(const ble_uuid128_t *u, const ble_uuid128_t *ref)
{
    if (u->u.type != BLE_UUID_TYPE_128) {
        return false;
    }
    return memcmp(u->value, ref->value, 16) == 0;
}
