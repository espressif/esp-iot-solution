# SPDX-FileCopyrightText: 2026 youkorr
# SPDX-License-Identifier: MIT
"""Compare transformed objects with an independent legacy-software oracle.

The caller stages the pinned fake PPA and this adapter, and supplies a *patched*
LVGL 9.5/9.6 tree. Each library is rebuilt completely: the policy adds a field
to the private display structure, whose allocation size is also used by init.
Only the Python standard library, a POSIX C compiler, pthreads and zlib are used.
"""
import argparse
from array import array
import concurrent.futures
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shlex
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
CASES = 256
CONF = '''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#ifndef LV_USE_OS
#define LV_USE_OS LV_OS_NONE
#endif
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_USE_FLOAT 1
#define LV_USE_MATRIX 1
#ifndef LV_DRAW_TRANSFORM_USE_MATRIX
#define LV_DRAW_TRANSFORM_USE_MATRIX 1
#endif
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_DRAW_SW_COMPLEX 1
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (64 * 1024)
#define LV_DEF_REFR_PERIOD 33
#define LV_USE_PERF_MONITOR 0
#define LV_USE_SYSMON 0
#endif
'''
POLICY_CHECK = '''#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    lv_init();
    assert(!lv_display_get_software_object_transforms(NULL));
    lv_display_set_software_object_transforms(NULL, true);
    lv_display_t *a = lv_display_create(64,48), *b = lv_display_create(64,48);
    assert(a && b);
    assert(!lv_display_get_software_object_transforms(a));
    assert(!lv_display_get_software_object_transforms(b));
    lv_display_set_software_object_transforms(a, true);
    assert(lv_display_get_software_object_transforms(a));
    assert(!lv_display_get_software_object_transforms(b));
    lv_display_set_default(b);
    assert(!lv_display_get_software_object_transforms(NULL));
    lv_display_set_software_object_transforms(NULL, true);
    assert(lv_display_get_software_object_transforms(b));
    lv_display_set_software_object_transforms(a, false);
    assert(!lv_display_get_software_object_transforms(a));
    assert(lv_display_get_software_object_transforms(b));
    lv_display_delete(b);
    lv_display_t *c = lv_display_create(64,48);
    assert(c && !lv_display_get_software_object_transforms(c));
    assert(!lv_display_get_software_object_transforms(a));
    lv_display_delete(c); lv_display_delete(a); lv_deinit();
    puts("PASS: default false, two displays, NULL/default, toggle, delete/recreate");
    return 0;
}
'''


def _run(command, **kwargs):
    result = subprocess.run([str(arg) for arg in command], capture_output=True, text=True, **kwargs)
    if result.returncode:
        raise RuntimeError(' '.join(map(str, command)) + '\n' +
                           (result.stdout + result.stderr)[-4000:])
    return result


def _version(lvgl):
    for name in ('include/lvgl/lv_version.h', 'lv_version.h'):
        path = lvgl / name
        if not path.exists():
            continue
        parts = [re.search(r'^\s*#define\s+LVGL_VERSION_' + part + r'\s+(\d+)\b',
                           path.read_text(), re.M) for part in ('MAJOR', 'MINOR', 'PATCH')]
        if all(parts):
            result = tuple(int(part.group(1)) for part in parts)
            if result[:2] not in ((9, 5), (9, 6)):
                raise ValueError('Transform regression bounds cover LVGL 9.5/9.6 only')
            return result
    raise ValueError('Cannot identify the supplied LVGL version')


def _flags(root, lvgl, matrix, threaded, sanitize):
    optimization = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if sanitize else ['-O2']
    return [*shlex.split(os.environ.get('CC', 'cc')), *optimization,
            '-DLV_CONF_INCLUDE_SIMPLE', '-I' + str(root), '-I' + str(lvgl),
            '-DLV_DRAW_TRANSFORM_USE_MATRIX=' + str(int(matrix)),
            '-DLV_USE_OS=' + str(int(threaded)), '-DESP_LVGL_ADAPTER_OS_MUTEX_CLEANUP=1']


def _library(root, lvgl, name, flags):
    sources = sorted((lvgl / 'src').rglob('*.c'))
    fingerprint = hashlib.sha256(CONF.encode() + repr(flags).encode())
    for path in sorted(p for p in lvgl.rglob('*') if p.is_file() and p.suffix in ('.h', '.c')):
        fingerprint.update(str(path.relative_to(lvgl)).encode())
        fingerprint.update(path.read_bytes())
    digest = fingerprint.hexdigest()
    archive, marker = root / (name + '.a'), root / (name + '.sha256')
    if archive.exists() and marker.exists() and marker.read_text().strip() == digest:
        return archive
    objects = root / (name + '-objects')
    def compile_one(source):
        target = objects / source.relative_to(lvgl).with_suffix('.o')
        target.parent.mkdir(parents=True, exist_ok=True)
        _run([*flags, '-w', '-c', source, '-o', target])
        return target
    print('  rebuilding complete LVGL library: ' + name, flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        compiled = list(pool.map(compile_one, sources))
    archive.unlink(missing_ok=True)
    _run(['ar', 'rcs', archive, *compiled])
    marker.write_text(digest + '\n')
    shutil.rmtree(objects)  # Keep bounded disk usage; the archive is sufficient.
    return archive


def _program(root, lvgl, fixture, archive, flags, name, subject):
    command = [*flags, '-Wall', '-Wextra', '-Wno-unused-parameter',
               '-Wno-deprecated-declarations', '-Werror', '-DTEST_SUBJECT=' + str(int(subject)),
               HERE / 'transform_scenes.c']
    if subject:
        unit, stub = fixture / 'components/lvgl_ortho', fixture / 'test/p4stub'
        command += ['-I' + str(unit), '-I' + str(stub), unit / 'lv_draw_ortho.c',
                    unit / 'lv_draw_ortho_ppa.c', stub / 'p4fake.c']
    binary = root / name
    _run([*command, archive, '-lm', '-lpthread', '-lz', '-o', binary])
    return binary


def _no_core_dump():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def _case(binary, folder, mode, index, env):
    return subprocess.run([str(binary), str(folder), mode, str(index)], capture_output=True,
                          text=True, env=env, timeout=15, preexec_fn=_no_core_dump)


def _frame_statistics(log, require_ppa=False):
    statistics = {'frames': 0, 'ppa_transactions': 0, 'ppa_fills': 0, 'ppa_pixels': 0}
    for line in log.read_text().splitlines():
        fields = line.split(',')
        if len(fields) != 14 or fields[1:3] != ['320', '480']:
            continue
        statistics['frames'] += 1
        for key, index in (('ppa_transactions', 10), ('ppa_fills', 11), ('ppa_pixels', 12)):
            statistics[key] += int(fields[index])
        if int(fields[13]) != 0:
            raise RuntimeError('A simulated PPA transaction was still pending at frame capture')
    if statistics['frames'] != CASES:
        raise RuntimeError('Missing per-frame renderer/PPA statistics')
    if require_ppa and statistics['ppa_transactions'] == 0:
        raise RuntimeError('The fake-PPA variant did not execute any PPA transactions')
    return statistics


def _render(root, binary, label, mode, env):
    folder = root / 'frames' / label
    folder.mkdir(parents=True, exist_ok=True)
    for old in folder.glob('*.bin.gz'):
        old.unlink()
    errors = []
    with (root / (label + '.log')).open('w') as log:
        for index in range(CASES):
            try:
                result = _case(binary, folder, mode, index, env)
                log.write(f'CASE {index} returncode={result.returncode}\n' + result.stdout + result.stderr)
                if result.returncode:
                    errors.append({'case': index, 'returncode': result.returncode})
            except subprocess.TimeoutExpired:
                errors.append({'case': index, 'timeout_s': 15})
                log.write(f'CASE {index}: timeout\n')
            log.flush()
    if errors or len(list(folder.glob('*.bin.gz'))) != CASES:
        raise RuntimeError(f'{label}: transform render failures: {errors}; see {root / (label + ".log")}')
    if label != 'oracle':
        statistics = _frame_statistics(root / (label + '.log'), require_ppa=mode == 'ppa')
        (root / (label + '-statistics.json')).write_text(json.dumps(statistics, indent=2) + '\n')
    return folder


def _words(raw, bits):
    words = array('H' if bits == 16 else 'I')
    words.frombytes(raw)
    if sys.byteorder != 'little':
        words.byteswap()
    return words


def _difference(expected, actual, bits):
    a, b = gzip.decompress(expected.read_bytes()), gzip.decompress(actual.read_bytes())
    if len(a) != len(b) or len(a) != 320 * 480 * (bits // 8):
        raise RuntimeError('Invalid native framebuffer length')
    if a == b:
        return {'different_pixels': 0, 'max_native_channel_difference': 0,
                'pixels_above_one_native_level': 0}
    aa, bb = _words(a, bits), _words(b, bits)
    mask = 0xffff if bits == 16 else 0xffffff  # Ignore unused X in XRGB8888.
    changed = [i for i, (x, y) in enumerate(zip(aa, bb)) if (x ^ y) & mask]
    maximum, above_one = 0, 0
    for i in changed:
        x, y = aa[i], bb[i]
        if bits == 16:
            delta = max(abs((x >> 11) - (y >> 11)), abs(((x >> 5) & 63) - ((y >> 5) & 63)),
                        abs((x & 31) - (y & 31)))
        else:
            delta = max(abs(((x >> shift) & 255) - ((y >> shift) & 255)) for shift in (0, 8, 16))
        maximum = max(maximum, delta)
        above_one += delta > 1
    return {'different_pixels': len(changed), 'max_native_channel_difference': maximum,
            'pixels_above_one_native_level': above_one}


def _bounds(version, scene, bits, rotation, mode):
    """Known legacy-SW resampling varies with clipping/tile subdivision.

    This is not a blanket visual tolerance. DIRECT 9.6 is exact; DIRECT 9.5
    permits only quantization in the two named scaling scenes. PARTIAL's
    isolated sampling residuals are named and bounded separately. Missing
    transformations change thousands of pixels and cannot pass these bounds.
    """
    if mode == 'direct':
        if version[:2] == (9, 5) and scene in ('object-scale', 'parent-scale') and rotation:
            return 256, 1, 0
        return 0, 0, 0
    if scene in ('control', 'image-rot30', 'image-rot90', 'image-rot30-scale'):
        return 0, 0, 0
    if version[:2] == (9, 5) and scene == 'object-rot30-scale' and rotation in (90, 270):
        return (32, 11, 2) if bits == 16 else (160, 95, 7)
    if version[:2] == (9, 6) and scene == 'nested-rotations' and bits == 32 and rotation in (90, 270):
        return 128, 2, 1
    return 256, 1, 0


def _compare(oracle, subject, version):
    rows = []
    for expected in sorted(oracle.glob('*.bin.gz')):
        scene, dimensions, bits, rotation, mode = expected.name.removesuffix('.bin.gz').rsplit('-', 4)
        bits, rotation = int(bits), int(rotation[1:])
        row = {'scene': scene, 'bits': bits, 'rotation': rotation, 'mode': mode,
               **_difference(expected, subject / expected.name, bits)}
        bound = _bounds(version, scene, bits, rotation, mode)
        row['bounds'] = dict(zip(('different_pixels', 'max_native_channel_difference',
                                 'pixels_above_one_native_level'), bound))
        row['passed'] = all(row[key] <= limit for key, limit in row['bounds'].items())
        rows.append(row)
    if len(rows) != CASES:
        raise RuntimeError('The independent oracle did not produce all 256 references')
    return rows


def _policy_check(root, archive, flags, env):
    source, binary = root / 'display_policy.c', root / 'display_policy'
    source.write_text(POLICY_CHECK)
    _run([*flags, '-Wall', '-Wextra', '-Werror', source, archive, '-lm', '-lpthread', '-o', binary])
    result = _run([binary], env=env, timeout=30)
    (root / 'display-policy.log').write_text(result.stdout + result.stderr)


def _rotate_box(box, rotation):
    x1, y1, x2, y2 = box
    width, height = (480, 320) if rotation in (90, 270) else (320, 480)
    if rotation == 90:
        return [y1, width - 1 - x2, y2, width - 1 - x1]
    if rotation == 180:
        return [width - 1 - x2, height - 1 - y2, width - 1 - x1, height - 1 - y1]
    if rotation == 270:
        return [height - 1 - y2, x1, height - 1 - y1, x2]
    return list(box)


def _red_box(frame, bits):
    words = _words(gzip.decompress(frame.read_bytes()), bits)
    color, mask = (0xd168, 0xffff) if bits == 16 else (0xd52e45, 0xffffff)
    indices = [i for i, word in enumerate(words) if word & mask == color]
    if len(indices) != 80 * 40:
        raise RuntimeError('A custom transform lost or resampled its opaque rectangle')
    xs, ys = [i % 320 for i in indices], [i // 320 for i in indices]
    return [min(xs), min(ys), max(xs), max(ys)]


def _custom_checks(root, binary, env):
    results = []
    for kind, name in enumerate(('custom-translation', 'custom-quarter90')):
        logical = [113, 207, 192, 246] if kind == 0 else [60, 200, 99, 279]
        for rotation in (0, 90, 180, 270):
            for bits in (16, 32):
                paths = []
                expected = _rotate_box(logical, rotation)
                for enabled in (0, 1):
                    folder = root / 'custom' / str(enabled)
                    folder.mkdir(parents=True, exist_ok=True)
                    result = _run([binary, '--custom', folder, kind, rotation, bits, enabled],
                                  env=env, timeout=15, preexec_fn=_no_core_dump)
                    frame = folder / f'{name}-320x480-{bits}-r{rotation}-direct.bin.gz'
                    observed = _red_box(frame, bits)
                    if observed != expected:
                        raise RuntimeError(f'{name} r{rotation} RGB{bits}: bbox {observed}, expected {expected}')
                    paths.append(frame)
                difference = _difference(*paths, bits)
                if difference['different_pixels']:
                    raise RuntimeError(f'The per-display style policy changed {name} r{rotation} RGB{bits}')
                results.append({'scene': name, 'rotation': rotation, 'bits': bits,
                                'policy_off_on_identical': True, 'analytic_bbox': expected})
    (root / 'custom-matrix-results.json').write_text(json.dumps(results, indent=2) + '\n')
    return results


def _negative_controls(root, binary, oracle, env):
    folder = root / 'negative-control'
    folder.mkdir(exist_ok=True)
    direct = _case(binary, folder, 'no-policy', 5, env)
    if direct.returncode:
        raise RuntimeError('The direct control should reproduce a lost transform, not an unrelated failure')
    name = 'object-rot30-320x480-16-r0-direct.bin.gz'
    mismatch = _difference(oracle / name, folder / name, 16)
    if mismatch['different_pixels'] < 1000:
        raise RuntimeError('The regression oracle did not detect the disabled object-layer policy')
    partial = _case(binary, folder, 'no-policy', 21, env)
    diagnostic = partial.stdout + partial.stderr
    # LVGL 9.5 exposes the out-of-band SW write here. LVGL 9.6's clipping
    # instead selects a bitmap layer in this particular banded case, even
    # without the policy. The DIRECT control above must always detect the
    # missing transformation; do not require a crash on a newer LVGL path.
    if partial.returncode and 'coordinates out of range' not in diagnostic:
        raise RuntimeError('The partial control failed for an unrelated reason: ' + diagnostic[-1500:])
    partial_difference = None
    if partial.returncode == 0:
        name = 'object-rot30-320x480-16-r0-partial.bin.gz'
        partial_difference = _difference(oracle / name, folder / name, 16)['different_pixels']
    (root / 'negative-controls.log').write_text(direct.stdout + direct.stderr + diagnostic)
    return {'disabled_policy_direct_different_pixels': mismatch['different_pixels'],
            'disabled_policy_partial_returncode': partial.returncode,
            'disabled_policy_partial_bounds_error': 'coordinates out of range' in diagnostic,
            'disabled_policy_partial_different_pixels': partial_difference}


def run_transform_tests(work, lvgl_source, *, sanitize=False):
    """Run after staging and installing this adapter's LVGL compatibility hooks."""
    work, lvgl = Path(work).resolve(), Path(lvgl_source).resolve()
    version = _version(lvgl)
    root = work / 'test' / ('transform-build-asan' if sanitize else 'transform-build')
    root.mkdir(parents=True, exist_ok=True)
    (root / 'lv_conf.h').write_text(CONF)
    env = dict(os.environ)
    env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    oracle_flags = _flags(root, lvgl, False, False, sanitize)
    oracle_lib = _library(root, lvgl, 'oracle', oracle_flags)
    oracle_bin = _program(root, lvgl, work, oracle_lib, oracle_flags, 'oracle', False)
    oracle = _render(root, oracle_bin, 'oracle', 'cpu', env)
    cpu_flags = _flags(root, lvgl, True, False, sanitize)
    cpu_lib = _library(root, lvgl, 'cpu', cpu_flags)
    _policy_check(root, cpu_lib, cpu_flags, env)
    cpu_bin = _program(root, lvgl, work, cpu_lib, cpu_flags, 'cpu', True)
    custom = _custom_checks(root, cpu_bin, env)
    controls = _negative_controls(root, cpu_bin, oracle, env)
    results = {}
    for name, mode, threaded in (('cpu', 'cpu', False), ('fake-ppa', 'ppa', False), ('pthread', 'cores', True)):
        if threaded:
            flags = _flags(root, lvgl, True, True, sanitize)
            library = _library(root, lvgl, name, flags)
            binary = _program(root, lvgl, work, library, flags, name, True)
        else:
            binary = cpu_bin
        frames = _render(root, binary, name, mode, env)
        rows = _compare(oracle, frames, version)
        results[name] = rows
        (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        failed = [row for row in rows if not row['passed']]
        exact = sum(row['different_pixels'] == 0 for row in rows)
        print(f'  transformed objects {name}: {exact}/{CASES} RGB-exact; '
              f'{CASES - exact} within documented sampling bounds; {len(failed)} failures', flush=True)
        if failed:
            raise RuntimeError(f'Transformed-object rendering differs beyond its regression bounds: {failed[:8]}')
    summary = {'lvgl': '.'.join(map(str, version)), 'hardware': False,
               'independent_oracle_object_matrix': False, 'subject_object_matrix': True,
               'full_library_rebuilds': True, 'style_cases_per_variant': CASES,
               'style_variants': len(results), 'custom_matrix_renders': len(custom) * 2,
               'policy_default_and_isolation': 'pass', 'negative_controls': controls,
               'fake_ppa_statistics': json.loads((root / 'fake-ppa-statistics.json').read_text()),
               'source_sha256': hashlib.sha256((HERE / 'transform_scenes.c').read_bytes()).hexdigest(),
               'counts': {name: {'exact': sum(r['different_pixels'] == 0 for r in rows),
                                  'within_sampling_bounds': sum(r['passed'] for r in rows),
                                  'direct_exact': sum(r['mode'] == 'direct' and r['different_pixels'] == 0 for r in rows)}
                          for name, rows in results.items()}}
    (root / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True, help='Staged host fixture directory')
    parser.add_argument('--lvgl', type=Path, required=True, help='Patched LVGL source directory')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    run_transform_tests(args.work, args.lvgl, sanitize=args.sanitize)
