/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** Default max payload size (64KB) */
#define ESP_XIAOZHI_PAYLOAD_MAX_DEFAULT (64 * 1024)

/**
 * @brief  Opaque payload context (protocol-agnostic buffer by offset).
 */
typedef struct esp_xiaozhi_payload_s {
    uint8_t *buf;
    size_t len;   /**< Length of data received so far */
    size_t cap;   /**< Buffer capacity */
} esp_xiaozhi_payload_t;

/**
 * @brief  Clear reassembly buffer and free memory.
 *
 * @param[in] r  Reassembly context
 */
void esp_xiaozhi_payload_clear(esp_xiaozhi_payload_t *r);

/**
 * @brief  Append a chunk into the reassembly buffer at the given offset.
 *
 *         On first chunk, buffer is allocated (by total_len if known and within max_payload, else by chunk_offset + data_len).
 *         When chunk_offset + data_len exceeds current capacity, buffer is reallocated up to max_payload.
 *
 * @param[in] r            Reassembly context
 * @param[in] data         Chunk data
 * @param[in] data_len     Chunk length
 * @param[in] chunk_offset Offset of this chunk in the full payload
 * @param[in] total_len    Total payload length (if known, > 0); 0 if unknown
 * @param[in] max_payload  Maximum allowed reassembled size
 *
 * @return
 *       - ESP_OK   On success
 *       - ESP_ERR_INVALID_ARG   Invalid arguments (NULL data, data_len <= 0, or chunk_offset < 0)
 *       - ESP_ERR_INVALID_SIZE   Reassembly would exceed max_payload
 *       - ESP_ERR_NO_MEM        Allocation or realloc failed
 */
esp_err_t esp_xiaozhi_payload_append(esp_xiaozhi_payload_t *r, const void *data, int data_len, int chunk_offset, int total_len, size_t max_payload);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
