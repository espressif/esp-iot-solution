/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *fs_name;
    const char *base_path;
    const char *lvgl_image_path;
    const char *lvgl_font_path;
} fs_assets_config_t;

esp_err_t fs_backend_mount(fs_assets_config_t *config);
void fs_backend_unmount(void);

#ifdef __cplusplus
}
#endif
