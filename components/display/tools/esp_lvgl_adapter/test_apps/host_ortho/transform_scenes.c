/* SPDX-FileCopyrightText: 2026 youkorr
 * SPDX-License-Identifier: MIT
 * Host-only comparison of object transforms composed with display rotation.
 */
#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>
#if TEST_SUBJECT
#include "lv_draw_ortho.h"
#include "lv_draw_ortho_ppa.h"
#include "p4fake.h"
static bool object_layers = true;
#endif

#define STYLE_CASES 16

#if LVGL_VERSION_MAJOR > 9 || LVGL_VERSION_MINOR >= 6
#define rotate_pixels lv_draw_rotate
#else
#define rotate_pixels lv_draw_sw_rotate
#endif

static uint32_t tick_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

typedef struct {
    uint8_t *frame, *draw;
    uint32_t stride;
    int w, h, px;
    bool partial;
} surface_t;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    surface_t *surface = lv_display_get_user_data(display);
    if (surface->partial) {
        const int row_bytes = lv_area_get_width(area) * surface->px;
        const uint32_t stride = lv_draw_buf_width_to_stride(lv_area_get_width(area),
                                                            lv_display_get_color_format(display));
        for (int y = area->y1; y <= area->y2; y++) {
            if (y < 0 || y >= surface->h || area->x1 < 0 || area->x2 >= surface->w) {
                fprintf(stderr, "flush outside native framebuffer\n");
                exit(3);
            }
            memcpy(surface->frame + y * surface->stride + area->x1 * surface->px,
                   pixels + (y - area->y1) * stride, row_bytes);
        }
    }
    lv_display_flush_ready(display);
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    return object;
}

static lv_image_dsc_t photo;
static uint8_t *photo_data;

static void make_photo(lv_color_format_t cf)
{
    const int width = 65, height = 43;
    const int px = lv_color_format_get_size(cf);
    const uint32_t stride = lv_draw_buf_width_to_stride(width, cf);
    photo_data = calloc(height, stride);
    for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) {
            unsigned r = (x * 5 + y * 3) & 255;
            unsigned g = (y * 7 + 17) & 255;
            unsigned b = (x * 3 + 90) & 255;
            uint8_t *p = photo_data + y * stride + x * px;
            if (cf == LV_COLOR_FORMAT_RGB565) {
                uint16_t v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                memcpy(p, &v, 2);
            } else {
                p[0] = b;
                p[1] = g;
                p[2] = r;
                p[3] = 255;
            }
        }
    memset(&photo, 0, sizeof(photo));
    photo.header.magic = LV_IMAGE_HEADER_MAGIC;
    photo.header.cf = cf;
    photo.header.w = width;
    photo.header.h = height;
    photo.header.stride = stride;
    photo.data_size = stride * height;
    photo.data = photo_data;
}

static void transform(lv_obj_t *object, int angle, int scale_x, int scale_y)
{
    lv_obj_set_style_transform_pivot_x(object, 40, 0);
    lv_obj_set_style_transform_pivot_y(object, 25, 0);
    lv_obj_set_style_transform_rotation(object, angle, 0);
    lv_obj_set_style_transform_scale_x(object, scale_x, 0);
    lv_obj_set_style_transform_scale_y(object, scale_y, 0);
}

static const char *names[] = {
    "control", "image-rot30", "image-rot90", "image-scale", "image-rot30-scale",
    "object-rot30", "object-rot90", "object-scale", "object-rot30-scale",
    "parent-rot30", "parent-scale", "nested-rotations", "nested-scales",
    "layered-object-rot30", "layered-parent-child", "clipped-object-rot30",
    "custom-translation", "custom-quarter90"
};

static void scene(lv_obj_t *screen, int test, int width, int height)
{
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x14243a), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    /* A fixed untransformed marker makes a wrong display mapping evident. */
    box(screen, 5, 7, 23, 11, 0xffffff);
    if (test >= STYLE_CASES) {
        lv_obj_t *object = box(screen, 100, 200, 80, 40, 0xd52e45);
        lv_matrix_t matrix;
        lv_matrix_identity(&matrix);
        if (test == STYLE_CASES) {
            lv_matrix_translate(&matrix, 13, 7);
        } else {
            lv_matrix_rotate(&matrix, 90);
        }
        lv_obj_set_transform(object, &matrix);
        return;
    }
    if (test >= 1 && test <= 4) {
        lv_obj_t *image = lv_image_create(screen);
        lv_image_set_src(image, &photo);
        lv_obj_set_pos(image, width / 2 - 32, height / 2 - 21);
        lv_image_set_pivot(image, 21, 13);
        if (test == 1 || test == 4) {
            lv_image_set_rotation(image, 300);
        }
        if (test == 2) {
            lv_image_set_rotation(image, 900);
        }
        if (test == 3 || test == 4) {
            lv_image_set_scale_x(image, 384);
            lv_image_set_scale_y(image, 192);
        }
        return;
    }
    lv_obj_t *parent = screen;
    int x = width / 2 - 60, y = height / 2 - 40;
    if (test == 15) {
        parent = box(screen, x, y, 90, 65, 0x386078);
        x = 36; y = 20;
    }
    lv_obj_t *object = box(parent, x, y, 120, 80, 0xd52e45);
    box(object, 7, 9, 22, 13, 0x30e09a);
    box(object, 73, 42, 35, 28, 0x3a88ff);
    lv_obj_t *label = lv_label_create(object);
    lv_label_set_text(label, "Ab7");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_pos(label, 35, 8);
    if (test == 0) {
        return;
    }
    switch (test) {
    case 5: case 9: case 13: case 14: case 15:
        transform(object, 300, 256, 256); break;
    case 6: transform(object, 900, 256, 256); break;
    case 7: case 10: transform(object, 0, 384, 192); break;
    case 8: transform(object, 300, 384, 192); break;
    case 11: transform(object, 300, 256, 256); break;
    case 12: transform(object, 0, 384, 256); break;
    }
    if ((test >= 9 && test <= 12) || test == 14) {
        lv_obj_t *child = box(object, 28, 27, 58, 35, 0xf4d85a);
        box(child, 3, 3, 14, 9, 0x9338ca);
        if (test == 11 || test == 14) {
            transform(child, 900, 320, 192);
        }
        if (test == 12) {
            transform(child, 0, 192, 320);
        }
    }
    if (test == 13 || test == 14) {
        lv_obj_set_style_opa_layered(object, 160, 0);
    }
}

static void render(const char *folder, int test, int nw, int nh, lv_color_format_t cf,
                   lv_display_rotation_t rotation, int degrees, bool partial)
{
    const bool quarter = degrees == 90 || degrees == 270;
    const int lw = quarter ? nh : nw, lh = quarter ? nw : nh;
#if TEST_SUBJECT
    const int width = nw, height = nh;
#else
    const int width = lw, height = lh;
#endif
    surface_t surface = {.w = width, .h = height,
                         .px = lv_color_format_get_size(cf), .partial = partial
                        };
    surface.stride = lv_draw_buf_width_to_stride(width, cf);
    const size_t bytes = surface.stride * height;
    surface.frame = aligned_alloc(128, (bytes + 127) / 128 * 128);
    memset(surface.frame, 0x5a, bytes);
    lv_display_t *display = lv_display_create(width, height);
    lv_display_set_color_format(display, cf);
    lv_display_set_user_data(display, &surface);
    if (partial) {
        const size_t band = surface.stride * 37;
        surface.draw = aligned_alloc(128, (band + 127) / 128 * 128);
        lv_display_set_buffers(display, surface.draw, NULL, band, LV_DISPLAY_RENDER_MODE_PARTIAL);
    } else {
        lv_display_set_buffers_with_stride(display, surface.frame, NULL, bytes, surface.stride,
                                           LV_DISPLAY_RENDER_MODE_DIRECT);
    }
    lv_display_set_flush_cb(display, flush);
#if TEST_SUBJECT
    if (lv_display_get_software_object_transforms(display)) {
        exit(8);
    }
    lv_display_set_software_object_transforms(display, object_layers);
    if (lv_display_get_software_object_transforms(display) != object_layers) {
        exit(9);
    }
    lv_display_set_rotation(display, rotation);
    lv_display_set_matrix_rotation(display, true);
    lv_draw_ortho_reset_stats();
#endif
    lv_display_set_default(display);
    scene(lv_display_get_screen_active(display), test, lw, lh);
    lv_refr_now(display);
    const uint32_t nstride = lv_draw_buf_width_to_stride(nw, cf);
    uint8_t *native = surface.frame;
#if !TEST_SUBJECT
    native = calloc(nh, nstride);
    if (degrees == 0) {
        memcpy(native, surface.frame, nstride * nh);
    } else {
        rotate_pixels(surface.frame, native, lw, lh, surface.stride, nstride, rotation, cf);
    }
#endif
    char name[1024];
    snprintf(name, sizeof(name), "%s/%s-%dx%d-%d-r%d-%s.bin.gz", folder, names[test], nw, nh,
             surface.px * 8, degrees, partial ? "partial" : "direct");
    gzFile output = gzopen(name, "wb9");
    if (!output) {
        perror(name);
        exit(4);
    }
    if (gzwrite(output, native, nstride * nh) != (int)(nstride * nh)) {
        exit(6);
    }
    gzclose(output);
#if TEST_SUBJECT
    const lv_draw_ortho_stats_t *stats = lv_draw_ortho_get_stats();
    printf("%s,%d,%d,%d,%d,%s,%u,%u,%u,%u,%d,%d,%llu,%d\n", names[test], nw, nh, surface.px * 8,
           degrees, partial ? "partial" : "direct", stats->tasks_general,
           stats->fills_direct, stats->images_turned, stats->tasks_refused,
           g_p4_runs, g_p4_fills, g_p4_pixels, p4fake_pending());
#else
    free(native);
#endif
    lv_display_delete(display);
    free(surface.draw); free(surface.frame);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        return 2;
    }
    lv_init(); lv_tick_set_cb(tick_ms);
#if TEST_SUBJECT
    lv_draw_ortho_init();
    if (argc > 2 && strcmp(argv[2], "cores") == 0 && !lv_draw_ortho_set_cores(2)) {
        return 5;
    }
    if (argc > 2 && strcmp(argv[2], "ppa") == 0 && lv_draw_ortho_ppa_init() != ESP_OK) {
        return 7;
    }
#endif
    const lv_display_rotation_t rotations[] = {LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_90,
                                               LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_270
                                              };
    const int degrees[] = {0, 90, 180, 270};
    const lv_color_format_t formats[] = {LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_XRGB8888};
#if TEST_SUBJECT
    if (argc == 7 && strcmp(argv[1], "--custom") == 0) {
        const int type = atoi(argv[3]), angle = atoi(argv[4]), bits = atoi(argv[5]);
        if (type < 0 || type > 1 || angle < 0 || angle > 270 || angle % 90 || (bits != 16 && bits != 32)) {
            return 10;
        }
        object_layers = atoi(argv[6]) != 0;
        render(argv[2], STYLE_CASES + type, 320, 480, formats[bits == 32], rotations[angle / 90], angle, false);
        lv_deinit();
        return 0;
    }
    if (argc > 2 && strcmp(argv[2], "no-policy") == 0) {
        object_layers = false;
    }
#endif
    const int selected = argc > 3 ? atoi(argv[3]) : -1;
    int index = 0;
    for (int cf = 0; cf < 2; cf++) {
        make_photo(formats[cf]);
        for (int r = 0; r < 4; r++) for (int mode = 0; mode < 2; mode++)
                for (unsigned test = 0; test < STYLE_CASES; test++, index++) {
                    if (selected < 0 || selected == index) {
                        render(argv[1], test, 320, 480, formats[cf], rotations[r], degrees[r], mode != 0);
                    }
                }
        lv_image_cache_drop(&photo);
        free(photo_data);
    }
    lv_deinit();
    return 0;
}
