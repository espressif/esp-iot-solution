/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host-testable layout helpers for Tag image A/B bank persistence.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESL_IMG_BANK_SIZE_BYTES       8192u
#define ESL_IMG_BANKS_PER_SLOT        2u
#define ESL_IMG_PHYSICAL_SLOT_COUNT   4u
#define ESL_IMG_PARTITION_SIZE_BYTES  \
    (ESL_IMG_PHYSICAL_SLOT_COUNT * ESL_IMG_BANKS_PER_SLOT * ESL_IMG_BANK_SIZE_BYTES)

#define ESL_IMG_MAGIC                 0x494C5345u /* 'ESLI' little-endian */
#define ESL_IMG_SCHEMA                1u
#define ESL_IMG_COMMIT_MARKER         0xC0111111u
#define ESL_IMG_COMMIT_ERASED         0xFFFFFFFFu
#define ESL_IMG_HEADER_PREFIX_LEN     16u /* magic .. data_len */

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint16_t schema;
    uint8_t image_index;
    uint8_t reserved0;
    uint32_t generation;
    uint32_t data_len;
    uint32_t crc32;
    uint8_t reserved1[8];
    uint32_t commit_marker;
} esl_img_bank_header_t;

_Static_assert(sizeof(esl_img_bank_header_t) == 32, "esl_img_bank_header_t size drift");

typedef struct {
    bool valid;
    uint32_t generation;
    uint32_t data_len;
    uint8_t bank_index;
} esl_img_bank_view_t;

static inline size_t esl_img_slot_base_offset(uint8_t slot)
{
    return (size_t)slot * ESL_IMG_BANKS_PER_SLOT * ESL_IMG_BANK_SIZE_BYTES;
}

static inline size_t esl_img_bank_offset(uint8_t slot, uint8_t bank)
{
    return esl_img_slot_base_offset(slot) +
           ((size_t)bank * ESL_IMG_BANK_SIZE_BYTES);
}

static inline bool esl_img_generation_newer(uint32_t candidate, uint32_t baseline)
{
    return (int32_t)(candidate - baseline) > 0;
}

/** CRC-32/ISO-HDLC update (poly 0xEDB88320, reflected). Seed with 0. */
static inline uint32_t esl_img_crc32_le_update(uint32_t crc, const uint8_t *data, size_t len)
{
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

/**
 * CRC over header prefix (magic..data_len) + payload.
 * CRC and commit fields are intentionally excluded.
 */
static inline uint32_t esl_img_compute_crc(const esl_img_bank_header_t *hdr,
                                           const uint8_t *payload,
                                           uint32_t data_len)
{
    uint8_t prefix[ESL_IMG_HEADER_PREFIX_LEN];

    if (hdr == NULL || (data_len > 0 && payload == NULL)) {
        return 0;
    }

    memcpy(prefix, hdr, ESL_IMG_HEADER_PREFIX_LEN);
    uint32_t crc = esl_img_crc32_le_update(0, prefix, sizeof(prefix));
    if (data_len > 0) {
        crc = esl_img_crc32_le_update(crc, payload, data_len);
    }
    return crc;
}

/**
 * Validate a bank header against payload CRC and slot identity.
 * @param max_image_size Upper bound for data_len (CONFIG_BLE_ESL_MAX_IMAGE_SIZE).
 */
static inline bool esl_img_bank_header_valid(const esl_img_bank_header_t *hdr,
                                             uint8_t slot,
                                             uint32_t max_image_size,
                                             const uint8_t *payload)
{
    if (hdr == NULL) {
        return false;
    }
    if (hdr->commit_marker != ESL_IMG_COMMIT_MARKER) {
        return false;
    }
    if (hdr->magic != ESL_IMG_MAGIC || hdr->schema != ESL_IMG_SCHEMA) {
        return false;
    }
    if (hdr->image_index != slot) {
        return false;
    }
    if (hdr->data_len == 0 || hdr->data_len > max_image_size) {
        return false;
    }
    if (payload == NULL) {
        return false;
    }
    return esl_img_compute_crc(hdr, payload, hdr->data_len) == hdr->crc32;
}

/**
 * Pick the active bank among two validated views.
 * @return 0 or 1 for bank index, or -1 if neither valid.
 */
static inline int esl_img_pick_active_bank(const esl_img_bank_view_t *a,
                                           const esl_img_bank_view_t *b)
{
    if (a == NULL || b == NULL) {
        return -1;
    }
    if (!a->valid && !b->valid) {
        return -1;
    }
    if (!a->valid) {
        return (int)b->bank_index;
    }
    if (!b->valid) {
        return (int)a->bank_index;
    }
    if (esl_img_generation_newer(b->generation, a->generation)) {
        return (int)b->bank_index;
    }
    return (int)a->bank_index;
}

/** Inactive bank for the next write; prefer bank 0 when none are valid. */
static inline uint8_t esl_img_next_write_bank(int active_bank)
{
    if (active_bank < 0) {
        return 0;
    }
    return (uint8_t)(1 - active_bank);
}

static inline uint32_t esl_img_next_generation(uint32_t active_generation, bool have_active)
{
    if (!have_active) {
        return 1;
    }
    return active_generation + 1u;
}

#ifdef __cplusplus
}
#endif
