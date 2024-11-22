/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lv_fs.h"

#include "lvgl.h"

static char *TAG = "lv_fs";

typedef struct {
    const char *name;
    const uint8_t *data;    // asset_mem
    size_t size;            // asset_size
} file_descriptor_t;

typedef struct {
    int file_count;
    file_descriptor_t **desc;
    mmap_assets_handle_t fs_assets;
    lv_fs_drv_t *fs_drv;
} file_system_t;

typedef struct {
    int fd;
    size_t pos;
    bool is_open;     // Moved flag to indicate if the file is open
} FILE_t;

static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    LV_UNUSED(drv);
    file_system_t *fs = drv->user_data;

    for (int i = 0; i < fs->file_count; i++) {
        if (strcmp(fs->desc[i]->name, path) == 0) {
            FILE_t *fp = (FILE_t *)malloc(sizeof(FILE_t));
            if (!fp) {
                return NULL;
            }
            fp->is_open = true;
            fp->fd = i;
            fp->pos = 0;
            return (void*)fp;
        }
    }

    return NULL; // file not found
}

static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    LV_UNUSED(drv);
    file_system_t *fs = drv->user_data;

    FILE_t *fp = (FILE_t *)file_p;
    if (!fp || fp->fd < 0 || fp->fd >= fs->file_count) {
        return LV_FS_RES_FS_ERR;
    }

    fp->is_open = false;
    free(fp);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    LV_UNUSED(drv);
    file_system_t *fs = drv->user_data;

    FILE_t *fp = (FILE_t *)file_p;
    if (!fp || fp->fd < 0 || fp->fd >= fs->file_count || !fp->is_open) {
        return LV_FS_RES_FS_ERR;
    }

    file_descriptor_t *file = fs->desc[fp->fd];
    if (fp->pos + btr > file->size) {
        btr = file->size - fp->pos;
    }

    mmap_assets_copy_mem(fs->fs_assets, (size_t)(file->data + fp->pos), buf, btr);
    fp->pos += btr;
    *br = btr;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    LV_UNUSED(drv);
    file_system_t *fs = drv->user_data;

    FILE_t *fp = (FILE_t *)file_p;
    if (!fp || fp->fd < 0 || fp->fd >= fs->file_count || !fp->is_open) {
        return LV_FS_RES_FS_ERR;
    }

    return LV_FS_RES_DENIED;
}

static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    LV_UNUSED(drv);
    file_system_t *fs = drv->user_data;

    FILE_t *fp = (FILE_t *)file_p;
    if (!fp || fp->fd < 0 || fp->fd >= fs->file_count || !fp->is_open) {
        return LV_FS_RES_FS_ERR;
    }

    file_descriptor_t *file = fs->desc[fp->fd];
    size_t new_pos;
    switch (whence) {
    case LV_FS_SEEK_SET:
        new_pos = pos;
        break;
    case LV_FS_SEEK_CUR:
        new_pos = fp->pos + pos;
        break;
    case LV_FS_SEEK_END:
        new_pos = file->size + pos;
        break;
    default:
        return LV_FS_RES_INV_PARAM;
    }

    if (new_pos > file->size) {
        new_pos = file->size;
    }
    fp->pos = new_pos;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    LV_UNUSED(drv);
    file_system_t *fs = drv->user_data;

    FILE_t *fp = (FILE_t *)file_p;
    if (!fp || fp->fd < 0 || fp->fd >= fs->file_count || !fp->is_open) {
        return LV_FS_RES_FS_ERR;
    }

    *pos_p = fp->pos;
    return LV_FS_RES_OK;
}

static void *fs_dir_open(lv_fs_drv_t *drv, const char *path)
{
    LV_UNUSED(drv);
    // Return an error indicating that write operations are not supported
    return NULL;
}

#if LVGL_VERSION_MAJOR >= 9
static lv_fs_res_t fs_dir_read(lv_fs_drv_t * drv, void * dir_p, char * fn, uint32_t fn_len)
#else
static lv_fs_res_t fs_dir_read(lv_fs_drv_t *drv, void *dir_p, char *fn)
#endif
{
    LV_UNUSED(drv);
    // Return an error indicating that write operations are not supported
    return LV_FS_RES_DENIED;
}

static lv_fs_res_t fs_dir_close(lv_fs_drv_t *drv, void *dir_p)
{
    LV_UNUSED(drv);
    // Return an error indicating that write operations are not supported
    return LV_FS_RES_DENIED;
}

/**
 * Register a driver for the File system interface
 */
static void lv_fs_flash_init(char fs_letter, file_system_t *handle)
{
    /*---------------------------------------------------
     * Register the file system interface in LVGL
     *--------------------------------------------------*/

    /*Add a simple drive to open images*/
    lv_fs_drv_init(handle->fs_drv);

    /*Set up fields...*/
    handle->fs_drv->letter = fs_letter;

    handle->fs_drv->open_cb = fs_open;
    handle->fs_drv->close_cb = fs_close;
    handle->fs_drv->read_cb = fs_read;
    handle->fs_drv->write_cb = fs_write;
    handle->fs_drv->seek_cb = fs_seek;
    handle->fs_drv->tell_cb = fs_tell;

    handle->fs_drv->dir_open_cb = fs_dir_open;
    handle->fs_drv->dir_read_cb = fs_dir_read;
    handle->fs_drv->dir_close_cb = fs_dir_close;

#if LVGL_VERSION_MAJOR >= 9 || LV_USE_USER_DATA
    handle->fs_drv->user_data = handle;
#else
#error "LV_USE_USER_DATA is disabled. Please enable it in lv_conf.h"
#endif
    lv_fs_drv_register(handle->fs_drv);
}

esp_err_t esp_lv_fs_desc_deinit(esp_lv_fs_handle_t handle)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    file_system_t *fs = (file_system_t*)handle;

    if (fs->fs_drv) {//fs_drv can't be deleted, you can just delete them all
        free(fs->fs_drv);
    }

    for (int i = 0; i < fs->file_count; i++) {
        free(fs->desc[i]);
    }
    if (fs->desc) {
        free(fs->desc);
    }

    free(fs);

    return ESP_OK;
}

esp_err_t esp_lv_fs_desc_init(const fs_cfg_t *cfg, esp_lv_fs_handle_t *ret_handle)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(cfg && ret_handle && cfg->fs_nums && cfg->fs_assets, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    file_system_t *fs = (file_system_t *)calloc(1, sizeof(file_system_t));
    ESP_GOTO_ON_FALSE(fs, ESP_ERR_NO_MEM, err, TAG, "no mem for fs handle");

    fs->desc = (file_descriptor_t**)calloc(1, cfg->fs_nums * sizeof(file_descriptor_t*));
    ESP_GOTO_ON_FALSE(fs, ESP_ERR_NO_MEM, err, TAG, "no mem for desc list");

    fs->fs_assets = cfg->fs_assets;
    fs->file_count = 0;

    for (int i = 0; i < cfg->fs_nums; i++) {
        fs->desc[i] = (file_descriptor_t*)calloc(1, sizeof(file_descriptor_t));
        ESP_GOTO_ON_FALSE(fs, ESP_ERR_NO_MEM, err, TAG, "no mem for file descriptor");

        fs->desc[i]->name = mmap_assets_get_name(fs->fs_assets, i);
        fs->desc[i]->data = mmap_assets_get_mem(fs->fs_assets, i);
        fs->desc[i]->size = mmap_assets_get_size(fs->fs_assets, i);

        fs->file_count++;
    }

    fs->fs_drv = (lv_fs_drv_t *)calloc(1, sizeof(lv_fs_drv_t));
    ESP_GOTO_ON_FALSE(fs->fs_drv, ESP_ERR_NO_MEM, err, TAG, "no mem for fs_drv");

    lv_fs_flash_init(cfg->fs_letter, fs);

    *ret_handle = (esp_lv_fs_handle_t)fs;
    ESP_LOGD(TAG, "new fs handle:@%p", fs);

    ESP_LOGI(TAG, "Drive '%c' successfully created, version: %d.%d.%d",
             cfg->fs_letter, ESP_LV_FS_VER_MAJOR, ESP_LV_FS_VER_MINOR, ESP_LV_FS_VER_PATCH);

    return ret;

err:
    esp_lv_fs_desc_deinit((esp_lv_fs_handle_t)fs);
    return ret;
}
