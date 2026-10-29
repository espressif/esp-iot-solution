/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "example_lvgl_init.h"
#include "lvgl.h"
#include "fs_backend.h"

static const char *TAG = "fs_assets_demo";
static const char *TITLE_TEXT = "乐鑫 ESP32";

static fs_assets_config_t s_assets;
static lv_font_t *s_binfont;

static int32_t clamp_dimension(int32_t value, int32_t minimum)
{
    return value < minimum ? minimum : value;
}

static const char *asset_basename(const char *path)
{
    if (path == NULL) {
        return "";
    }

    const char *name = strrchr(path, '/');
    return name ? (name + 1) : path;
}

static void create_demo_screen(lv_display_t *disp)
{
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "LVGL lock failed");
        return;
    }

    lv_obj_t *screen = lv_display_get_screen_active(disp);
    lv_coord_t hres = lv_display_get_horizontal_resolution(disp);
    lv_coord_t vres = lv_display_get_vertical_resolution(disp);
    bool compact = (hres <= 240) || (vres <= 240);

    lv_obj_set_style_bg_color(screen, lv_color_hex(0xEEE9E1), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0xD7E8F4), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);

    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_size(card, clamp_dimension(hres - 16, 180), clamp_dimension(vres - 16, 180));
    lv_obj_center(card);
    lv_obj_set_style_radius(card, compact ? 12 : 18, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFDF9), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, compact ? 12 : 24, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, compact ? 10 : 18, 0);
    if (s_binfont) {
        lv_obj_set_style_text_font(card, s_binfont, 0);
    }
    lv_obj_set_layout(card, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *badge = lv_label_create(card);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x205781), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(badge, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_pad_hor(badge, 10, 0);
    lv_obj_set_style_pad_ver(badge, 4, 0);
    lv_label_set_text_fmt(badge, "%s /storage", s_assets.fs_name);

    lv_obj_t *hint = lv_label_create(card);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x4E5E6A), 0);
    lv_label_set_text(hint, "File-backed JPEG decode and binfont load");

    lv_obj_t *image = lv_image_create(card);
    lv_obj_set_size(image, LV_PCT(100), compact ? 110 : 180);
    lv_obj_set_style_radius(image, compact ? 10 : 14, 0);
    lv_obj_set_style_clip_corner(image, true, 0);
    lv_obj_set_style_bg_color(image, lv_color_hex(0xE4F1F8), 0);
    lv_obj_set_style_bg_opa(image, LV_OPA_COVER, 0);
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
    lv_image_set_src(image, s_assets.lvgl_image_path);

    lv_obj_t *title = lv_label_create(card);
    lv_obj_set_width(title, LV_PCT(100));
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1D2A33), 0);
    lv_label_set_text(title, TITLE_TEXT);

    lv_obj_t *details = lv_label_create(card);
    lv_obj_set_width(details, LV_PCT(100));
    lv_obj_set_style_text_align(details, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(details, lv_color_hex(0x50606D), 0);
    lv_label_set_text_fmt(details,
                          "Image: %s\nFont: %s\nPath: %s",
                          asset_basename(s_assets.lvgl_image_path),
                          asset_basename(s_assets.lvgl_font_path),
                          s_assets.base_path);

    esp_lv_adapter_unlock();
}

void app_main(void)
{
    example_lvgl_ctx_t ctx;
    ESP_ERROR_CHECK(example_lvgl_init(&ctx));

    ESP_ERROR_CHECK(fs_backend_mount(&s_assets));

    ESP_LOGI(TAG, "Loading image from %s", s_assets.lvgl_image_path);
    ESP_LOGI(TAG, "Loading binfont from %s", s_assets.lvgl_font_path);

    if (esp_lv_adapter_lock(-1) == ESP_OK) {
        s_binfont = lv_binfont_create(s_assets.lvgl_font_path);
        esp_lv_adapter_unlock();
    }

    if (s_binfont == NULL) {
        ESP_LOGW(TAG, "Binfont load failed, using LVGL default font");
    }

    create_demo_screen(ctx.disp);
}
