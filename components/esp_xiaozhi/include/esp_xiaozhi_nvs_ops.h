/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/** @brief Opaque NVS handle passed through registered NVS ops (same width as IDF nvs_handle_t). */
typedef uint32_t nvs_ops_handle_t;

/**
 * @brief  NVS operations callbacks for keystore.
 *         When set via esp_xiaozhi_nvs_ops_register(), all NVS access goes through these
 *         callbacks (e.g. to run NVS on an SRAM task and avoid PSRAM stack issues). NULL means use
 *         default direct nvs_* calls.
 */
typedef struct esp_xiaozhi_nvs_ops_s {
    esp_err_t (*nvs_open)(const char *name_space, bool read_write, nvs_ops_handle_t *out_handle); /*!< Same semantics as IDF nvs_open() */
    void (*nvs_close)(nvs_ops_handle_t handle);                                                     /*!< Same semantics as IDF nvs_close() */
    esp_err_t (*nvs_commit)(nvs_ops_handle_t handle);                                               /*!< Same semantics as IDF nvs_commit() */
    esp_err_t (*nvs_get_str)(nvs_ops_handle_t handle, const char *key, char *out_value, size_t *length); /*!< Same semantics as IDF nvs_get_str() */
    esp_err_t (*nvs_set_str)(nvs_ops_handle_t handle, const char *key, const char *value);          /*!< Same semantics as IDF nvs_set_str() */
    esp_err_t (*nvs_get_i32)(nvs_ops_handle_t handle, const char *key, int32_t *out_value);         /*!< Same semantics as IDF nvs_get_i32() */
    esp_err_t (*nvs_set_i32)(nvs_ops_handle_t handle, const char *key, int32_t value);              /*!< Same semantics as IDF nvs_set_i32() */
    esp_err_t (*nvs_erase_key)(nvs_ops_handle_t handle, const char *key);                           /*!< Same semantics as IDF nvs_erase_key() */
    esp_err_t (*nvs_erase_all)(nvs_ops_handle_t handle);                                            /*!< Same semantics as IDF nvs_erase_all() */
} esp_xiaozhi_nvs_ops_t;

/**
 * @brief  Register NVS operations (e.g. to delegate to NVS service on SRAM task). Pass NULL to use default direct NVS.
 *
 * @param[in] ops  Pointer to ops struct, or NULL for default
 */
void esp_xiaozhi_nvs_ops_register(const esp_xiaozhi_nvs_ops_t *ops);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
