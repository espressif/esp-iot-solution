/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file main.c
 * @brief LVGL Lottie animation player demo
 *
 * Plays a Lottie JSON animation loaded from a flash partition.
 * Requires CONFIG_EXAMPLE_LVGL_ADAPTER_TASK_STACK_SIZE=32768 because
 * lottie rendering uses deep recursion.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include <string.h>
#include <strings.h>
#include "sdkconfig.h"
#include "example_lvgl_init.h"
#include "esp_lv_lottie.h"
#include "esp_mmap_assets.h"
#include "mmap_generate_lottie.h"

static const char *TAG = "main";

#define LOTTIE_FS_ROOT   "A:"
#define LOTTIE_PATH_MAX  128

static mmap_assets_handle_t s_lottie_assets;
static esp_lv_fs_handle_t s_fs_handle;

static bool lottie_is_json(const char *name)
{
    size_t len = strlen(name);
    if (len < 5) {
        return false;
    }
    return strcasecmp(name + len - 5, ".json") == 0;
}

static bool lottie_find_first_json(char *path, size_t path_size)
{
    int file_count = mmap_assets_get_stored_files(s_lottie_assets);
    for (int i = 0; i < file_count; i++) {
        const char *name = mmap_assets_get_name(s_lottie_assets, i);
        if (name == NULL || !lottie_is_json(name)) {
            continue;
        }
        int written = snprintf(path, path_size, "%s%s", LOTTIE_FS_ROOT, name);
        if (written > 0 && written < (int)path_size) {
            return true;
        }
    }

    return false;
}

static esp_err_t lottie_mount_assets(void)
{
    const mmap_assets_config_t cfg = {
        .partition_label = "lottie",
        .max_files = MMAP_LOTTIE_FILES,
        .checksum = MMAP_LOTTIE_CHECKSUM,
        .flags = {
            .mmap_enable = true,
        },
    };

    ESP_RETURN_ON_ERROR(mmap_assets_new(&cfg, &s_lottie_assets), TAG, "mmap assets init failed");

    const fs_cfg_t fs_cfg = {
        .fs_letter = 'A',
        .fs_nums = MMAP_LOTTIE_FILES,
        .fs_assets = s_lottie_assets,
    };

    ESP_RETURN_ON_ERROR(esp_lv_adapter_fs_mount(&fs_cfg, &s_fs_handle), TAG, "fs mount failed");
    return ESP_OK;
}

static void lottie_demo_start(lv_display_t *disp)
{
    int32_t hor_res = lv_display_get_horizontal_resolution(disp);
    int32_t ver_res = lv_display_get_vertical_resolution(disp);
    int32_t size = (hor_res < ver_res) ? hor_res : ver_res;

    lv_obj_t *screen = lv_display_get_screen_active(disp);
    lv_obj_t *lottie = lv_lottie_create(screen);
    lv_lottie_set_size(lottie, size, size);
    lv_obj_center(lottie);

    char lottie_path[LOTTIE_PATH_MAX];
    if (!lottie_find_first_json(lottie_path, sizeof(lottie_path))) {
        ESP_LOGE(TAG, "No lottie assets found");
        return;
    }

    lv_lottie_set_src(lottie, lottie_path);
    if (!lv_lottie_is_loaded(lottie)) {
        ESP_LOGE(TAG, "Failed to load lottie asset: %s", lottie_path);
        return;
    }

    lv_lottie_set_loop_enabled(lottie, true);
    lv_lottie_play(lottie);
}

void app_main(void)
{
    example_lvgl_ctx_t ctx;
    ESP_ERROR_CHECK(example_lvgl_init(&ctx));

    ESP_LOGI(TAG, "Starting LVGL Lottie demo");
    ESP_ERROR_CHECK(lottie_mount_assets());

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        lottie_demo_start(ctx.disp);
        esp_lv_adapter_unlock();
    }
}
