/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esl_tag_assoc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load association blob from NVS.
 *
 * @return
 *  - ESP_OK on valid blob
 *  - ESP_ERR_NOT_FOUND when key missing
 *  - ESP_ERR_INVALID_CRC / ESP_ERR_INVALID_STATE on corrupt/invalid blob
 *    (caller should erase)
 */
esp_err_t esl_tag_storage_load_assoc(esl_tag_assoc_blob_t *out);

/**
 * Atomically save a prepared association blob.
 * Calls esl_tag_assoc_prepare() before write.
 */
esp_err_t esl_tag_storage_save_assoc(esl_tag_assoc_blob_t *blob);

/** Erase association blob. Idempotent. */
esp_err_t esl_tag_storage_erase_assoc(void);

#ifdef __cplusplus
}
#endif
