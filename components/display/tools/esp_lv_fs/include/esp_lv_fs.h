/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_mmap_assets.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for the filesystem
 */
typedef struct {
    char fs_letter;                   /*!< Filesystem letter identifier */
    int fs_nums;                      /*!< Number of filesystem instances */
    mmap_assets_handle_t fs_assets;   /*!< Handle to memory-mapped assets */
} fs_cfg_t;

/**
 * @brief Filesystem handle type definition
 */
typedef void *esp_lv_fs_handle_t;

/**
 * @brief Initialize file descriptors for the filesystem.
 *
 * This function sets up the filesystem by initializing file descriptors
 * based on the provided configuration. It allocates necessary memory and
 * populates the file descriptors with information about the assets.
 *
 * @param[in] cfg           Pointer to the filesystem configuration structure.
 * @param[out] ret_handle   Pointer to the handle that will hold the initialized filesystem instance.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 *     - ESP_ERR_NO_MEM: Memory allocation failed
 */
esp_err_t esp_lv_fs_desc_init(const fs_cfg_t *cfg, esp_lv_fs_handle_t *ret_handle);

/**
 * @brief Deinitialize the filesystem and clean up resources.
 *
 * This function cleans up the filesystem by freeing allocated memory
 * for file descriptors and other resources associated with the provided handle.
 *
 * @param[in] handle Handle to the filesystem instance to be deinitialized.
 *
 * @return
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid argument
 */
esp_err_t esp_lv_fs_desc_deinit(esp_lv_fs_handle_t handle);

#ifdef __cplusplus
} /* extern "C" */
#endif
