/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esl_product_types.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esl_nvs_init(void);

/**
 * Load versioned inventory blob.
 *
 * @return
 *  - ESP_OK on success (including empty inventory when key is missing)
 *  - ESP_ERR_INVALID_CRC / ESP_ERR_INVALID_STATE when blob checksum/schema fails
 *    (NVS is NOT wiped)
 *  - other NVS errors as returned by the flash layer
 */
esp_err_t esl_nvs_load_inventory(esl_inventory_blob_t *blob);

/**
 * Persist inventory blob. Updates header.generation and checksum before write.
 */
esp_err_t esl_nvs_save_inventory(esl_inventory_blob_t *blob);

/** CRC-32/ISO-HDLC over buf (poly 0x04C11DB7, init/xorout 0xFFFFFFFF, refin/refout). */
uint32_t esl_nvs_crc32(const void *buf, size_t len);

/** Fill header fields (except generation/checksum) and recompute checksum. */
void esl_nvs_blob_prepare_header(esl_inventory_blob_t *blob, uint32_t generation);

/** Validate magic/version/lengths/checksum and per-record wire semantics. */
esp_err_t esl_nvs_blob_validate(const esl_inventory_blob_t *blob);

#ifdef __cplusplus
}
#endif
