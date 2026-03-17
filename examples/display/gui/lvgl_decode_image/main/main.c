/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file main.c
 * @brief LVGL image decoding showcase
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "sdkconfig.h"
#include "example_lvgl_init.h"
#include "lvgl.h"
#include "esp_lv_decoder.h"
#include "esp_mmap_assets.h"

#include "mmap_generate_spng.h"
#include "mmap_generate_sjpg.h"

#if !CONFIG_IDF_TARGET_ESP32C3
#include "mmap_generate_jpg.h"
#include "mmap_generate_png.h"
#include "mmap_generate_qoi.h"
#endif

#if CONFIG_IDF_TARGET_ESP32P4
#include "mmap_generate_pjpg.h"
#endif

static const char *TAG = "decode_demo";

/* Encoder threshold configuration */
#define ENCODER_ROTATION_THRESHOLD    3

typedef struct {
    const char *label;
    const char *partition;
    char drive;
    int max_files;
    uint32_t checksum;
    bool available;
    mmap_assets_handle_t assets;
    esp_lv_fs_handle_t fs_handle;
    size_t file_count;
    size_t current_index;
    char path[64];
} image_format_ctx_t;

typedef struct {
    image_format_ctx_t *formats;
    size_t format_count;
    size_t current_format;
    lv_obj_t *img;
    lv_obj_t *label;
    lv_timer_t *timer;
    int32_t encoder_accumulator;
} image_player_ctx_t;

static image_format_ctx_t s_formats[] = {
#if !CONFIG_IDF_TARGET_ESP32C3
    { .label = "JPG",  .partition = "jpg",  .drive = 'J', .max_files = MMAP_JPG_FILES,  .checksum = MMAP_JPG_CHECKSUM },
    { .label = "PNG",  .partition = "png",  .drive = 'P', .max_files = MMAP_PNG_FILES,  .checksum = MMAP_PNG_CHECKSUM },
    { .label = "QOI",  .partition = "qoi",  .drive = 'Q', .max_files = MMAP_QOI_FILES,  .checksum = MMAP_QOI_CHECKSUM },
#endif
    { .label = "SPNG", .partition = "spng", .drive = 'S', .max_files = MMAP_SPNG_FILES, .checksum = MMAP_SPNG_CHECKSUM },
    { .label = "SJPG", .partition = "sjpg", .drive = 'T', .max_files = MMAP_SJPG_FILES, .checksum = MMAP_SJPG_CHECKSUM },
#if CONFIG_IDF_TARGET_ESP32P4
    { .label = "PJPG", .partition = "pjpg", .drive = 'U', .max_files = MMAP_PJPG_FILES, .checksum = MMAP_PJPG_CHECKSUM },
#endif
};

static image_player_ctx_t s_player = {
    .formats = s_formats,
    .format_count = sizeof(s_formats) / sizeof(s_formats[0]),
};

static bool next_available_format(image_player_ctx_t *player)
{
    for (size_t i = 0; i < player->format_count; i++) {
        player->current_format = (player->current_format + 1) % player->format_count;
        if (player->formats[player->current_format].available) {
            return true;
        }
    }
    return false;
}

static void update_format_label(image_player_ctx_t *player)
{
    image_format_ctx_t *fmt = &player->formats[player->current_format];
    char buf[48];
    snprintf(buf, sizeof(buf), "%s (%u files)", fmt->label, (unsigned)fmt->file_count);
    lv_label_set_text(player->label, buf);
}

static void show_next_frame(image_player_ctx_t *player)
{
    image_format_ctx_t *fmt = &player->formats[player->current_format];
    if (!fmt->available || fmt->file_count == 0) {
        return;
    }

    const char *name = mmap_assets_get_name(fmt->assets, fmt->current_index);
    if (!name) {
        return;
    }

    snprintf(fmt->path, sizeof(fmt->path), "%c:%s", fmt->drive, name);
    lv_img_set_src(player->img, fmt->path);

    fmt->current_index = (fmt->current_index + 1) % fmt->file_count;
}

static void image_timer_cb(lv_timer_t *timer)
{
#if LVGL_VERSION_MAJOR >= 9
    image_player_ctx_t *player = (image_player_ctx_t *)lv_timer_get_user_data(timer);
#else
    image_player_ctx_t *player = (image_player_ctx_t *)timer->user_data;
#endif
    show_next_frame(player);
}

static void on_format_button(lv_event_t *e)
{
    image_player_ctx_t *player = (image_player_ctx_t *)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "Button clicked");
        if (!next_available_format(player)) {
            ESP_LOGW(TAG, "No other format available");
            return;
        }
        image_format_ctx_t *fmt = &player->formats[player->current_format];
        fmt->current_index = 0;
        update_format_label(player);
        show_next_frame(player);
    }
#if HW_USE_ENCODER
    else if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        ESP_LOGI(TAG, "Key event: %lu", key);

        if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
            player->encoder_accumulator += (key == LV_KEY_RIGHT) ? 1 : -1;
            ESP_LOGI(TAG, "Accumulator: %ld", player->encoder_accumulator);

            if (player->encoder_accumulator >= ENCODER_ROTATION_THRESHOLD ||
                    player->encoder_accumulator <= -ENCODER_ROTATION_THRESHOLD) {
                ESP_LOGI(TAG, "Switching format");
                if (next_available_format(player)) {
                    image_format_ctx_t *fmt = &player->formats[player->current_format];
                    fmt->current_index = 0;
                    update_format_label(player);
                    show_next_frame(player);
                }
                player->encoder_accumulator = 0;
            }
        }
    }
#endif
}

static esp_err_t mount_format(image_format_ctx_t *fmt)
{
    mmap_assets_config_t cfg = {
        .partition_label = fmt->partition,
        .max_files = fmt->max_files,
        .checksum = fmt->checksum,
        .flags = {
            .mmap_enable = 1,
        },
    };

    ESP_RETURN_ON_ERROR(mmap_assets_new(&cfg, &fmt->assets), TAG, "mmap init failed");
    fmt->file_count = mmap_assets_get_stored_files(fmt->assets);
    if (fmt->file_count == 0) {
        mmap_assets_del(fmt->assets);
        fmt->assets = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    fs_cfg_t fs_cfg = {
        .fs_letter = fmt->drive,
        .fs_nums = fmt->file_count,
        .fs_assets = fmt->assets,
    };

    esp_err_t ret = esp_lv_adapter_fs_mount(&fs_cfg, &fmt->fs_handle);
    if (ret != ESP_OK) {
        mmap_assets_del(fmt->assets);
        fmt->assets = NULL;
        return ret;
    }
    fmt->available = true;
    ESP_LOGI(TAG, "%s ready (%u files)", fmt->label, (unsigned)fmt->file_count);
    return ESP_OK;
}

static void prepare_formats(void)
{
    s_player.current_format = 0;
    for (size_t i = 0; i < s_player.format_count; i++) {
        esp_err_t ret = mount_format(&s_player.formats[i]);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Skip format %s (%s)", s_player.formats[i].label, esp_err_to_name(ret));
        }
    }
}

static void start_image_player(lv_display_t *disp, lv_indev_t *encoder)
{
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "LVGL lock failed");
        return;
    }

    lv_obj_t *screen = lv_disp_get_scr_act(disp);

    lv_coord_t hres = lv_display_get_horizontal_resolution(disp);
    lv_coord_t vres = lv_display_get_vertical_resolution(disp);
    bool compact = (hres <= 240) || (vres <= 240);

    s_player.label = lv_label_create(screen);
    lv_obj_set_width(s_player.label, LV_PCT(100));
    lv_obj_set_style_text_align(s_player.label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_player.label, LV_ALIGN_TOP_MID, 0, compact ? 2 : 10);

    s_player.img = lv_img_create(screen);
    lv_obj_align(s_player.img, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn = lv_btn_create(screen);
    lv_obj_set_style_pad_hor(btn, compact ? 8 : 12, 0);
    lv_obj_set_style_pad_ver(btn, compact ? 4 : 8, 0);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, compact ? -4 : -20);
    lv_obj_add_event_cb(btn, on_format_button, LV_EVENT_CLICKED, &s_player);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Next Format");
    lv_obj_center(btn_label);

    if (encoder) {
        lv_group_t *group = lv_group_create();

#if HW_USE_ENCODER
        lv_obj_t *hidden = lv_obj_create(screen);
        lv_obj_set_size(hidden, 1, 1);
        lv_obj_add_flag(hidden, LV_OBJ_FLAG_HIDDEN);
        lv_group_add_obj(group, hidden);

        lv_obj_add_event_cb(btn, on_format_button, LV_EVENT_KEY, &s_player);
#endif

        lv_group_add_obj(group, btn);
        lv_group_focus_obj(btn);
        lv_group_set_editing(group, true);
        lv_indev_set_group(encoder, group);
    }

    if (!s_player.timer) {
        s_player.timer = lv_timer_create(image_timer_cb, 50, &s_player);
    }

    if (compact) {
        lv_obj_set_style_bg_opa(s_player.label, LV_OPA_60, 0);
        lv_obj_set_style_bg_color(s_player.label, lv_color_hex(0x303030), 0);
        lv_obj_set_style_text_color(s_player.label, lv_color_hex(0xFFFFFF), 0);
    }

    esp_lv_adapter_unlock();

    if (!s_player.formats[s_player.current_format].available) {
        if (!next_available_format(&s_player)) {
            ESP_LOGE(TAG, "No available image format");
            return;
        }
    }

    s_player.formats[s_player.current_format].current_index = 0;

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        update_format_label(&s_player);
        show_next_frame(&s_player);
        esp_lv_adapter_unlock();
    }
}

void app_main(void)
{
    example_lvgl_ctx_t ctx;
    ESP_ERROR_CHECK(example_lvgl_init(&ctx));

    prepare_formats();
    start_image_player(ctx.disp, ctx.encoder);
}
