/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file esl_tag_assoc.h
 * @brief Pure-C TAG association wire blob (host-testable, no NVS/GAP).
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "ble_esl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESL_TAG_ASSOC_MAGIC         0x54414731u /* "TAG1" */
#define ESL_TAG_ASSOC_SCHEMA_VER    1

/**
 * Fixed-width NVS wire layout. No bool/enum/padding fields.
 * Multi-byte integers are little-endian on the wire.
 */
typedef struct __attribute__((packed))
{
    uint32_t magic_le;
    uint16_t schema_version_le;
    uint16_t payload_len_le; /* bytes after header through peer_addr (excl. checksum) */
    ble_esl_address_t esl_address;
    ble_esl_key_material_t ap_sync_key;
    ble_esl_key_material_t resp_key;
    uint8_t peer_addr_type;
    uint8_t peer_addr[6];
    uint32_t checksum_le; /* CRC-32 over [0 .. offsetof(checksum_le)) with checksum zeroed */
} esl_tag_assoc_blob_t;

_Static_assert(sizeof(ble_esl_address_t) == 2, "ble_esl_address_t size");
_Static_assert(sizeof(ble_esl_key_material_t) == BLE_ESL_KEY_MATERIAL_SIZE,
               "ap_sync_key size");
_Static_assert(sizeof(ble_esl_key_material_t) == BLE_ESL_KEY_MATERIAL_SIZE,
               "resp_key size");
_Static_assert(sizeof(esl_tag_assoc_blob_t) ==
               (4 + 2 + 2 + 2 + BLE_ESL_KEY_MATERIAL_SIZE + BLE_ESL_KEY_MATERIAL_SIZE + 1 + 6 + 4),
               "esl_tag_assoc_blob_t size drift");

/** Payload length covering fields after the 8-byte header through peer_addr. */
#define ESL_TAG_ASSOC_PAYLOAD_LEN \
    ((uint16_t)(sizeof(esl_tag_assoc_blob_t) - 8u - sizeof(uint32_t)))

static inline uint32_t esl_tag_assoc_rd_le32(uint32_t v)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return v;
#else
    return ((v & 0xffu) << 24) | ((v & 0xff00u) << 8) |
           ((v & 0xff0000u) >> 8) | ((v >> 24) & 0xffu);
#endif
}

static inline uint16_t esl_tag_assoc_rd_le16(uint16_t v)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return v;
#else
    return (uint16_t)(((v & 0xffu) << 8) | ((v >> 8) & 0xffu));
#endif
}

static inline uint32_t esl_tag_assoc_wr_le32(uint32_t v)
{
    return esl_tag_assoc_rd_le32(v);
}

static inline uint16_t esl_tag_assoc_wr_le16(uint16_t v)
{
    return esl_tag_assoc_rd_le16(v);
}

/**
 * CRC-32/ISO-HDLC (poly 0x04C11DB7, init/xorout 0xFFFFFFFF, refin/refout).
 * Matches esl_nvs_crc32 (both produce the standard CRC-32/ISO-HDLC value).
 */
static inline uint32_t esl_tag_assoc_crc32(const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static inline uint32_t esl_tag_assoc_compute_checksum(const esl_tag_assoc_blob_t *blob)
{
    esl_tag_assoc_blob_t tmp;

    memcpy(&tmp, blob, sizeof(tmp));
    tmp.checksum_le = 0;
    return esl_tag_assoc_crc32(&tmp, offsetof(esl_tag_assoc_blob_t, checksum_le));
}

static inline void esl_tag_assoc_prepare(esl_tag_assoc_blob_t *blob)
{
    if (blob == NULL) {
        return;
    }
    blob->magic_le = esl_tag_assoc_wr_le32(ESL_TAG_ASSOC_MAGIC);
    blob->schema_version_le = esl_tag_assoc_wr_le16(ESL_TAG_ASSOC_SCHEMA_VER);
    blob->payload_len_le = esl_tag_assoc_wr_le16(ESL_TAG_ASSOC_PAYLOAD_LEN);
    blob->checksum_le = esl_tag_assoc_wr_le32(esl_tag_assoc_compute_checksum(blob));
}

typedef enum {
    ESL_TAG_ASSOC_OK = 0,
    ESL_TAG_ASSOC_ERR_NULL,
    ESL_TAG_ASSOC_ERR_MAGIC,
    ESL_TAG_ASSOC_ERR_SCHEMA,
    ESL_TAG_ASSOC_ERR_LEN,
    ESL_TAG_ASSOC_ERR_CRC,
    ESL_TAG_ASSOC_ERR_ADDRESS,
    ESL_TAG_ASSOC_ERR_PEER,
} esl_tag_assoc_status_t;

static inline bool esl_tag_assoc_peer_addr_set(const uint8_t addr[6])
{
    for (int i = 0; i < 6; i++) {
        if (addr[i] != 0) {
            return true;
        }
    }
    return false;
}

static inline esl_tag_assoc_status_t esl_tag_assoc_validate(const esl_tag_assoc_blob_t *blob)
{
    if (blob == NULL) {
        return ESL_TAG_ASSOC_ERR_NULL;
    }
    if (esl_tag_assoc_rd_le32(blob->magic_le) != ESL_TAG_ASSOC_MAGIC) {
        return ESL_TAG_ASSOC_ERR_MAGIC;
    }
    if (esl_tag_assoc_rd_le16(blob->schema_version_le) != ESL_TAG_ASSOC_SCHEMA_VER) {
        return ESL_TAG_ASSOC_ERR_SCHEMA;
    }
    if (esl_tag_assoc_rd_le16(blob->payload_len_le) != ESL_TAG_ASSOC_PAYLOAD_LEN) {
        return ESL_TAG_ASSOC_ERR_LEN;
    }
    if (esl_tag_assoc_rd_le32(blob->checksum_le) != esl_tag_assoc_compute_checksum(blob)) {
        return ESL_TAG_ASSOC_ERR_CRC;
    }
    if (blob->esl_address.esl_id == BLE_ESL_BROADCAST_ADDRESS) {
        return ESL_TAG_ASSOC_ERR_ADDRESS;
    }
    if (!esl_tag_assoc_peer_addr_set(blob->peer_addr)) {
        return ESL_TAG_ASSOC_ERR_PEER;
    }
    return ESL_TAG_ASSOC_OK;
}

#ifdef __cplusplus
}
#endif
