/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

/* Optional LVGL adapter for the GUI-independent player core. */
#include <stdlib.h>
#include <string.h>

#include "eaf_port.h"
#include "lv_eaf_private.h"

#if LVGL_VERSION_MAJOR >= 9
#include "src/core/lv_obj_class_private.h"
#include "src/misc/cache/lv_cache.h"
#include "src/misc/lv_timer_private.h"
typedef lv_result_t lv_eaf_event_res_t;
typedef lv_image_dsc_t lv_eaf_src_dsc_t;
#define LV_EAF_RESULT_OK LV_RESULT_OK
#define LV_EAF_SEND_EVENT(obj, code, param) lv_obj_send_event((obj), (code), (param))
#define LV_EAF_CACHE_DROP(src) lv_image_cache_drop((src))
#define LV_EAF_GET_SRC(obj) lv_image_get_src((obj))
#define LV_EAF_SET_SRC(obj, src) lv_image_set_src((obj), (src))
#define LV_EAF_SRC_GET_TYPE(src) lv_image_src_get_type(src)
#define LV_EAF_SRC_TYPE_VARIABLE LV_IMAGE_SRC_VARIABLE
#define LV_EAF_SRC_TYPE_FILE LV_IMAGE_SRC_FILE
#else
typedef lv_res_t lv_eaf_event_res_t;
typedef lv_img_dsc_t lv_eaf_src_dsc_t;
#define LV_EAF_RESULT_OK LV_RES_OK
#define LV_EAF_SEND_EVENT(obj, code, param) lv_event_send((obj), (code), (param))
#define LV_EAF_CACHE_DROP(src) lv_img_cache_invalidate_src((src))
#define LV_EAF_GET_SRC(obj) lv_img_get_src((obj))
#define LV_EAF_SET_SRC(obj, src) lv_img_set_src((obj), (src))
#define LV_EAF_SRC_GET_TYPE(src) lv_img_src_get_type(src)
#define LV_EAF_SRC_TYPE_VARIABLE LV_IMG_SRC_VARIABLE
#define LV_EAF_SRC_TYPE_FILE LV_IMG_SRC_FILE
#endif

#define MY_CLASS (&lv_eaf_class)
#define DEFAULT_FRAME_DELAY 0

static void lv_eaf_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_eaf_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void next_frame_task_cb(lv_timer_t *timer);
static esp_err_t load_eaf_from_data(lv_eaf_t *eaf, const uint8_t *data, size_t size);
static esp_err_t decode_current_frame(lv_eaf_t *eaf);

#if LVGL_VERSION_MAJOR >= 9
const lv_obj_class_t lv_eaf_class = {
    .constructor_cb = lv_eaf_constructor,
    .destructor_cb = lv_eaf_destructor,
    .instance_size = sizeof(lv_eaf_t),
    .base_class = LV_EAF_BASE_CLASS,
    .name = "lv_eaf",
};
#else
const lv_obj_class_t lv_eaf_class = {
    .base_class = LV_EAF_BASE_CLASS,
    .constructor_cb = lv_eaf_constructor,
    .destructor_cb = lv_eaf_destructor,
    .instance_size = sizeof(lv_eaf_t),
};
#endif

static void unload(lv_obj_t *obj)
{
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    lv_timer_pause(eaf->timer);
    if (eaf->player) {
        if (LV_EAF_GET_SRC(obj)) {
            LV_EAF_CACHE_DROP(LV_EAF_GET_SRC(obj));
        }
        esp_eaf_player_close(eaf->player);
        eaf->player = NULL;
    }
    free(eaf->file_data);
    eaf->file_data = NULL;
    eaf->imgdsc.data = NULL;
    eaf->current_frame = 0;
    eaf->total_frames = 0;
}

lv_obj_t *lv_eaf_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_eaf_set_src(lv_obj_t *obj, const void *src)
{
    if (!obj || !src) {
        return;
    }
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    unload(obj);

    esp_err_t err = ESP_ERR_INVALID_ARG;
    if (LV_EAF_SRC_GET_TYPE(src) == LV_EAF_SRC_TYPE_VARIABLE) {
        const lv_eaf_src_dsc_t *dsc = src;
        err = load_eaf_from_data(eaf, dsc->data, dsc->data_size);
    } else if (LV_EAF_SRC_GET_TYPE(src) == LV_EAF_SRC_TYPE_FILE) {
        lv_fs_file_t file;
        const char *path = src;
        if (lv_fs_open(&file, path, LV_FS_MODE_RD) != LV_FS_RES_OK) {
            return;
        }
        uint32_t size = 0, read = 0;
        lv_fs_res_t fs_err = lv_fs_seek(&file, 0, LV_FS_SEEK_END);
        if (fs_err == LV_FS_RES_OK) {
            fs_err = lv_fs_tell(&file, &size);
        }
        if (fs_err == LV_FS_RES_OK) {
            fs_err = lv_fs_seek(&file, 0, LV_FS_SEEK_SET);
        }
        if (fs_err != LV_FS_RES_OK || size == 0) {
            lv_fs_close(&file);
            return;
        }
        eaf->file_data = eaf_alloc(size);
        if (!eaf->file_data) {
            lv_fs_close(&file);
            return;
        }
        fs_err = lv_fs_read(&file, eaf->file_data, size, &read);
        lv_fs_close(&file);
        if (fs_err == LV_FS_RES_OK && read == size) {
            err = load_eaf_from_data(eaf, eaf->file_data, size);
        }
        if (err != ESP_OK) {
            free(eaf->file_data);
            eaf->file_data = NULL;
        }
    }
    if (err != ESP_OK) {
        LV_LOG_WARN("Couldn't load EAF source (%d)", (int)err);
        return;
    }
    eaf->last_call = lv_tick_get();
    LV_EAF_SET_SRC(obj, &eaf->imgdsc);
    lv_timer_resume(eaf->timer);
    lv_timer_reset(eaf->timer);
}

void lv_eaf_set_src_data(lv_obj_t *obj, const void *data, size_t len)
{
    if (!data || len == 0) {
        return;
    }
    lv_eaf_src_dsc_t dsc = {0};
#if LVGL_VERSION_MAJOR >= 9
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_RAW;
    dsc.header.w = dsc.header.h = 1;
#else
    dsc.header.always_zero = 0;
    dsc.header.cf = LV_IMG_CF_RAW;
    dsc.header.w = dsc.header.h = 1;
#endif
    dsc.data = data;
    dsc.data_size = len;
    lv_eaf_set_src(obj, &dsc);
}

void lv_eaf_set_color_key(lv_obj_t *obj, int32_t color_key)
{
    ((lv_eaf_t *)obj)->color_key = color_key;
}

void lv_eaf_restart(lv_obj_t *obj)
{
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    if (!eaf->player) {
        return;
    }
    eaf->current_frame = 0;
    eaf->loops_remaining = eaf->loop_count;
    eaf->last_call = lv_tick_get();
    if (decode_current_frame(eaf) != ESP_OK) {
        lv_timer_pause(eaf->timer);
        return;
    }
    lv_obj_invalidate(obj);
    lv_timer_resume(eaf->timer);
    lv_timer_reset(eaf->timer);
}

void lv_eaf_pause(lv_obj_t *obj)
{
    lv_timer_pause(((lv_eaf_t *)obj)->timer);
}

void lv_eaf_resume(lv_obj_t *obj)
{
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    if (eaf->player) {
        lv_timer_resume(eaf->timer);
    }
}

bool lv_eaf_is_loaded(lv_obj_t *obj)
{
    return ((lv_eaf_t *)obj)->player != NULL;
}
int32_t lv_eaf_get_loop_count(lv_obj_t *obj)
{
    return ((lv_eaf_t *)obj)->loop_count;
}
void lv_eaf_set_loop_count(lv_obj_t *obj, int32_t count)
{
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    eaf->loop_count = count;
    eaf->loops_remaining = count;
}
int32_t lv_eaf_get_total_frames(lv_obj_t *obj)
{
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    return eaf->player ? eaf->total_frames : -1;
}
int32_t lv_eaf_get_current_frame(lv_obj_t *obj)
{
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    return eaf->player ? eaf->current_frame : -1;
}
void lv_eaf_set_frame_delay(lv_obj_t *obj, uint32_t delay)
{
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    eaf->frame_delay_explicit = true;
    eaf->frame_delay = delay;
    lv_timer_set_period(eaf->timer, delay ? delay : 1);
}
uint32_t lv_eaf_get_frame_delay(lv_obj_t *obj)
{
    return ((lv_eaf_t *)obj)->frame_delay;
}
void lv_eaf_set_loop_enabled(lv_obj_t *obj, bool enable)
{
    ((lv_eaf_t *)obj)->loop_enabled = enable;
}
bool lv_eaf_get_loop_enabled(lv_obj_t *obj)
{
    return ((lv_eaf_t *)obj)->loop_enabled;
}

static void lv_eaf_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    eaf->player = NULL;
    eaf->file_data = NULL;
    eaf->color_key = -1;
    eaf->current_frame = 0;
    eaf->total_frames = 0;
    eaf->frame_delay = DEFAULT_FRAME_DELAY;
    eaf->loop_enabled = true;
    eaf->loop_count = -1;
    eaf->loops_remaining = -1;
    eaf->frame_delay_explicit = false;
    eaf->timer = lv_timer_create(next_frame_task_cb, 1, obj);
    lv_timer_pause(eaf->timer);
}

static void lv_eaf_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj)
{
    LV_UNUSED(class_p);
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    unload(obj);
#if LVGL_VERSION_MAJOR >= 9
    lv_timer_delete(eaf->timer);
#else
    lv_timer_del(eaf->timer);
#endif
}

static void next_frame_task_cb(lv_timer_t *timer)
{
    lv_obj_t *obj = timer->user_data;
    lv_eaf_t *eaf = (lv_eaf_t *)obj;
    if (lv_tick_elaps(eaf->last_call) < eaf->frame_delay) {
        return;
    }
    eaf->last_call = lv_tick_get();
    eaf->current_frame++;
    if (eaf->current_frame >= eaf->total_frames) {
        if (eaf->loop_enabled && (eaf->loops_remaining == -1 || eaf->loops_remaining > 0)) {
            eaf->current_frame = 0;
            if (eaf->loops_remaining > 0) {
                eaf->loops_remaining--;
            }
        } else {
            lv_timer_pause(timer);
            lv_eaf_event_res_t result = LV_EAF_SEND_EVENT(obj, LV_EVENT_READY, NULL);
            if (result != LV_EAF_RESULT_OK) {
                return;
            }
            return;
        }
    }
    if (decode_current_frame(eaf) != ESP_OK) {
        lv_timer_pause(timer);
        return;
    }
    LV_EAF_CACHE_DROP(LV_EAF_GET_SRC(obj));
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(eaf->player);
    const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(eaf->player);
    if (!info || !frame) {
        lv_obj_invalidate(obj);
        return;
    }
    if (!frame->dirty.valid) {
        return;
    }
    if (frame->dirty.x1 == 0 && frame->dirty.y1 == 0 &&
            frame->dirty.x2 >= info->width - 1 && frame->dirty.y2 >= info->height - 1) {
        lv_obj_invalidate(obj);
        return;
    }
    lv_area_t area = {
        .x1 = obj->coords.x1 + frame->dirty.x1,
        .y1 = obj->coords.y1 + frame->dirty.y1,
        .x2 = obj->coords.x1 + frame->dirty.x2,
        .y2 = obj->coords.y1 + frame->dirty.y2,
    };
    lv_obj_invalidate_area(obj, &area);
}

static esp_err_t load_eaf_from_data(lv_eaf_t *eaf, const uint8_t *data, size_t size)
{
    esp_eaf_player_config_t config = ESP_EAF_PLAYER_CONFIG_DEFAULT();
    config.color_key = eaf->color_key;
    esp_err_t err = esp_eaf_player_open_memory(data, size, &config, &eaf->player);
    if (err != ESP_OK) {
        return err;
    }
    const esp_eaf_stream_info_t *info = esp_eaf_player_get_info(eaf->player);
    const esp_eaf_frame_t *frame = esp_eaf_player_get_frame(eaf->player);
    if (!info || !frame) {
        return ESP_FAIL;
    }
    eaf->total_frames = info->frame_count;
    eaf->loops_remaining = eaf->loop_count;
    if (!eaf->frame_delay_explicit) {
        eaf->frame_delay = info->period_ms;
    }
    lv_timer_set_period(eaf->timer, eaf->frame_delay ? eaf->frame_delay : 1);
    eaf->current_frame = 0;
#if LVGL_VERSION_MAJOR >= 9
    eaf->imgdsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    eaf->imgdsc.header.flags = LV_IMAGE_FLAGS_MODIFIABLE;
    eaf->imgdsc.header.cf = info->pixel_format == ESP_EAF_OUT_BGRA8888
                            ? LV_COLOR_FORMAT_ARGB8888
                            : (info->has_alpha ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565);
    eaf->imgdsc.header.stride = frame->stride;
#else
    eaf->imgdsc.header.always_zero = 0;
    eaf->imgdsc.header.cf = info->pixel_format == ESP_EAF_OUT_BGRA8888
                            ? LV_IMG_CF_TRUE_COLOR_ALPHA
                            : (info->has_alpha ? LV_IMG_CF_RGB565A8 : LV_IMG_CF_TRUE_COLOR);
#endif
    eaf->imgdsc.header.w = info->width;
    eaf->imgdsc.header.h = info->height;
    eaf->imgdsc.data = frame->pixels;
    eaf->imgdsc.data_size = frame->pixel_bytes +
                            (frame->alpha ? (size_t)info->width * info->height : 0);
    return ESP_OK;
}

static esp_err_t decode_current_frame(lv_eaf_t *eaf)
{
    esp_err_t err = esp_eaf_player_seek(eaf->player, eaf->current_frame);
    if (err == ESP_OK) {
        eaf->imgdsc.data = esp_eaf_player_get_frame(eaf->player)->pixels;
    }
    return err;
}
