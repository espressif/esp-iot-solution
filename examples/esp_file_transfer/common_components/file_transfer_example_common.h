/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_gmp.h"
#include "flux_gatt_session.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_TRANSFER_EXAMPLE_MOUNT_POINT   "/fatfs"
#define FILE_TRANSFER_EXAMPLE_RECV_DIR      "/fatfs/recv"
#define FILE_TRANSFER_EXAMPLE_MAX_FILE_SIZE (2u * 1024u * 1024u)
#define FILE_TRANSFER_EXAMPLE_DEVICE_NAME   "ESP-File-Transfer"

/* Little-endian UUID bytes, used to initialize esp_ble_conn_uuid_t.uuid128. */
#define FILE_TRANSFER_EXAMPLE_SERVICE_UUID_BYTES \
    0x7a, 0x8d, 0x59, 0x10, 0xd2, 0x43, 0x46, 0x7a, \
    0xa8, 0x6f, 0x5c, 0x5e, 0x08, 0x00, 0x00, 0x01
#define FILE_TRANSFER_EXAMPLE_RX_UUID_BYTES \
    0x7a, 0x8d, 0x59, 0x10, 0xd2, 0x43, 0x46, 0x7a, \
    0xa8, 0x6f, 0x5c, 0x5e, 0x08, 0x00, 0x00, 0x02
#define FILE_TRANSFER_EXAMPLE_TX_UUID_BYTES \
    0x7a, 0x8d, 0x59, 0x10, 0xd2, 0x43, 0x46, 0x7a, \
    0xa8, 0x6f, 0x5c, 0x5e, 0x08, 0x00, 0x00, 0x03

esp_err_t file_transfer_example_init(void);
esp_err_t file_transfer_example_get_session_callbacks(gatt_session_t *session,
                                                      ble_session_callbacks_t *callbacks);
esp_err_t file_transfer_example_link_up(gatt_session_t *session);
void file_transfer_example_link_down(gatt_session_t *session);
bool file_transfer_example_link_ready(void);
void file_transfer_example_poll(void);

#ifdef __cplusplus
}
#endif
