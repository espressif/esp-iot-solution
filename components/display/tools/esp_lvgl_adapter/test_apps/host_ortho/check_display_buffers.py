# SPDX-FileCopyrightText: 2026 youkorr
# SPDX-License-Identifier: MIT
"""Run the shipped buffer-binding helpers against an already-built LVGL library.

The archive, source headers and lv_conf.h must come from the same build. No
sources are downloaded or modified. The main host runner supplies its plain
LVGL build automatically.
"""

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ADAPTER = Path(__file__).resolve().parents[2]


def function(source, name):
    """Extract a definition, preserving its actual preprocessor branches."""
    clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',
                   lambda match: ' ' * len(match.group()), source, flags=re.S)
    match = re.search(r'^static\s+\w+\s+' + re.escape(name) + r'\([^;{]*\)\s*\{', clean, re.M)
    if not match:
        raise RuntimeError('Cannot find shipped definition: ' + name)
    depth = 1
    end = match.end()
    while depth:
        depth += (clean[end] == '{') - (clean[end] == '}')
        end += 1
    return source[match.start():end]


HARNESS = r'''
#include "lvgl.h"
#include "src/display/lv_display_private.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !LV_DRAW_TRANSFORM_USE_MATRIX
#error This regression needs LVGL matrix rotation enabled
#endif

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_NO_MEM 3
#define ESP_LOGE(...) do {} while(0)
#define ESP_RETURN_ON_FALSE(test, error, ...) do { if(!(test)) return error; } while(0)
enum { ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC = 2, ESP_LV_ADAPTER_TE_PIPELINE_SYNC_FULL = 1 };
typedef enum {
    ESP_LV_ADAPTER_DISPLAY_RENDER_MODE_PARTIAL,
    ESP_LV_ADAPTER_DISPLAY_RENDER_MODE_FULL,
    ESP_LV_ADAPTER_DISPLAY_RENDER_MODE_DIRECT
} esp_lv_adapter_display_render_mode_t;
typedef struct { unsigned hor_res, ver_res; bool use_psram; } esp_lv_adapter_display_profile_t;
typedef struct {
    struct {
        esp_lv_adapter_display_profile_t profile;
        int tear_avoid_mode;
        struct { int pipeline; } te_sync;
    } base;
    void *draw_buf_primary, *draw_buf_secondary, *dummy_draw_buf_primary;
    size_t draw_buf_pixels, dummy_draw_buf_pixels, dummy_draw_buf_bytes;
    bool dummy_draw_redirected;
} esp_lv_adapter_display_runtime_config_t;
typedef struct { esp_lv_adapter_display_runtime_config_t cfg; lv_display_t *lv_disp; }
    esp_lv_adapter_display_node_t;

static void *panel[2];
static unsigned allocations;
static void check(bool condition, const char *message)
{
    if(!condition) { fprintf(stderr, "layout regression: %s\n", message); exit(33); }
}
static bool display_manager_is_panel_frame_buffer(const esp_lv_adapter_display_node_t *node, const void *p)
{ (void)node; return p && (p == panel[0] || p == panel[1]); }
static bool display_manager_draw_buffers_use_panel_framebuffers(const esp_lv_adapter_display_node_t *node)
{ return display_manager_is_panel_frame_buffer(node, node->cfg.draw_buf_primary); }
static size_t display_manager_calc_draw_buf_bytes(const esp_lv_adapter_display_profile_t *profile,
                                                  size_t pixels, lv_color_format_t color)
{ (void)profile; return pixels * lv_color_format_get_size(color); }
static void *display_manager_alloc_draw_buffer(size_t bytes, bool psram, bool dma)
{ (void)psram; (void)dma; allocations++; return malloc(bytes); }

/* Check the API's capacity preconditions before LVGL's configurable assertion
 * handler: negative controls must fail promptly instead of entering its loop.
 * Every admitted call executes the real LVGL function. */
static void checked_buffers(lv_display_t *d, void *a, void *b, uint32_t bytes, lv_display_render_mode_t mode)
{
    size_t stride = lv_draw_buf_width_to_stride(d->hor_res, lv_display_get_color_format(d));
    check(bytes >= stride * (mode == LV_DISPLAY_RENDER_MODE_PARTIAL ? 1 : d->ver_res),
          "buffer capacity is smaller than LVGL's original row layout");
    lv_display_set_buffers(d, a, b, bytes, mode);
}
static void checked_stride(lv_display_t *d, void *a, void *b, uint32_t bytes,
                           uint32_t stride, lv_display_render_mode_t mode)
{
    check(stride >= (uint32_t)d->hor_res * lv_color_format_get_size(lv_display_get_color_format(d)),
          "packed stride is smaller than the native row");
    check(bytes >= (size_t)stride * (mode == LV_DISPLAY_RENDER_MODE_PARTIAL ? 1 : d->ver_res),
          "packed stride exceeds framebuffer capacity");
    lv_display_set_buffers_with_stride(d, a, b, bytes, stride, mode);
}
#define lv_display_set_buffers checked_buffers
#define lv_display_set_buffers_with_stride checked_stride
__HELPERS__
#undef lv_display_set_buffers
#undef lv_display_set_buffers_with_stride

int main(void)
{
    const unsigned dimensions[][2] = {{720, 1280}, {1024, 600}, {321, 239}};
    const lv_color_format_t colors[] = {LV_COLOR_FORMAT_RGB565, LV_COLOR_FORMAT_RGB888,
                                        LV_COLOR_FORMAT_XRGB8888};
    unsigned cases = 0;
    lv_init();
    for(unsigned board = 0; board < 3; board++) {
        unsigned w = dimensions[board][0], h = dimensions[board][1];
        for(unsigned color = 0; color < 3; color++) for(unsigned rot = 0; rot < 4; rot++) {
            for(unsigned mode = 1; mode <= 2; mode++) {
                lv_color_format_t cf = colors[color];
                unsigned px = lv_color_format_get_size(cf);
                esp_lv_adapter_display_node_t node = {0};
                lv_display_t *d = node.lv_disp = lv_display_create(w, h);
                check(d != NULL, "display allocation");
                lv_display_set_color_format(d, cf);
                node.cfg.base.profile = (esp_lv_adapter_display_profile_t){w, h, false};
                node.cfg.draw_buf_pixels = (size_t)w * h;
                size_t bytes = (size_t)w * h * px;
                panel[0] = node.cfg.draw_buf_primary = malloc(bytes);
                panel[1] = node.cfg.draw_buf_secondary = malloc(bytes);
                check(panel[0] && panel[1], "framebuffer allocations");
                check(display_manager_set_buffers_v9(&node, d, mode), "initial panel bind");
                lv_display_set_rotation(d, rot);
                lv_display_set_matrix_rotation(d, true);
                check(lv_display_get_matrix_rotation(d), "matrix rotation enabled");
                /* Rebind after logical resolution has swapped, as after dummy draw. */
                check(display_manager_set_buffers_v9(&node, d, mode), "rotated panel rebind");
                check(d->buf_1->header.w == w && d->buf_1->header.h == h &&
                      d->buf_1->header.stride == w * px && d->buf_1->data_size == bytes,
                      "panel bind must retain its native dimensions and packed stride");
                check(d->buf_2 && d->buf_2->data == panel[1], "secondary buffer retained");
                unsigned before = allocations;
                check(display_manager_redirect_dummy_draw_buffer(&node) == ESP_OK, "dummy redirect");
                check(node.cfg.dummy_draw_buf_pixels == w &&
                      node.cfg.dummy_draw_buf_bytes == lv_draw_buf_width_to_stride(w, cf) &&
                      d->buf_1->header.w == w && d->buf_1->header.h == 1 && d->buf_2 == NULL,
                      "dummy draw must allocate exactly one native row");
                check(display_manager_redirect_dummy_draw_buffer(&node) == ESP_OK && allocations == before + 1,
                      "repeated redirect must reuse its allocation");
                node.cfg.draw_buf_primary = panel[0]; node.cfg.draw_buf_secondary = panel[1];
                node.cfg.draw_buf_pixels = (size_t)w * h;
                check(display_manager_set_buffers_v9(&node, d, mode), "restore panel after dummy draw");
                check(d->buf_1->data == panel[0] && d->buf_1->header.stride == w * px &&
                      d->buf_1->header.h == h, "restored packed layout");
                lv_display_delete(d);
                free(panel[0]); free(panel[1]); free(node.cfg.dummy_draw_buf_primary);
                panel[0] = panel[1] = NULL;
                cases++;
            }
        }
    }
    lv_deinit();
    printf("PASS: %u native/rebound/dummy layouts; 3 geometries, 4 rotations, 3 formats, FULL/DIRECT\n", cases);
    return 0;
}
'''


def run_display_buffer_tests(work, lvgl, library, config_dir, sanitize=False):
    source = (ADAPTER / 'src/display/display_manager.c').read_text()
    helpers = '\n\n'.join(function(source, name) for name in
                          ('display_manager_set_buffers_v9', 'display_manager_redirect_dummy_draw_buffer'))
    work = Path(work) / 'display-buffer-tests'
    work.mkdir(parents=True, exist_ok=True)
    flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if sanitize else ['-O2']
    command = [*shlex.split(os.environ.get('CC', 'cc')), *flags, '-DLV_CONF_INCLUDE_SIMPLE',
               '-I' + str(config_dir), '-I' + str(lvgl), '-Wall', '-Wextra', '-Werror']
    variants = [('current', helpers)]
    for label, current, old in (
        ('logical-panel-stride', 'uint32_t width = disp->hor_res;',
         'uint32_t width = lv_display_get_horizontal_resolution(disp);'),
        ('logical-dummy-row', 'size_t draw_buf_pixels = node->lv_disp->hor_res;',
         'size_t draw_buf_pixels = lv_display_get_horizontal_resolution(node->lv_disp);'),
    ):
        if helpers.count(current) != 1:
            raise RuntimeError('Cannot install negative control: ' + label)
        variants.append((label, helpers.replace(current, old)))
    for label, body in variants:
        cfile, binary = work / (label + '.c'), work / label
        cfile.write_text(HARNESS.replace('__HELPERS__', body))
        subprocess.run([*command, str(cfile), str(library), '-lm', '-lpthread', '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'))
        if label == 'current':
            if result.returncode != 0 or 'PASS: 72 ' not in result.stdout:
                raise RuntimeError('Buffer layout regression: ' + result.stdout + result.stderr)
            print(result.stdout.strip(), flush=True)
        elif result.returncode == 0 or 'layout regression:' not in result.stderr:
            raise RuntimeError('Negative control escaped: ' + label + result.stdout + result.stderr)
        else:
            print('caught buffer-layout control: ' + label, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl', type=Path, required=True)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--config-dir', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    if args.build_dir:
        run_display_buffer_tests(args.build_dir, args.lvgl, args.library, args.config_dir, args.sanitize)
    else:
        with tempfile.TemporaryDirectory(prefix='adapter-buffer-layout-') as work:
            run_display_buffer_tests(work, args.lvgl, args.library, args.config_dir, args.sanitize)


if __name__ == '__main__':
    main()
