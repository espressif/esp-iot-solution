/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Asset configuration structure, contains the asset table and other configuration information.
 */
typedef struct {
    const char *partition_label;            /*!< Configuration partition_label */
    int max_files;                          /*!< Number of assets */
    uint32_t checksum;                      /*!< Checksum of table */
} mmap_assets_config_t;

/**
 * @brief Asset handle type, points to the asset.
 */
typedef struct mmap_assets_t *mmap_assets_handle_t;       /*!< Type of asset handle */

/**
 * @brief Create a new asset instance.
 *
 * @param[in]  config   Pointer to the asset configuration structure.
 * @param[out] ret_item Pointer to the handle of the newly created asset instance.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Insufficient memory
 *     - ESP_ERR_NOT_FOUND: Can't find partition
 *     - ESP_ERR_INVALID_SIZE: File num mismatch
 *     - ESP_ERR_INVALID_CRC: Checksum mismatch
 */
esp_err_t mmap_assets_new(const mmap_assets_config_t *config, mmap_assets_handle_t *ret_item);

/**
 * @brief Delete an asset instance.
 *
 * @param[in] handle Asset instance handle.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t mmap_assets_del(mmap_assets_handle_t handle);

/**
 * @brief Get the memory of the asset at the specified index.
 *
 * @param[in] handle Asset instance handle.
 * @param[in] index  Index of the asset.
 *
 * @return Pointer to the asset memory, or NULL if index is invalid.
 */
const uint8_t * mmap_assets_get_mem(mmap_assets_handle_t handle, int index);

/**
 * @brief Get the name of the asset at the specified index.
 *
 * @param[in] handle Asset instance handle.
 * @param[in] index  Index of the asset.
 *
 * @return Pointer to the asset name, or NULL if index is invalid.
 */
const char * mmap_assets_get_name(mmap_assets_handle_t handle, int index);

/**
 * @brief Get the size of the asset at the specified index.
 *
 * @param[in] handle Asset instance handle.
 * @param[in] index  Index of the asset.
 *
 * @return Size of the asset, or -1 if index is invalid.
 */
int mmap_assets_get_size(mmap_assets_handle_t handle, int index);

/**
 * @brief Get the width of the asset at the specified index.
 *
 * @param[in] handle Asset instance handle.
 * @param[in] index  Index of the asset.
 *
 * @return Width of the asset, or -1 if index is invalid.
 */
int mmap_assets_get_width(mmap_assets_handle_t handle, int index);

/**
 * @brief Get the height of the asset at the specified index.
 *
 * @param[in] handle Asset instance handle.
 * @param[in] index  Index of the asset.
 *
 * @return Height of the asset, or -1 if index is invalid.
 */
int mmap_assets_get_height(mmap_assets_handle_t handle, int index);

#ifdef __cplusplus
}
#endif
