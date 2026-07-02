/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_heap_caps.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief RAMFS mount configuration structure
 */
typedef struct {
    const char *base_path; /*!< VFS mount point, e.g. "/ram". Must start with "/" and must not end with "/" */
    size_t max_files;     /*!< Maximum number of files that can be open at the same time, not the total number of files stored in RAMFS */
    size_t max_bytes;     /*!< Maximum total bytes available for file contents in this RAMFS mount. Directory metadata is not counted */
    uint32_t caps;        /*!< Heap capability flags used for RAMFS internal allocations. Must include MALLOC_CAP_8BIT, e.g. MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT or MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT */
} ramfs_config_t;

/**
 * @brief Register a RAM-backed filesystem with VFS
 *
 * @param[in] config RAMFS mount configuration. Must not be NULL
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_INVALID_STATE if the mount point is already registered
 *          - ESP_ERR_NO_MEM        if out of memory or no RAMFS context slot is available
 *          - ESP_OK                on success
 */
esp_err_t ramfs_register(const ramfs_config_t *config);

/**
 * @brief Unregister a RAM-backed filesystem from VFS and release all stored files
 *
 * @param[in] base_path VFS mount point passed to `ramfs_register()`
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_INVALID_STATE if RAMFS is not registered at the mount point, or files/directories are still open
 *          - ESP_OK                on success
 */
esp_err_t ramfs_unregister(const char *base_path);

/**
 * @brief Get RAMFS capacity and current file-content usage
 *
 * @param[in] base_path VFS mount point passed to `ramfs_register()`
 * @param[out] out_total_bytes Total bytes available for file contents, can be NULL
 * @param[out] out_used_bytes Bytes currently used by file contents, can be NULL
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_INVALID_STATE if RAMFS is not registered at the mount point
 *          - ESP_OK                on success
 */
esp_err_t ramfs_info(const char *base_path, size_t *out_total_bytes, size_t *out_used_bytes);

/**
 * @brief Persist one RAMFS file to an external filesystem path
 *
 * @param[in] ramfs_path Source file path in RAMFS
 * @param[in] fatfs_path Destination file path in the external filesystem
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid, source is not a file, or destination path is invalid
 *          - ESP_ERR_NOT_FOUND     if the source RAMFS file is not found
 *          - ESP_ERR_INVALID_STATE if the source RAMFS file is open
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_FAIL              if external filesystem IO fails
 *          - ESP_OK                on success
 */
esp_err_t ramfs_sync_file_to_fatfs(const char *ramfs_path, const char *fatfs_path);

/**
 * @brief Persist one RAMFS directory tree to an external filesystem directory
 *
 * @param[in] ramfs_dir Source directory path in RAMFS
 * @param[in] fatfs_dir Destination directory path in the external filesystem
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid, source is not a directory, or destination path is invalid
 *          - ESP_ERR_NOT_FOUND     if the source RAMFS directory is not found
 *          - ESP_ERR_INVALID_STATE if any node in the source RAMFS tree is open
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_FAIL              if external filesystem IO fails
 *          - ESP_OK                on success
 */
esp_err_t ramfs_sync_tree_to_fatfs(const char *ramfs_dir, const char *fatfs_dir);

/**
 * @brief Load one external filesystem file into RAMFS
 *
 * @param[in] fatfs_path Source file path in the external filesystem
 * @param[in] ramfs_path Destination file path in RAMFS
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid, source is not a file, or destination path is invalid
 *          - ESP_ERR_NOT_FOUND     if the source external filesystem file is not found
 *          - ESP_ERR_INVALID_STATE if the destination RAMFS file is open
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_FAIL              if IO fails
 *          - ESP_OK                on success
 */
esp_err_t ramfs_load_file_from_fatfs(const char *fatfs_path, const char *ramfs_path);

/**
 * @brief Load one external filesystem directory tree into RAMFS
 *
 * @param[in] fatfs_dir Source directory path in the external filesystem
 * @param[in] ramfs_dir Destination directory path in RAMFS
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid, source is not a directory, or destination path is invalid
 *          - ESP_ERR_NOT_FOUND     if the source external filesystem directory is not found
 *          - ESP_ERR_INVALID_STATE if any destination RAMFS node that would be replaced is open
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_FAIL              if IO fails
 *          - ESP_OK                on success
 */
esp_err_t ramfs_load_tree_from_fatfs(const char *fatfs_dir, const char *ramfs_dir);

#ifdef __cplusplus
}
#endif
