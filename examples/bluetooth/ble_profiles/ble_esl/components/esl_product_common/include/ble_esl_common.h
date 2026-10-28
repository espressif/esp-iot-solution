/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Compatibility aliases so product ESL example sources can compile against
 * iot-solution `esp_esl` / `esp_eslp_*` types.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef ESL_TAG_ASSOC_HOST_TEST

#ifndef BLE_ESL_SESSION_KEY_SIZE
#define BLE_ESL_SESSION_KEY_SIZE        16
#endif
#ifndef BLE_ESL_IV_SIZE
#define BLE_ESL_IV_SIZE                 8
#endif
#ifndef BLE_ESL_KEY_MATERIAL_SIZE
#define BLE_ESL_KEY_MATERIAL_SIZE       24
#endif
#ifndef BLE_ESL_BROADCAST_ADDRESS
#define BLE_ESL_BROADCAST_ADDRESS       0xFF
#endif
#ifndef BLE_ESL_ECP_MAX_SIZE
#define BLE_ESL_ECP_MAX_SIZE            17
#endif

typedef struct {
    uint8_t esl_id;
    uint8_t group_id_rfu;
} __attribute__((packed)) ble_esl_address_t;

typedef struct {
    uint8_t session_key[BLE_ESL_SESSION_KEY_SIZE];
    uint8_t iv[BLE_ESL_IV_SIZE];
} __attribute__((packed)) ble_esl_key_material_t;

#else /* !ESL_TAG_ASSOC_HOST_TEST */

#include "esp_esl.h"
#include "esp_eslp_state.h"
#include "esp_ble_conn_mgr.h"
#include "host/ble_hs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_ble_esl_address_t ble_esl_address_t;
typedef esp_ble_esl_key_material_t ble_esl_key_material_t;
typedef esp_ble_eslp_state_t ble_esl_state_t;

#define BLE_ESL_TLV_MAX_SIZE            BLE_ESL_ECP_MAX_SIZE

#define BLE_ESL_STATE_UNASSOCIATED      BLE_ESLP_STATE_UNASSOCIATED
#define BLE_ESL_STATE_CONFIGURING       BLE_ESLP_STATE_CONFIGURING
#define BLE_ESL_STATE_UPDATING          BLE_ESLP_STATE_UPDATING
#define BLE_ESL_STATE_SYNCHRONIZED      BLE_ESLP_STATE_SYNCHRONIZED
#define BLE_ESL_STATE_UNSYNCHRONIZED    BLE_ESLP_STATE_UNSYNCHRONIZED

#define BLE_ESL_DISPLAY_TYPE_BLACK_WHITE    0x01

#define ESL_AP_RESP_ERROR               BLE_ESL_RESP_ERROR
#define ESL_AP_RESP_LED_STATE           BLE_ESL_RESP_LED_STATE
#define ESL_AP_RESP_BASIC_STATE         BLE_ESL_RESP_BASIC_STATE
#define ESL_AP_RESP_DISPLAY_STATE       BLE_ESL_RESP_DISPLAY_STATE

#ifndef BLE_HS_CONN_HANDLE_NONE
#define BLE_HS_CONN_HANDLE_NONE         BLE_CONN_HANDLE_INVALID
#endif

typedef struct __attribute__((packed))
{
    uint8_t pattern[5];
    uint8_t bit_off_period;
    uint8_t bit_on_period;
} ble_esl_flashing_pattern_t;

#define BLE_ESL_LED_CTRL_COLOR_BRIGHTNESS(r, g, b, bright) \
    ((uint8_t)(((r) & 0x03) | (((g) & 0x03) << 2) | (((b) & 0x03) << 4) | (((bright) & 0x03) << 6)))

#define BLE_ESL_LED_CTRL_REPEAT(type, duration) \
    ((uint16_t)(((type) & 0x01) | (((duration) & 0x7FFF) << 1)))

static inline ble_esl_address_t ble_esl_addr_make(uint8_t esl_id, uint8_t group_id)
{
    ble_esl_address_t addr = {
        .esl_id = esl_id,
        .group_id_rfu = (uint8_t)(group_id & 0x7F),
    };
    return addr;
}

#ifdef __cplusplus
}
#endif

#endif /* ESL_TAG_ASSOC_HOST_TEST */
