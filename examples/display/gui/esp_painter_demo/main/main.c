/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_painter.h"
#include "lcd_init.h"

static const char *TAG = "esp_painter_demo";

#define FB_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * (EXAMPLE_LCD_BIT_PER_PIXEL / 8))

/* ── demo screens ─────────────────────────────────────────────────────────── */

static void demo_text(esp_painter_handle_t painter, uint8_t *fb)
{
    esp_painter_clear(painter, fb, FB_SIZE, ESP_PAINTER_COLOR_NAVY);

    /* Centered title using font_48 */
    uint16_t tw, th;
    esp_painter_get_text_size(&esp_painter_basic_font_48, "ESP Painter Demo", &tw, &th);
    uint16_t tx = (EXAMPLE_LCD_H_RES - tw) / 2;
    uint16_t ty = 40;
    esp_painter_draw_string(painter, fb, FB_SIZE, tx, ty,
                            &esp_painter_basic_font_48,
                            ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT,
                            "ESP Painter Demo");

    /* Subtitle with solid background */
    esp_painter_draw_string(painter, fb, FB_SIZE, tx, ty + th + 12,
                            &esp_painter_basic_font_24,
                            ESP_PAINTER_COLOR_BLACK, ESP_PAINTER_COLOR_CYAN,
                            "Text drawing with esp_painter");

    /* Colored strings, transparent background */
    uint16_t y = ty + th + 12 + 24 + 24;
    const uint32_t colors[] = {
        ESP_PAINTER_COLOR_RED, ESP_PAINTER_COLOR_GREEN,
        ESP_PAINTER_COLOR_YELLOW, ESP_PAINTER_COLOR_ORANGE,
    };
    const char *words[] = {"Red", "Green", "Yellow", "Orange"};
    uint16_t x = 80;
    for (int i = 0; i < 4; i++) {
        esp_painter_draw_string(painter, fb, FB_SIZE, x, y,
                                &esp_painter_basic_font_24,
                                colors[i], ESP_PAINTER_COLOR_TRANSPARENT,
                                words[i]);
        x += 160;
    }

    /* Formatted string */
    uint16_t fw, fh;
    esp_painter_get_text_size(&esp_painter_basic_font_24, "Canvas: 0000 x 0000", &fw, &fh);
    uint16_t fx = (EXAMPLE_LCD_H_RES - fw) / 2;
    esp_painter_draw_string_format(painter, fb, FB_SIZE, fx, y + 48,
                                   &esp_painter_basic_font_24,
                                   ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT,
                                   "Canvas: %4d x %4d", EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
}

static void demo_shapes(esp_painter_handle_t painter, uint8_t *fb)
{
    esp_painter_clear(painter, fb, FB_SIZE, ESP_PAINTER_COLOR_BLACK);

    /* Title */
    uint16_t tw, th;
    esp_painter_get_text_size(&esp_painter_basic_font_48, "Shapes Demo", &tw, &th);
    esp_painter_draw_string(painter, fb, FB_SIZE,
                            (EXAMPLE_LCD_H_RES - tw) / 2, 20,
                            &esp_painter_basic_font_48,
                            ESP_PAINTER_COLOR_GREENYELLOW, ESP_PAINTER_COLOR_TRANSPARENT,
                            "Shapes Demo");

    /* Filled rectangles — color palette row */
    uint32_t fill_colors[] = {
        ESP_PAINTER_COLOR_RED, ESP_PAINTER_COLOR_GREEN, ESP_PAINTER_COLOR_BLUE,
        ESP_PAINTER_COLOR_YELLOW, ESP_PAINTER_COLOR_MAGENTA, ESP_PAINTER_COLOR_CYAN,
    };
    uint16_t rect_w = 120, rect_h = 80;
    uint16_t row_y = 140;
    uint16_t gap = (EXAMPLE_LCD_H_RES - 6 * rect_w) / 7;
    for (int i = 0; i < 6; i++) {
        uint16_t rx = gap + i * (rect_w + gap);
        esp_painter_draw_fill_rect(painter, fb, FB_SIZE, rx, row_y, rect_w, rect_h,
                                   fill_colors[i]);
    }

    /* Hollow rectangles with increasing line_width */
    uint32_t box_colors[] = {
        ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_ORANGE, ESP_PAINTER_COLOR_PINK,
    };
    uint16_t lws[] = {1, 3, 6};
    uint16_t box_w = 200, box_h = 120;
    uint16_t row2_y = 300;
    uint16_t gap2 = (EXAMPLE_LCD_H_RES - 3 * box_w) / 4;
    for (int i = 0; i < 3; i++) {
        uint16_t bx = gap2 + i * (box_w + gap2);
        esp_painter_draw_rect(painter, fb, FB_SIZE, bx, row2_y, box_w, box_h,
                              lws[i], box_colors[i]);
        esp_painter_draw_string_format(painter, fb, FB_SIZE,
                                       bx + 4, row2_y + box_h + 4,
                                       &esp_painter_basic_font_24,
                                       ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT,
                                       "lw=%d", lws[i]);
    }
}

static void demo_detection(esp_painter_handle_t painter, uint8_t *fb)
{
    esp_painter_clear(painter, fb, FB_SIZE, ESP_PAINTER_COLOR_DARKGREY);

    /* Title */
    uint16_t tw, th;
    esp_painter_get_text_size(&esp_painter_basic_font_48, "Detection + Pose", &tw, &th);
    esp_painter_draw_string(painter, fb, FB_SIZE,
                            (EXAMPLE_LCD_H_RES - tw) / 2, 20,
                            &esp_painter_basic_font_48,
                            ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT,
                            "Detection + Pose");

    /* Skeleton bone index pairs (shared by both persons) */
    static const int bones[][2] = {
        {0, 1}, {0, 2}, {1, 2}, /* head–shoulders */
        {1, 3}, {3, 5},        /* left  arm      */
        {2, 4}, {4, 6},        /* right arm      */
        {1, 7}, {2, 8}, {7, 8}, /* torso          */
        {7, 9}, {9, 11},       /* left  leg      */
        {8, 10}, {10, 12},     /* right leg      */
    };
    int n_bones = sizeof(bones) / sizeof(bones[0]);

    /* Two persons: detection box + pose keypoints */
    struct {
        uint16_t bx, by, bw, bh;   /* detection box      */
        uint32_t box_color;
        const char *label;
        uint16_t kx[13], ky[13];   /* 13 keypoint coords */
        uint32_t skel_color;
        uint32_t pt_color;
    } persons[] = {
        {
            /* box */
            40, 110, 240, 340, ESP_PAINTER_COLOR_RED, "Person 0.94",
            /* keypoints: nose, l_shoulder, r_shoulder, l_elbow, r_elbow,
                          l_wrist, r_wrist, l_hip, r_hip,
                          l_knee, r_knee, l_ankle, r_ankle */
            { 160, 140, 180, 120, 200, 110, 210, 145, 175, 140, 180, 138, 182 },
            { 155, 195, 195, 270, 270, 340, 340, 330, 330, 390, 390, 430, 430 },
            ESP_PAINTER_COLOR_YELLOW, ESP_PAINTER_COLOR_CYAN,
        },
        {
            /* box */
            600, 120, 220, 340, ESP_PAINTER_COLOR_GREEN, "Person 0.87",
            { 710, 690, 730, 668, 752, 655, 765, 695, 725, 690, 730, 688, 732 },
            { 165, 205, 205, 272, 272, 335, 335, 318, 318, 382, 382, 438, 438 },
            ESP_PAINTER_COLOR_ORANGE, ESP_PAINTER_COLOR_PINK,
        },
    };

    for (int p = 0; p < 2; p++) {
        /* Detection box */
        esp_painter_box_style_t style = {
            .line_width = 3,
            .box_color  = persons[p].box_color,
            .font       = &esp_painter_basic_font_24,
            .text_color = persons[p].box_color,
        };
        esp_painter_draw_detection_box(painter, fb, FB_SIZE,
                                       persons[p].bx, persons[p].by,
                                       persons[p].bw, persons[p].bh,
                                       &style, persons[p].label);

        /* Skeleton */
        for (int b = 0; b < n_bones; b++) {
            int i0 = bones[b][0], i1 = bones[b][1];
            esp_painter_draw_line(painter, fb, FB_SIZE,
                                  persons[p].kx[i0], persons[p].ky[i0],
                                  persons[p].kx[i1], persons[p].ky[i1],
                                  2, persons[p].skel_color);
        }

        /* Keypoints */
        for (int k = 0; k < 13; k++) {
            esp_painter_draw_point(painter, fb, FB_SIZE,
                                   persons[p].kx[k], persons[p].ky[k],
                                   4, persons[p].pt_color);
        }
    }
}

static void demo_primitives(esp_painter_handle_t painter, uint8_t *fb)
{
    /* Dark background using ESP_PAINTER_RGB888 custom color */
    esp_painter_clear(painter, fb, FB_SIZE, ESP_PAINTER_RGB888(15, 15, 40));

    /* Title */
    uint16_t tw, th;
    esp_painter_get_text_size(&esp_painter_basic_font_48, "Lines & Points", &tw, &th);
    esp_painter_draw_string(painter, fb, FB_SIZE,
                            (EXAMPLE_LCD_H_RES - tw) / 2, 16,
                            &esp_painter_basic_font_48,
                            ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT,
                            "Lines & Points");

    /* Reserve left margin wide enough for the longest Y-axis label */
    uint16_t lw, lh;
    esp_painter_get_text_size(&esp_painter_basic_font_24, "100%", &lw, &lh);

    uint16_t cx  = (uint16_t)(lw + 14);          /* chart left  */
    uint16_t cy  = (uint16_t)(th + 28);           /* chart top   */
    uint16_t cx2 = (uint16_t)(EXAMPLE_LCD_H_RES - 20); /* chart right */
    uint16_t cy2 = (uint16_t)(EXAMPLE_LCD_V_RES - 110);/* chart bottom*/
    uint16_t cw  = cx2 - cx;
    uint16_t ch  = cy2 - cy;

    /* Horizontal grid lines + Y-axis labels (0 % … 100 %) */
    const char *ylabels[] = {"  0%", " 25%", " 50%", " 75%", "100%"};
    uint32_t grid_color = ESP_PAINTER_RGB888(50, 50, 90);
    for (int i = 0; i <= 4; i++) {
        uint16_t gy = cy2 - (uint16_t)((uint32_t)ch * i / 4);
        esp_painter_draw_line(painter, fb, FB_SIZE,
                              cx, gy, cx2, gy, 1, grid_color);
        esp_painter_draw_string(painter, fb, FB_SIZE,
                                0, (uint16_t)(gy - lh / 2),
                                &esp_painter_basic_font_24,
                                ESP_PAINTER_COLOR_LIGHTGREY, ESP_PAINTER_COLOR_TRANSPARENT,
                                ylabels[i]);
    }

    /* Axes */
    esp_painter_draw_line(painter, fb, FB_SIZE, cx, cy,  cx,  cy2, 2, ESP_PAINTER_COLOR_LIGHTGREY);
    esp_painter_draw_line(painter, fb, FB_SIZE, cx, cy2, cx2, cy2, 2, ESP_PAINTER_COLOR_LIGHTGREY);

    /* X-axis label */
    uint16_t xlw, xlh;
    esp_painter_get_text_size(&esp_painter_basic_font_24, "Frame", &xlw, &xlh);
    esp_painter_draw_string(painter, fb, FB_SIZE,
                            cx + (cw - xlw) / 2, cy2 + 8,
                            &esp_painter_basic_font_24,
                            ESP_PAINTER_COLOR_LIGHTGREY, ESP_PAINTER_COLOR_TRANSPARENT,
                            "Frame");

    /* Two data series: detection confidence over 10 frames */
    uint8_t data_a[] = { 72, 78, 85, 91, 88, 94, 90, 87, 92, 95 };
    uint8_t data_b[] = { 45, 52, 48, 60, 55, 63, 70, 65, 72, 68 };
    struct {
        uint8_t       *data;
        uint32_t       line_color;
        uint32_t       pt_color;
        const char    *name;
    } series[2] = {
        { data_a, ESP_PAINTER_COLOR_CYAN,   ESP_PAINTER_COLOR_WHITE,  "Class A" },
        { data_b, ESP_PAINTER_COLOR_ORANGE, ESP_PAINTER_COLOR_YELLOW, "Class B" },
    };

    int n = 10;
    for (int s = 0; s < 2; s++) {
        uint16_t prev_px = 0, prev_py = 0;
        for (int i = 0; i < n; i++) {
            uint16_t px = cx + (uint16_t)((uint32_t)cw * (i + 1) / (n + 1));
            uint16_t py = cy2 - (uint16_t)((uint32_t)ch * series[s].data[i] / 100);
            if (i > 0) {
                esp_painter_draw_line(painter, fb, FB_SIZE,
                                      prev_px, prev_py, px, py,
                                      2, series[s].line_color);
            }
            esp_painter_draw_point(painter, fb, FB_SIZE, px, py, 4, series[s].pt_color);
            prev_px = px;
            prev_py = py;
        }
    }

    /* Legend */
    uint16_t leg_y = cy2 + 42;
    for (int s = 0; s < 2; s++) {
        uint16_t leg_x = cx + (uint16_t)(s * 280);
        esp_painter_draw_line(painter, fb, FB_SIZE,
                              leg_x, (uint16_t)(leg_y + lh / 2),
                              (uint16_t)(leg_x + 40), (uint16_t)(leg_y + lh / 2),
                              2, series[s].line_color);
        esp_painter_draw_point(painter, fb, FB_SIZE,
                               (uint16_t)(leg_x + 20), (uint16_t)(leg_y + lh / 2),
                               4, series[s].pt_color);
        esp_painter_draw_string(painter, fb, FB_SIZE,
                                (uint16_t)(leg_x + 50), leg_y,
                                &esp_painter_basic_font_24,
                                ESP_PAINTER_COLOR_WHITE, ESP_PAINTER_COLOR_TRANSPARENT,
                                series[s].name);
    }
}

/* ── app_main ─────────────────────────────────────────────────────────────── */

void app_main(void)
{
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_panel_io_handle_t io = NULL;
    SemaphoreHandle_t refresh_done = NULL;

    ESP_ERROR_CHECK(example_lcd_init(&panel, &io, &refresh_done));

    uint8_t *fb = heap_caps_malloc(FB_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!fb) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer (%u bytes)", FB_SIZE);
        return;
    }

    esp_painter_config_t cfg = {
        .canvas = {
            .width  = EXAMPLE_LCD_H_RES,
            .height = EXAMPLE_LCD_V_RES,
        },
        .color_format = ESP_PAINTER_COLOR_FORMAT_RGB565,
        .default_font = &esp_painter_basic_font_24,
    };
    esp_painter_handle_t painter = NULL;
    ESP_ERROR_CHECK(esp_painter_init(&cfg, &painter));

    typedef void (*demo_fn_t)(esp_painter_handle_t, uint8_t *);
    demo_fn_t screens[] = {demo_text, demo_shapes, demo_detection, demo_primitives};
    int n_screens = sizeof(screens) / sizeof(screens[0]);

    ESP_LOGI(TAG, "Starting demo loop");
    int screen = 0;
    while (1) {
        screens[screen](painter, fb);
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0,
                                                  EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, fb));
        xSemaphoreTake(refresh_done, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(3000));
        screen = (screen + 1) % n_screens;
    }
}
