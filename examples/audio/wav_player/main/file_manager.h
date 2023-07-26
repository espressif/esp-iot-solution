/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_spiffs.h"
#include "esp_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOUNT_POINT  "/audio"

#ifdef CONFIG_STORAGE_SDCARD
/**
 * @brief Initialize the sdcard
 *
 * @return ESP_OK on success
 */
esp_err_t fm_sdcard_init(void);
#endif

/**
 * @brief Initialize the spiffs
 *
 * @return ESP_OK on success
 */
esp_err_t fm_spiffs_init(void);

/**
 * @brief Traversing the file and printing
 *
 * @param direntName Root directory
 * @param level Traversal depth
 */
void fm_print_dir(char *direntName, int level);

/**
 * @brief Get the root directory
 *
 * @return Root directory
 */
const char *fm_get_basepath(void);

/**
 * @brief Get the file name
 *
 * @param file file Pointer
 * @return file name
 */
const char *fm_get_filename(const char *file);

/**
 * @brief Get the file size
 *
 * @param filepath File path
 * @return File size
 */
size_t fm_get_file_size(const char *filepath);

/**
 * @brief Create a file table
 *
 * @param list_out file list
 * @param files_number the number of files
 * @param filter_suffix File Suffix Name
 * @return ESP_OK on success
 */
esp_err_t fm_file_table_create(char ***list_out, uint16_t *files_number, const char *filter_suffix);

/**
 * @brief Free file table
 *
 * @param list file list
 * @param files_number the number of files
 * @return ESP_OK on success
 */
esp_err_t fm_file_table_free(char ***list, uint16_t files_number);

#ifdef __cplusplus
}
#endif
