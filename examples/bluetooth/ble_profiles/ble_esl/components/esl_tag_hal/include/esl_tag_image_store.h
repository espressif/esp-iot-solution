/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize image store, clean legacy NVS meta keys, handle wipe-pending,
 * restore active slots into OTS RAM, then start the flash worker.
 *
 * Must be called after ble_esl_init() (OTS objects exist) and before
 * ble_esl_start().
 *
 * @param active_slot_count CONFIG_ESL_TAG_MAX_IMAGES (1..4)
 */
esp_err_t esl_tag_image_store_init(uint8_t active_slot_count);

/**
 * Queue an async save of a completed OTS image slot.
 * Rejects while wipe-pending / erase is in progress.
 */
esp_err_t esl_tag_image_store_queue_save(uint8_t image_index);

/**
 * Persist wipe-pending flag then queue full-partition erase.
 * Used by Unassociate / Factory Reset.
 */
esp_err_t esl_tag_image_store_queue_erase_all(void);

#ifdef __cplusplus
}
#endif
