/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IOT_FILE_MANAGER_H_
#define _IOT_FILE_MANAGER_H_

#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPIFFS_MOUNT_POINT  "/spiffs"
#define SD_CARD_MOUNT_POINT "/sdcard"

esp_err_t fm_sdcard_init(void);
esp_err_t fm_spiffs_init(void);
void fm_print_dir(char *direntName, int level);
const char *fm_get_basepath(void);
const char *fm_get_filename(const char *file);
size_t fm_get_file_size(const char *filepath);
esp_err_t fm_file_table_create(char ***list_out, uint16_t *files_number, const char *filter_suffix);
esp_err_t fm_file_table_free(char ***list, uint16_t files_number);
esp_err_t fm_unmount_sdcard(void);

#ifdef __cplusplus
}
#endif

#endif
