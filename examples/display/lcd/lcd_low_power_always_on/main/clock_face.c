/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "clock_face.h"

#define GLYPH_WIDTH       3
#define GLYPH_HEIGHT      5
#define GLYPH_SPACING     1
#define COLON_GLYPH       10
#define CLOCK_COLOR       0x07ff // Cyan in RGB565

// Each value encodes one row of a 3 x 5 glyph, from the leftmost bit.
static const uint8_t glyphs[][GLYPH_HEIGHT] = {
    {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
    {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
    {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
    {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
    {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
    {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
    {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
    {0b111, 0b001, 0b001, 0b001, 0b001}, // 7
    {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
    {0b111, 0b101, 0b111, 0b001, 0b111}, // 9
    {0b000, 0b010, 0b000, 0b010, 0b000}, // colon
};

static void clock_glyphs(int minutes_since_midnight, int digits[5])
{
    const int hours = minutes_since_midnight / 60;
    const int minutes = minutes_since_midnight % 60;
    digits[0] = hours / 10;
    digits[1] = hours % 10;
    digits[2] = COLON_GLYPH;
    digits[3] = minutes / 10;
    digits[4] = minutes % 10;
}

void clock_face_draw(const clock_panel_t *panel, int previous_minute, int current_minute)
{
    const bool first_draw = previous_minute == CLOCK_FACE_FIRST_DRAW;
    int time_glyphs[5];
    int previous_glyphs[5];
    clock_glyphs(current_minute, time_glyphs);
    if (!first_draw) {
        clock_glyphs(previous_minute, previous_glyphs);
    }
    const int glyph_count = sizeof(time_glyphs) / sizeof(time_glyphs[0]);

    const int scale = panel->width / 32;
    const int glyph_advance = (GLYPH_WIDTH + GLYPH_SPACING) * scale;
    const int clock_width = glyph_count * glyph_advance - GLYPH_SPACING * scale;
    const int clock_height = GLYPH_HEIGHT * scale;
    const int left = (panel->width - clock_width) / 2;
    const int top = (panel->height - clock_height) / 2;

    if (first_draw) {
        memset(panel->pixels, 0, panel->width * panel->height * sizeof(*panel->pixels));
    }

    for (int glyph = 0; glyph < glyph_count; glyph++) {
        if (!first_draw && time_glyphs[glyph] == previous_glyphs[glyph]) {
            continue;
        }

        const int glyph_x = left + glyph * glyph_advance;
        for (int y = 0; y < clock_height; y++) {
            uint16_t *row = panel->pixels + (top + y) * panel->width + glyph_x;
            const uint8_t bits = glyphs[time_glyphs[glyph]][y / scale];
            for (int x = 0; x < GLYPH_WIDTH * scale; x++) {
                const int bit = GLYPH_WIDTH - 1 - x / scale;
                row[x] = (bits & (1 << bit)) ? CLOCK_COLOR : 0;
            }
        }
    }

    // Submit whole rows so the source uses the framebuffer stride.
    const int first_row = first_draw ? 0 : top;
    const int end_row = first_draw ? panel->height : top + clock_height;
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel->handle, 0, first_row, panel->width, end_row,
                                              panel->pixels + first_row * panel->width));
}
