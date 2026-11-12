# SPDX-FileCopyrightText: 2026 youkorr
# SPDX-License-Identifier: MIT
"""Test this adapter's actual draw unit against pinned ppa-alternative fixtures.

Requires a ppa-alternative checkout containing the pinned commits below, a
LVGL 9.5/9.6 checkout, make and a C compiler. Nothing is downloaded by this
script. The fixtures' cache-era suite excludes later experimental split/tile
controls; the fake PPA and font regression use the newer board-tested source.
"""

import argparse
import importlib.util
import io
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

FIXTURES = '6cf0eed1277032489fd0281890b9a326c93f063f'
CACHE_SUITE = '9db8952fc7f06438819fd4aaf28f5fb8a35a56b7'
ADAPTER = Path(__file__).resolve().parents[2]
UNIT = ADAPTER / 'src/display/rotate_in_draw'
_cleanup_spec = importlib.util.spec_from_file_location('patch_lvgl_os_cleanup', ADAPTER / 'tools/patch_lvgl_os_cleanup.py')
_cleanup = importlib.util.module_from_spec(_cleanup_spec)
_cleanup_spec.loader.exec_module(_cleanup)
_object_spec = importlib.util.spec_from_file_location('patch_lvgl_object_transforms', ADAPTER / 'tools/patch_lvgl_object_transforms.py')
_object_policy = importlib.util.module_from_spec(_object_spec)
_object_spec.loader.exec_module(_object_policy)

MUTEX_SPY = r"""
#if LV_USE_OS != LV_OS_NONE
#include <stdatomic.h>
#include "src/osal/lv_os_private.h"
static _Atomic unsigned s_os_mutex_inits, s_os_mutex_deletes;
lv_result_t __real_lv_mutex_init(lv_mutex_t *mutex);
lv_result_t __real_lv_mutex_delete(lv_mutex_t *mutex);
lv_result_t __wrap_lv_mutex_init(lv_mutex_t *mutex)
{
    const lv_result_t result = __real_lv_mutex_init(mutex);
    if(result == LV_RESULT_OK) s_os_mutex_inits++;
    return result;
}
lv_result_t __wrap_lv_mutex_delete(lv_mutex_t *mutex)
{
    const lv_result_t result = __real_lv_mutex_delete(mutex);
    if(result == LV_RESULT_OK) s_os_mutex_deletes++;
    return result;
}
#endif
"""

CLEANUP_CASE = r"""
extern int g_p4_bad_geometry;
extern uint32_t g_p4_slow_out_width, g_p4_slow_out_height, g_p4_slow_max_bytes;

static void check_cache_progress(void)
{
    const lv_display_rotation_t rotations[] = {
        LV_DISPLAY_ROTATION_90, LV_DISPLAY_ROTATION_180, LV_DISPLAY_ROTATION_270
    };
    for(unsigned k = 0; k < sizeof(rotations) / sizeof(rotations[0]); k++) {
        const lv_display_rotation_t rot = rotations[k];
        const bool quarter = rot != LV_DISPLAY_ROTATION_180;
        const int nw = quarter ? 720 : 1280, nh = quarter ? 1280 : 720;
        lv_draw_ortho_set_window(NULL, NULL);
        lv_draw_ortho_ppa_init();
        lv_draw_ortho_set_image_cache(4 * 1024 * 1024);
        g_wall = image_of(1280, 720, LV_COLOR_FORMAT_RGB565, 29);
        g_wall_constant = true;
        lv_draw_ortho_set_image_constant_cb(wall_constant);
        screen_t *screen = make_screen(nw, nh, LV_COLOR_FORMAT_RGB565, rot, true, true);
        lv_obj_t *image = lv_image_create(lv_display_get_screen_active(screen->disp));
        lv_image_set_src(image, g_wall);
        lv_obj_set_pos(image, 0, 0);
        /* Only a full-height output is the new cache, not the small partial
         * display bands. At 4 bytes/us the full 1.8 MiB job needs 460 ms;
         * bounded bands finish well inside the unchanged 250 ms watchdog. */
        g_p4_slow_out_width = (uint32_t)nw;
        g_p4_slow_out_height = (uint32_t)nh;
        g_p4_slow_max_bytes = 0;
        for(int i = 0; i < 3; i++) {
            lv_obj_invalidate(image);
            lv_refr_now(screen->disp);
        }
        const lv_draw_ortho_stats_t st = *lv_draw_ortho_get_stats();
        uint8_t *expected = malloc((size_t)screen->stride * nh);
        turn_buffer(g_wall->data, expected, 1280, 720, g_wall->header.stride,
                    screen->stride, rot, LV_COLOR_FORMAT_RGB565);
        const bool pixels_ok = memcmp(expected, screen->buf, (size_t)screen->stride * nh) == 0;
        g_cases++;
        if(!st.turner_failed && st.images_cached > 0 && pixels_ok &&
           g_p4_slow_max_bytes > 0 && g_p4_slow_max_bytes <= 128 * 1024)
            printf("  ok    cache preparation at %d: slow transactions make progress, peak %u bytes, pixels match\n",
                   (int)rot, g_p4_slow_max_bytes);
        else {
            printf("  FAIL  cache preparation progress at %d: failed=%u peak=%u cached=%u pixels=%d\n",
                   (int)rot, st.turner_failed, g_p4_slow_max_bytes, st.images_cached, pixels_ok);
            g_failures++;
        }
        g_p4_slow_out_height = 0;
        if(st.turner_failed) p4fake_reset(); /* discard a mutant's timed-out DMA before freeing its buffers */
        free(expected);
        drop_screen(screen);
        lv_draw_ortho_set_image_cache(0);
        lv_draw_ortho_set_image_constant_cb(NULL);
        lv_draw_ortho_ppa_deinit();
        free((void *)g_wall->data);
        free(g_wall);
        g_wall = NULL;
        g_wall_constant = false;
    }
}

static void check_padded_cache_geometry(void)
{
    lv_draw_ortho_set_window(NULL, NULL);
    lv_draw_ortho_ppa_init();
    lv_draw_ortho_set_image_cache(4 * 1024 * 1024);
    g_wall = image_of(1280, 720, LV_COLOR_FORMAT_RGB565, 23);
    g_wall_constant = true;
    lv_draw_ortho_set_image_constant_cb(wall_constant);
    screen_t *screen = make_screen(720, 1280, LV_COLOR_FORMAT_RGB565, LV_DISPLAY_ROTATION_270, true, false);
    lv_obj_t *scr = lv_display_get_screen_active(screen->disp);
    lv_obj_t *image = lv_image_create(scr);
    lv_image_set_src(image, g_wall);
    lv_obj_set_pos(image, 0, 0);
    const int rejected = g_p4_bad_geometry;
    lv_refr_now(screen->disp);
    lv_draw_ortho_reset_stats();
    lv_obj_invalidate(scr);
    lv_refr_now(screen->disp);
    const lv_draw_ortho_stats_t *st = lv_draw_ortho_get_stats();
    g_cases++;
    if(g_p4_bad_geometry == rejected && st->image_cache_builds > 0 && st->bytes_handed > 0)
        printf("  ok    padded cache geometry: 720 RGB565 pixels by 1281 allocated rows accepted by the PPA\n");
    else {
        printf("  FAIL  padded cache geometry: %d refusals, %u builds, %llu DMA bytes\n",
               g_p4_bad_geometry - rejected, st->image_cache_builds, (unsigned long long)st->bytes_handed);
        g_failures++;
    }
    drop_screen(screen);
    lv_draw_ortho_set_image_cache(0);
    lv_draw_ortho_set_image_constant_cb(NULL);
    lv_draw_ortho_ppa_deinit();
    free((void *)g_wall->data);
    free(g_wall);
    g_wall = NULL;
    g_wall_constant = false;
}

static void check_cache_deinit(void)
{
    lv_draw_ortho_set_turner(NULL, NULL);
    lv_draw_ortho_set_image_cache(1024 * 1024);
    g_wall = image_of(200, 150, LV_COLOR_FORMAT_RGB565, 23);
    g_wall_constant = true;
    lv_draw_ortho_set_image_constant_cb(wall_constant);
    screen_t *screen = make_screen(320, 480, LV_COLOR_FORMAT_RGB565, LV_DISPLAY_ROTATION_90, true, false);
    scene_wall(lv_display_get_screen_active(screen->disp));
    for(int i = 0; i < 3; i++) {
        lv_obj_invalidate(lv_display_get_screen_active(screen->disp));
        lv_refr_now(screen->disp);
    }
    const bool held = lv_draw_ortho_get_stats()->image_cache_bytes > 0;
    drop_screen(screen);
    lv_deinit();
#if LV_USE_OS != LV_OS_NONE
    g_cases++;
    if(s_os_mutex_inits == s_os_mutex_deletes)
        printf("  ok    lv_deinit releases all OS mutexes (%u created, %u deleted)\n", s_os_mutex_inits, s_os_mutex_deletes);
    else {
        printf("  FAIL  OS mutex lifecycle: %u created, %u deleted\n", s_os_mutex_inits, s_os_mutex_deletes);
        g_failures++;
    }
#endif
    g_cases++;
    if(held && lv_draw_ortho_get_stats()->image_cache_bytes == 0)
        printf("  ok    lv_deinit releases cached pixel allocations\n");
    else {
        printf("  FAIL  cached pixel allocations survive lv_deinit (held=%d)\n", held);
        g_failures++;
    }
    free((void *)g_wall->data);
    free(g_wall);
}
"""


def git(reference, *args):
    return subprocess.check_output(['git', '-C', str(reference), *args])


def stage(reference, work):
    archive = git(reference, 'archive', FIXTURES, 'test', 'patches', 'components/lvgl_ortho')
    with tarfile.open(fileobj=io.BytesIO(archive)) as source:
        for entry in source:
            path = work / entry.name
            if not path.resolve().is_relative_to(work.resolve()):
                raise ValueError('Fixture path escapes the test directory')
            if entry.isdir():
                path.mkdir(parents=True, exist_ok=True)
            elif entry.isfile():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(source.extractfile(entry).read())
                os.utime(path, (entry.mtime, entry.mtime))
            else:
                raise ValueError('Only regular fixture files are accepted')
    target = work / 'components/lvgl_ortho'
    for path in UNIT.glob('lv_draw_ortho*.[ch]'):
        shutil.copyfile(path, target / path.name)
    # Match the actual IDF check; the fixture allowed an extra cache line and
    # therefore missed a 720-pixel-wide cache's padded final row on hardware.
    fake = work / 'test/p4stub/p4fake.c'
    code = fake.read_text()
    before = 'if((size_t)op->out.pic_w * op->out.pic_h * px > op->out.buffer_size + 128) return ESP_ERR_INVALID_ARG;'
    assert code.count(before) == 1
    code = code.replace(before, 'if((size_t)op->out.pic_w * op->out.pic_h * px > op->out.buffer_size) { g_p4_bad_geometry++; return ESP_ERR_INVALID_ARG; }')
    code = code.replace('esp_err_t ppa_do_scale_rotate_mirror(', 'int g_p4_bad_geometry;\n\nesp_err_t ppa_do_scale_rotate_mirror(', 1)
    original_delay = '''void esp_rom_delay_us(uint32_t us)
{
    (void)us;
    finish_one();
}'''
    timed_delay = '''uint32_t g_p4_slow_out_width, g_p4_slow_out_height, g_p4_slow_max_bytes;
static int g_slow_qid;
static uint32_t g_slow_remaining;

void esp_rom_delay_us(uint32_t us)
{
    if(g_p4_slow_out_height && g_count && !g_queue[g_head].is_fill &&
       g_queue[g_head].op.out.pic_w >= g_p4_slow_out_width &&
       g_queue[g_head].op.out.pic_h >= g_p4_slow_out_height &&
       g_queue[g_head].op.rotation_angle != PPA_SRM_ROTATION_ANGLE_0) {
        const ppa_srm_oper_config_t *op = &g_queue[g_head].op;
        const uint32_t bytes = op->in.block_w * op->in.block_h * 2;
        if(bytes > g_p4_slow_max_bytes) g_p4_slow_max_bytes = bytes;
        if(g_slow_qid != g_queue[g_head].qid) {
            g_slow_qid = g_queue[g_head].qid;
            g_slow_remaining = bytes / 4;
        }
        if(g_slow_remaining > us) {
            g_slow_remaining -= us;
            return;
        }
    }
    finish_one();
}'''
    assert code.count(original_delay) == 1
    code = code.replace(original_delay, timed_delay)
    fake.write_text(code)
    current = git(reference, 'show', f"{FIXTURES}:test/main.c").decode()
    main = git(reference, 'show', f"{CACHE_SUITE}:test/main.c").decode()
    # Reference displays keep LVGL's original policy. Subject displays use
    # the same explicit opt-in as the adapter, including screen rotation 0.
    needle = '    if(matrix) lv_display_set_matrix_rotation(s.disp, true);'
    assert main.count(needle) == 1
    main = main.replace(needle, '    if(matrix) lv_display_set_software_object_transforms(s.disp, true);\n' + needle)
    start = current.index('/* A font whose engine')
    end = current.index('/* The same scenes with the PPA', start)
    font = current[start:end]
    font = font.replace('    lv_draw_ortho_set_tile_width(256);\n', '')
    font = font.replace('    lv_draw_ortho_set_tile_width(0);\n', '')
    main = main.replace('/* The same scenes with the PPA', font + '/* The same scenes with the PPA', 1)
    main = main.replace('int main(int argc, char ** argv)', MUTEX_SPY + CLEANUP_CASE + '\nint main(int argc, char ** argv)', 1)
    # The fixture deliberately submits unsafe transfers to validate its fake
    # hardware. Count only the adapter's submissions after that calibration.
    main = main.replace('    check_fake_ppa_against_the_board();', '    check_fake_ppa_against_the_board();\n    g_p4_short = 0;', 1)
    needle = '    /* Last: after it the PPA is stuck, as it would stay on a board. */'
    checks = r"""
#if LV_USE_OS != LV_OS_NONE
    if(g_two_cores) check_font_one_core();
#endif
    check_cache_progress();
    check_padded_cache_geometry();
    g_cases++;
    if(lv_draw_ortho_ppa_dig734(464, 70, LV_DISPLAY_ROTATION_90, 16, 1, 1) && g_p4_short == 0)
        printf("  ok    exact-FIFO block guarded and all submitted transfers keep their margin\n");
    else { printf("  FAIL  FIFO safety margin\n"); g_failures++; }
"""
    assert main.count(needle) == 1
    main = main.replace(needle, checks + needle)
    needle = '    printf("%d of %d cases passed\\n", g_cases - g_failures, g_cases);'
    # Only the final summary, after the stalled-PPA check: no LVGL work follows
    # in a normal run. The optional timing mode retains its live draw unit.
    at = main.rindex(needle)
    main = main[:at] + '    if(!run_bench) check_cache_deinit();\n' + main[at:]
    (work / 'test/main.c').write_text(main)


def build(work, lvgl, mode, sanitize, patch_os=True):
    flags = '-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' if sanitize else '-O2 -g'
    if mode == 'mt':
        flags += ' -DLV_USE_OS=1 -Wl,--wrap=lv_mutex_init -Wl,--wrap=lv_mutex_delete'
    flags += ' -DESP_LVGL_ADAPTER_OS_MUTEX_CLEANUP=1'
    folder = 'build-' + mode + ('-asan' if sanitize else '')
    subprocess.run(['make', '-C', str(work / 'test'), f"LVGL={lvgl}", f"BUILD={folder}",
                    f"{folder}/lvgl-src/.patched"], check=True)
    if patch_os:
        _cleanup.patch(work / 'test' / folder / 'lvgl-src/src/lv_init.c')
    if _object_policy.patch(work / 'test' / folder / 'lvgl-src') == 'patched':
        # The display private structure gained a field: rebuild every LVGL
        # translation unit, including lv_init's display-list node sizing.
        (work / 'test' / folder / 'lvgl-src/.patched').touch()
    subprocess.run(['make', '-C', str(work / 'test'), '-j2', f"LVGL={lvgl}", f"BUILD={folder}",
                    f"CFLAGS={flags}", f"{folder}/ortho_test"], check=True)
    return work / 'test' / folder / 'ortho_test'


def run_tests(work, args):
    stage(args.reference, work)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    for mode in ('plain', 'mt'):
        binary = build(work, args.lvgl, mode, args.sanitize)
        subprocess.run([str(binary), *(['--cores'] if mode == 'mt' else [])], check=True, env=env,
                       cwd=work / 'test')
    from transforms import run_transform_tests
    run_transform_tests(work, work / 'test' / ('build-plain-asan' if args.sanitize else 'build-plain') / 'lvgl-src',
                        sanitize=args.sanitize)
    if not args.mutants:
        return
    unit = work / 'components/lvgl_ortho'
    source = (unit / 'lv_draw_ortho.c').read_text()
    ppa = (unit / 'lv_draw_ortho_ppa.c').read_text()
    mutations = [
        ('font guard', 'lv_draw_ortho.c', 'const bool text = j->t->type == LV_DRAW_DSC_TYPE_LABEL || j->t->type == LV_DRAW_DSC_TYPE_LETTER;', 'const bool text = false;', 'font was asked'),
        ('FIFO margin', 'lv_draw_ortho_ppa.c', 'left < 2 * FIFO_BITS', 'left < FIFO_BITS', 'FIFO safety margin'),
        ('padded cache geometry', 'lv_draw_ortho_ppa.c', 'op.out.pic_h = out_h;', 'op.out.pic_h = dst->h;', 'FAIL  padded cache geometry'),
        ('cache preparation progress', 'lv_draw_ortho.c', 'const int32_t max_pixels = (128 * 1024) / px;', 'const int32_t max_pixels = INT32_MAX / px;', 'FAIL  cache preparation progress'),
        ('cache teardown', 'lv_draw_ortho.c', '    lv_draw_ortho_image_cache_drop(NULL);\n    if (g_helper.on)', '    if (g_helper.on)', 'cached pixel allocations survive'),
    ]
    for name, filename, before, after, expected in mutations:
        (unit / 'lv_draw_ortho.c').write_text(source)
        (unit / 'lv_draw_ortho_ppa.c').write_text(ppa)
        path = unit / filename
        text = path.read_text()
        assert text.count(before) == 1, name
        path.write_text(text.replace(before, after))
        binary = build(work, args.lvgl, 'mt', args.sanitize)
        result = subprocess.run([str(binary), '--cores'], capture_output=True, text=True, env=env, timeout=120,
                                cwd=work / 'test')
        if result.returncode == 0 or expected not in result.stdout:
            raise RuntimeError(f"Regression test did not catch {name}: {result.stdout[-1500:]}")
        print(f"caught mutant: {name}", flush=True)
    # Exercise the shipped LVGL init/deinit, not a model of its resource use.
    init = work / 'test' / ('build-mt-asan' if args.sanitize else 'build-mt') / 'lvgl-src/src/lv_init.c'
    original = init.read_text()
    needle = '    lv_mutex_delete(&LV_GLOBAL_DEFAULT()->lv_general_mutex);'
    assert original.count(needle) == 1
    (unit / 'lv_draw_ortho.c').write_text(source)
    (unit / 'lv_draw_ortho_ppa.c').write_text(ppa)
    try:
        init.write_text(original.replace(needle, '    /* mutant: global mutex leaked */'))
        binary = build(work, args.lvgl, 'mt', args.sanitize, patch_os=False)
        result = subprocess.run([str(binary), '--cores'], capture_output=True, text=True, env=env,
                                timeout=120, cwd=work / 'test')
        if result.returncode == 0 or 'FAIL  OS mutex lifecycle' not in result.stdout:
            raise RuntimeError('Regression test did not catch global OS mutex leak: ' + result.stdout[-1500:])
        print('caught mutant: global OS mutex cleanup', flush=True)
    finally:
        init.write_text(original)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--lvgl', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--mutants', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    args.reference, args.lvgl = args.reference.resolve(), args.lvgl.resolve()
    if args.build_dir:
        work = args.build_dir.resolve()
        marker = work / '.ortho-adapter-tests'
        if work.exists() and any(work.iterdir()) and not marker.exists():
            parser.error('--build-dir must be empty or an earlier test directory')
        work.mkdir(parents=True, exist_ok=True)
        marker.touch()
        run_tests(work, args)
    else:
        with tempfile.TemporaryDirectory(prefix='ortho-adapter-') as folder:
            run_tests(Path(folder), args)


if __name__ == '__main__':
    main()
