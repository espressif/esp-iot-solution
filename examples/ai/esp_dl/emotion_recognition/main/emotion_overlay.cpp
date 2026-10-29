/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "emotion_overlay.hpp"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

namespace {
constexpr char TAG[] = "emotion_overlay";
constexpr uint16_t kBoxColor = 0xF800;        // red, RGB565
constexpr uint16_t kTextBgColor = 0x0000;     // black, RGB565
constexpr uint16_t kTextPadX = 12;
constexpr uint16_t kTextPadY = 4;
constexpr uint16_t kTextY = 8;
const esp_painter_basic_font_t *kEmotionFont = &esp_painter_basic_font_40;

inline void draw_hline_rgb565(uint16_t *buf, int width, int height, int x1, int x2, int y, uint16_t color)
{
    if (y < 0 || y >= height) {
        return;
    }
    x1 = x1 < 0 ? 0 : x1;
    x2 = x2 >= width ? width - 1 : x2;
    if (x1 > x2) {
        return;
    }
    uint16_t *row = buf + y * width;
    for (int x = x1; x <= x2; ++x) {
        row[x] = color;
    }
}

inline void draw_vline_rgb565(uint16_t *buf, int width, int height, int x, int y1, int y2, uint16_t color)
{
    if (x < 0 || x >= width) {
        return;
    }
    y1 = y1 < 0 ? 0 : y1;
    y2 = y2 >= height ? height - 1 : y2;
    if (y1 > y2) {
        return;
    }
    for (int y = y1; y <= y2; ++y) {
        buf[y * width + x] = color;
    }
}

inline void fill_rect_rgb565(uint16_t *buf, int width, int height,
                             int x1, int y1, int x2, int y2, uint16_t color)
{
    x1 = x1 < 0 ? 0 : x1;
    y1 = y1 < 0 ? 0 : y1;
    x2 = x2 >= width ? width - 1 : x2;
    y2 = y2 >= height ? height - 1 : y2;
    if (x1 > x2 || y1 > y2) {
        return;
    }
    for (int y = y1; y <= y2; ++y) {
        uint16_t *row = buf + y * width;
        for (int x = x1; x <= x2; ++x) {
            row[x] = color;
        }
    }
}

void draw_boxes(cam_fb_t *fb, const EmotionPipeline::Result &result)
{
    auto *buf = static_cast<uint16_t *>(fb->buf);
    for (int i = 0; i < result.count; ++i) {
        const auto &box = result.boxes[i];
        draw_hline_rgb565(buf, fb->width, fb->height, box.x1, box.x2, box.y1, kBoxColor);
        draw_hline_rgb565(buf, fb->width, fb->height, box.x1, box.x2, box.y2, kBoxColor);
        draw_vline_rgb565(buf, fb->width, fb->height, box.x1, box.y1, box.y2, kBoxColor);
        draw_vline_rgb565(buf, fb->width, fb->height, box.x2, box.y1, box.y2, kBoxColor);
    }
}
} // namespace

EmotionOverlay::EmotionOverlay(esp_lcd_panel_handle_t panel) : m_panel(panel) {}

EmotionOverlay::~EmotionOverlay()
{
    if (m_painter) {
        esp_painter_deinit(m_painter);
        m_painter = nullptr;
    }
}

esp_err_t EmotionOverlay::ensure_painter(uint16_t width, uint16_t height)
{
    if (m_painter != nullptr) {
        return ESP_OK;
    }
    esp_painter_config_t cfg = {
        .canvas = {
            .width = width,
            .height = height,
        },
        .color_format = ESP_PAINTER_COLOR_FORMAT_RGB565,
        .default_font = kEmotionFont,
        .flags = {
            .swap_color_bytes = false,
        },
    };
    esp_err_t ret = esp_painter_init(&cfg, &m_painter);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init painter: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t EmotionOverlay::draw(cam_fb_t *fb, const EmotionPipeline::Result &result)
{
    if (fb == nullptr || fb->buf == nullptr || fb->format != VIDEO_PIX_FMT_RGB565) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ensure_painter(static_cast<uint16_t>(fb->width), static_cast<uint16_t>(fb->height));
    if (ret != ESP_OK) {
        return ret;
    }

    if (result.count > 0 && result.boxes[0].emotion_class_id >= 0 &&
            result.boxes[0].emotion_name[0] != '\0') {
        snprintf(m_cached_text, sizeof(m_cached_text), "%s %.2f",
                 result.boxes[0].emotion_name, result.boxes[0].emotion_score);
    }

    draw_boxes(fb, result);

    if (m_cached_text[0] != '\0') {
        size_t text_len = strlen(m_cached_text);
        uint16_t text_width = static_cast<uint16_t>(text_len * kEmotionFont->width);
        uint16_t x = (fb->width > text_width) ? (fb->width - text_width) / 2 : 0;

        fill_rect_rgb565(static_cast<uint16_t *>(fb->buf),
                         fb->width, fb->height,
                         static_cast<int>(x) - kTextPadX,
                         static_cast<int>(kTextY) - kTextPadY,
                         static_cast<int>(x) + text_width + kTextPadX - 1,
                         static_cast<int>(kTextY) + kEmotionFont->height + kTextPadY - 1,
                         kTextBgColor);

        esp_painter_draw_string(m_painter,
                                static_cast<uint8_t *>(fb->buf),
                                fb->width * fb->height * sizeof(uint16_t),
                                x, kTextY, kEmotionFont,
                                ESP_PAINTER_COLOR_YELLOW, ESP_PAINTER_COLOR_TRANSPARENT,
                                m_cached_text);
    }
    return esp_lcd_panel_draw_bitmap(m_panel, 0, 0, fb->width, fb->height, fb->buf);
}
