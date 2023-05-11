/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define GATT_DEF_BLE_MTU_SIZE               23   /* Maximum Transmission Unit used in GATT */
#define BLE_UUID128_VAL_LENGTH              16
#define BLE_UUID128_16_LENGTH               8
#define BLE_UUID128_64_LENGTH               2
#define BLE_ADV_DATA_LEN_MAX                31   /* Advertising data maximum length */
#define BLE_SCAN_RSP_DATA_LEN_MAX           31   /* Scan response data maximum length */

/* EIR/AD data type definitions */
#define BLE_CONN_DATA_FLAGS                 0x01 /* AD flags */;
#define BLE_CONN_DATA_UUID16_SOME           0x02 /* 16-bit UUID, more available */
#define BLE_CONN_DATA_UUID16_ALL            0x03 /* 16-bit UUID, all listed */
#define BLE_CONN_DATA_UUID32_SOME           0x04 /* 32-bit UUID, more available */
#define BLE_CONN_DATA_UUID32_ALL            0x05 /* 32-bit UUID, all listed */
#define BLE_CONN_DATA_UUID128_SOME          0x06 /* 128-bit UUID, more available */
#define BLE_CONN_DATA_UUID128_ALL           0x07 /* 128-bit UUID, all listed */
#define BLE_CONN_DATA_NAME_SHORTENED        0x08 /* Shortened name */
#define BLE_CONN_DATA_NAME_COMPLETE         0x09 /* Complete name */
#define BLE_CONN_DATA_TX_POWER              0x0a /* Tx Power */
#define BLE_CONN_DATA_SOLICIT16             0x14 /* Solicit UUIDs, 16-bit */
#define BLE_CONN_DATA_SOLICIT128            0x15 /* Solicit UUIDs, 128-bit */
#define BLE_CONN_DATA_SVC_DATA16            0x16 /* Service data, 16-bit UUID */
#define BLE_CONN_DATA_GAP_APPEARANCE        0x19 /* GAP appearance */
#define BLE_CONN_DATA_SOLICIT32             0x1f /* Solicit UUIDs, 32-bit */
#define BLE_CONN_DATA_SVC_DATA32            0x20 /* Service data, 32-bit UUID */
#define BLE_CONN_DATA_SVC_DATA128           0x21 /* Service data, 128-bit UUID */
#define BLE_CONN_DATA_URI                   0x24 /* URI */
#define BLE_CONN_DATA_CONN_PROV             0x29 /* Mesh Provisioning PDU */
#define BLE_CONN_DATA_CONN_MESSAGE          0x2a /* Mesh Networking PDU */
#define BLE_CONN_DATA_CONN_BEACON           0x2b /* Mesh Beacon */

#define BLE_CONN_DATA_MANUFACTURER_DATA     0xff /* Manufacturer Specific Data */

#define BLE_CONN_AD_LIMITED                 0x01 /* Limited Discoverable */
#define BLE_CONN_AD_GENERAL                 0x02 /* General Discoverable */
#define BLE_CONN_AD_NO_BREDR                0x04 /* BR/EDR not supported */

/**
 * @brief   This structure maps handler required by UUID which are used to
 *          BLE characteristics from a client device.
 */
typedef struct {
    uint8_t type;
    uint8_t length;
    uint8_t *value;
} esp_nimble_maps_t;

typedef void (esp_ble_conn_nimble_cb_t)(struct ble_gap_event *event, void *arg);
void ble_store_config_init(void);
int  ble_uuid_flat(const ble_uuid_t *, void *);

/*  Standard 16 bit UUID for characteristic User Description*/
#define BLE_GATT_UUID_CHAR_DSC              0x2901

#ifdef __cplusplus
}
#endif
