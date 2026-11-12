# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Offline test entry point. Sources live in present/dev; outputs stay outside the checkout."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import shutil
sys.path.insert(0, str(Path(__file__).resolve().parent))
from benchmark_buffers import POLICIES, align_adapter_allocation, expected_input_bytes

DEV = Path(__file__).resolve().parents[1]
BOARDS = json.loads((DEV / 'boards/boards.json').read_text())
SUITES = ('benchmark', 'pixels', 'native', 'te-lifecycle', 'rgb-events', 'rgb-handoff', 'smoke')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(cmd, **kwargs):
    print('+', ' '.join(map(str, cmd)), flush=True)
    return subprocess.run(list(map(str, cmd)), check=True, **kwargs)


def cases_for(board, angles, profiles=None):
    """One case table for both implementations; unsupported cases remain explicit."""
    scanout = BOARDS[board]['interface'] in ('rgb', 'dsi')
    if scanout:
        profiles_all = [(n, 'B_' + n, 1, False, 0) for n in
                        ('DOUBLE_FULL', 'TRIPLE_FULL', 'DOUBLE_DIRECT', 'DOUBLE_PARTIAL', 'TRIPLE_PARTIAL', 'AUTO')]
    elif board == 's31-qspi':
        profiles_all = [(f'TE_C{c}_T{t}', 'B_TE', t, False, c) for c in (1, 2) for t in (1, 2)]
        # Adapter-native baselines have distinct layouts; never relabel as C/T.
        if profiles and any(name.startswith('ADAPTER_TE_') for name in profiles):
            profiles_all += [('ADAPTER_TE_SYNC_FULL', 'B_TE', 1, True, 0),
                             ('ADAPTER_TE_ASYNC_FULL', 'B_TE', 1, True, 3),
                             ('ADAPTER_TE_ASYNC_BOUNCE', 'B_TE', 1, True, 4)]
    else:
        profiles_all = [(f'GRAM_T{t}_{m}', 'B_NONE', t, m == 'PSRAM', 0)
                        for m in ('INTERNAL', 'PSRAM') for t in (1, 2)]
        profiles_all += [('AUTO_T2_INTERNAL', 'B_AUTO', 2, False, 0)]
    if profiles:
        unknown = set(profiles) - {p[0] for p in profiles_all}
        if unknown:
            raise ValueError('Unknown profiles: ' + ', '.join(sorted(unknown)))
        profiles_all = [p for p in profiles_all if p[0] in profiles]
    rows = []
    for name, mode, tiles, psram, compose in profiles_all:
        for angle in angles:
            reason = ''
            if mode == 'B_AUTO':
                reason = 'adapter_has_no_AUTO_mode'
            elif mode == 'B_NONE' and angle:
                reason = 'adapter_NONE_has_no_software_rotation'
            elif mode == 'B_TE' and not name.startswith('ADAPTER_TE_'):
                reason = 'adapter_native_TE_baseline_not_measured_in_CT_matrix'
            elif name == 'ADAPTER_TE_SYNC_FULL' and angle:
                reason = 'adapter_sync_TE_has_no_software_rotation'
            adapter_te_pipeline = {3: 1, 4: 2}.get(compose, 0) if name.startswith('ADAPTER_TE_') else 0
            rows.append(dict(name=name, mode=mode, tiles=tiles, psram=psram,
                             compose=0 if name.startswith('ADAPTER_TE_') else compose,
                             adapter_te_pipeline=adapter_te_pipeline,
                             rotation=angle, adapter_supported=not reason, skip_reason=reason))
    return rows


def header(rows):
    def boolean(value):
        return 'true' if value else 'false'
    lines = ['/* Generated from the common case planner; do not edit. */', 'static const bench_case_t bench_cases[] = {']
    for c in rows:
        lines.append('    {%s,%s,%d,%d,%d,%s,%s,%s,%d},' %
                     (json.dumps(c['name']), c['mode'], c['rotation'], c['tiles'], c['compose'],
                      boolean(c['psram']), boolean(c['adapter_supported']), json.dumps(c['skip_reason']), c.get('adapter_te_pipeline', 0)))
    return '\n'.join(lines + ['};', ''])


def idf():
    value = os.environ.get('IDF_PATH')
    if not value:
        raise ValueError('Activate the clean ESP-IDF export.sh first')
    return Path(value).resolve()


def variant_name(args, implementation=None):
    name = implementation or args.implementation
    if getattr(args, 'profile', False):
        name += '-profile'
    if getattr(args, 'xip', 'default') != 'default':
        name += '-xip-' + args.xip
    if getattr(args, 'driver_trace', None):
        name += '-driver-trace'
    return name


def location(args):
    return args.work_root.resolve() / args.board / args.suite / variant_name(args)


def xip_defaults(mode):
    if mode not in ('on', 'off'):
        raise ValueError('Explicit XIP mode required')
    value = 'y' if mode == 'on' else 'n'
    return ''.join(f'CONFIG_{name}={value}\n' for name in
                   ('SPIRAM_XIP_FROM_PSRAM', 'SPIRAM_FETCH_INSTRUCTIONS', 'SPIRAM_RODATA'))


def validate_xip(sdk, mode):
    if mode == 'default':
        return
    for key in ('SPIRAM_XIP_FROM_PSRAM', 'SPIRAM_FETCH_INSTRUCTIONS', 'SPIRAM_RODATA'):
        if (f'CONFIG_{key}=y' in sdk.splitlines()) != (mode == 'on'):
            raise ValueError('Effective SDKCONFIG disagrees with --xip: ' + key)



def prepare(args):
    deps = args.work_root.resolve() / 'deps'
    run([sys.executable, DEV / 'test_apps/boards/prepare_deps.py', '--cache', args.cache, '--output', deps])
    return deps


def comparable_sdkconfig(sdk):
    # Include cache topology and LCD/PSRAM clocks, not just LVGL and CPU.
    # Disabled options must also affect the fingerprint.
    prefixes = ('CONFIG_LV_', 'CONFIG_SPIRAM', 'CONFIG_ESP_DEFAULT_CPU_',
                'CONFIG_COMPILER_OPTIMIZATION', 'CONFIG_FREERTOS_HZ',
                'CONFIG_ESP32', 'CONFIG_CACHE_', 'CONFIG_LCD_')
    return '\n'.join(sorted(line for line in sdk.splitlines()
                            if line.removeprefix('# ').startswith(prefixes)))


def build(args):
    xip_mode = getattr(args, 'xip', 'default')
    driver_trace = getattr(args, 'driver_trace', None)
    if (xip_mode != 'default' or driver_trace) and (args.suite != 'benchmark' or args.board not in ('s3-rgb', 's31-rgb', 'p4-dsi')):
        raise ValueError('XIP/driver trace variants require RGB/DSI benchmark')
    if args.profile and (args.implementation != 'present' or args.suite != 'benchmark'):
        raise ValueError('--profile requires present benchmark')
    if args.implementation == 'adapter' and args.suite != 'benchmark':
        raise ValueError('adapter comparison applies to benchmark only')
    if args.board == 'box3-spi' and args.suite in ('pixels', 'native'):
        raise ValueError('BOX-3 pixel matrix is not implemented; use benchmark (not a pass)')
    if args.board == 's31-rgb' and args.suite not in ('benchmark', 'pixels', 'rgb-events', 'rgb-handoff', 'smoke'):
        raise ValueError('S31 RGB currently supports benchmark, pixels, rgb-events, rgb-handoff, and smoke')
    if args.suite == 'native' and args.board not in ('s3-rgb', 'p4-dsi'):
        raise ValueError('Native scanout matrix requires RGB or DSI')
    if args.suite == 'te-lifecycle' and args.board != 's31-qspi':
        raise ValueError('TE lifecycle requires S31 mosaico')
    if args.suite == 'rgb-events' and args.board != 's31-rgb':
        raise ValueError('RGB event probe requires S31 Korvo RGB')
    if args.suite == 'rgb-handoff' and args.board not in ('s3-rgb', 's31-rgb'):
        raise ValueError('RGB handoff requires confirmed S3 or S31 RGB')
    dirty_idf = subprocess.check_output(['git', '-C', str(idf()), 'status', '--porcelain', '--untracked-files=no'], text=True)
    if dirty_idf.strip():
        raise ValueError('Clean IDF required; tracked IDF modifications detected')
    root = location(args)
    root.mkdir(parents=True, exist_ok=True)
    deps = args.work_root.resolve() / 'deps'
    if not (deps / 'manifest.json').is_file():
        raise ValueError('Run prepare first; drivers are copied from the local cache only')
    board = BOARDS[args.board]
    defaults = [DEV / 'boards/configs/base.defaults', DEV / f'boards/configs/{args.board}.defaults']
    app = DEV / 'test_apps' / {'benchmark': 'benchmark', 'pixels': 'boards', 'native': 'boards',
                               'te-lifecycle': 'te_lifecycle', 'rgb-events': 'rgb_events', 'rgb-handoff': 'rgb_handoff', 'smoke': 'smoke'}[args.suite]
    flags = [f'-DPRESENT_BOARD={args.board}']
    planned = []
    buffer_policy = getattr(args, 'buffer_policy', 'logical-50')
    lvgl = None
    if args.suite == 'benchmark':
        lvgl = (args.lvgl_dir or args.cache / 'lvgl__lvgl_9.6.0~1_7d824107').resolve()
        if not (lvgl / 'lvgl.h').is_file():
            raise ValueError('Local LVGL required: --lvgl-dir PATH')
        alias = root / 'lvgl'
        if alias.is_symlink() and alias.resolve() != lvgl:
            alias.unlink()
        if not alias.exists():
            alias.symlink_to(lvgl, target_is_directory=True)
        planned = cases_for(args.board, args.angles, args.profiles)
        if args.implementation != 'adapter' and any(c['name'].startswith('ADAPTER_TE_') for c in planned):
            raise ValueError('ADAPTER_TE profiles describe adapter-native layouts; use --implementation adapter')
        (root / 'bench_cases.h').write_text(header(planned))
        if args.implementation == 'adapter' and not any(c['adapter_supported'] for c in planned):
            print('Selected cases have no adapter mapping; S31 TE baselines use explicit ADAPTER_TE_* profiles. SKIP entries are not capability claims.')
        defaults.append(DEV / 'boards/configs/lvgl.defaults')
        flags += [f'-DPRESENT_BOARD={args.board}', f'-DPRESENT_IMPLEMENTATION={args.implementation}',
                  f'-DPRESENT_CASE_DIR={root}', f'-DPRESENT_LVGL_DIR={alias}', f'-DPRESENT_PROFILE={"ON" if args.profile else "OFF"}']
    if args.suite in ('pixels', 'native'):
        flags += [f'-DPRESENT_MODE_ROTATION_MATRIX={"ON" if args.suite == "pixels" else "OFF"}',
                  f'-DPRESENT_NATIVE_ROTATION_MATRIX={"ON" if args.suite == "native" else "OFF"}']
    variant_defaults = []
    if xip_mode != 'default':
        variant_defaults.append(xip_defaults(xip_mode))
    if args.suite == 'benchmark' and args.board == 's31-rgb':
        variant_defaults.append('CONFIG_PV2_LEGACY_RGB_BOUNCE_CONFIRMED=n\n')
    if variant_defaults:
        config_overlay = root / 'variant.defaults'
        config_overlay.write_text(''.join(variant_defaults))
        defaults.append(config_overlay)
    flags += [f'-DPRESENT_DEPS_DIR={deps}']
    command = [sys.executable, idf() / 'tools/idf.py', '--preview', '-C', app, '-B', root / 'build',
               f'-DIDF_TARGET={board["target"]}', f'-DSDKCONFIG={root / "sdkconfig"}',
               '-DSDKCONFIG_DEFAULTS=' + ';'.join(map(str, defaults)), *flags, 'build']
    env = os.environ.copy()
    env['IDF_COMPONENT_MANAGER'] = '0'
    env['PRESENT_TEST_LCD_DRIVER_DIR'] = ''
    env['PRESENT_TEST_DRIVER_TRACE'] = 'OFF'
    trace_manifest = None
    if driver_trace:
        driver_trace = driver_trace.resolve()
        trace_manifest = json.loads((driver_trace / 'trace-manifest.json').read_text())
        if trace_manifest['idf_commit'] != subprocess.check_output(['git', '-C', str(idf()), 'rev-parse', 'HEAD'], text=True).strip():
            raise ValueError('Trace overlay IDF revision mismatch')
        for relative, expected in trace_manifest['files_sha256'].items():
            if sha(driver_trace / relative) != expected:
                raise ValueError('Driver trace overlay changed: ' + relative)
        env['PRESENT_TEST_LCD_DRIVER_DIR'] = str(driver_trace / 'esp_lcd')
        env['PRESENT_TEST_DRIVER_TRACE'] = 'ON'

    env['LVGL_VERSION'] = '9.6.0'
    env['PRESENT_TEST_BUFFER_POLICY'] = buffer_policy
    env['PRESENT_TEST_PROFILE'] = 'ON' if args.profile else 'OFF'
    if args.implementation == 'adapter':
        original = DEV.parent.parent / 'esp_lvgl_adapter'
        overlay = root / 'esp_lvgl_adapter'
        overlay.mkdir(exist_ok=True)
        for child in original.iterdir():
            if child.name in ('CMakeLists.txt', 'idf_component.yml'):
                continue
            link = overlay / child.name
            if child.name == 'src' and buffer_policy == 'logical-50':
                if link.is_symlink():
                    link.unlink()
                elif link.exists():
                    shutil.rmtree(link)
                shutil.copytree(child, link)
                manager = link / 'display/display_manager.c'
                manager.write_text(align_adapter_allocation(manager.read_text()))
                continue
            if link.is_dir() and not link.is_symlink() and child.name == 'src':
                shutil.rmtree(link)  # Previous isolated aligned overlay; never the checkout.
            if not link.exists():
                link.symlink_to(child, target_is_directory=child.is_dir())
        # Only fix the offline dependency name during IDF's early requirements
        # pass. Runtime sources are symlinked unchanged to the original adapter.
        (overlay / 'CMakeLists.txt').write_text((original / 'CMakeLists.txt').read_text().replace(
            'set(lvgl_name "lvgl__lvgl")', 'set(lvgl_name "lvgl")').replace('espressif__esp_lcd_touch', 'esp_lcd_touch'))
        env['PRESENT_TEST_ADAPTER_DIR'] = str(overlay)

    env.update({f'PRESENT_TEST_{k}': str(v) for k, v in {
        'PRESENT_BOARD': args.board, 'PRESENT_IMPLEMENTATION': args.implementation,
        'PRESENT_CASE_DIR': root, 'PRESENT_ROOT': DEV.parent}.items()})
    # A fresh per-profile SDKCONFIG avoids stale target/board values.
    (root / 'sdkconfig').unlink(missing_ok=True)
    with (root / 'build.log').open('w') as log:
        try:
            run(command, stdout=log, stderr=subprocess.STDOUT, env=env)
        except subprocess.CalledProcessError:
            print('\n'.join((root / 'build.log').read_text(errors='replace').splitlines()[-35:]))
            raise
    project = json.loads((root / 'build/project_description.json').read_text())
    lcd_component = Path(project['build_component_info']['esp_lcd']['dir']).resolve()
    expected_lcd = driver_trace / 'esp_lcd' if driver_trace else idf() / 'components/esp_lcd'
    if lcd_component != expected_lcd.resolve():
        raise ValueError('Unexpected LCD driver component: ' + str(lcd_component))
    sdk = (root / 'sdkconfig').read_text()
    validate_xip(sdk, xip_mode)
    comparable = comparable_sdkconfig(sdk)
    state = dict(buffer_schema=1 if args.suite == 'benchmark' else 0, buffer_policy=buffer_policy, memory_schema=1 if args.suite == 'benchmark' else 0, profile=args.profile, board=args.board, suite=args.suite, implementation=args.implementation, cases=planned,
                 idf_commit=subprocess.check_output(['git', '-C', str(idf()), 'rev-parse', 'HEAD'], text=True).strip(),
                 xip_mode=xip_mode, driver_trace=trace_manifest,
                 rgb_bounce_lines=(0 if args.board == 's31-rgb' else 10) if args.suite == 'benchmark' and args.board in ('s31-rgb', 's3-rgb') else None,
                 lvgl_version='9.6.0' if lvgl else None,
                 lvgl_benchmark_sha256=sha(lvgl / 'demos/benchmark/lv_demo_benchmark.c') if lvgl else None,
                 comparable_sdkconfig_sha256=hashlib.sha256(comparable.encode()).hexdigest(),
                 defaults_sha256={p.name: sha(p) for p in defaults},
                 firmware_sha256=sha(root / 'build' / json.loads((root / 'build/flasher_args.json').read_text())['app']['file']))
    source_dirs = [app / 'main', DEV / 'test_apps/benchmark/components/hw_init', DEV / 'test_apps/boards/main']
    state['test_sources_sha256'] = {str(p.relative_to(DEV)): sha(p) for directory in source_dirs for p in directory.rglob('*') if p.is_file() and p.suffix in ('.c', '.h', '.txt')}
    state['component_sources_sha256'] = {str(p.relative_to(DEV.parent.parent)): sha(p)
        for name in ('esp_display_present', 'esp_lv_present', 'esp_lvgl_adapter')
        for sub in ('src', 'include') for p in (DEV.parent.parent / name / sub).rglob('*')
        if p.is_file() and p.suffix in ('.c', '.h')}
    if args.suite == 'benchmark':
        state['expected_input_bytes'] = {f"{c['name']}/{c['rotation']}": expected_input_bytes(board, c, args.implementation, buffer_policy) for c in planned}
        if args.implementation == 'adapter' and buffer_policy == 'logical-50':
            state['adapter_allocation_shim_sha256'] = sha(overlay / 'src/display/display_manager.c')
            for key in list(state['component_sources_sha256']):
                if key.startswith('esp_lvgl_adapter/src/'):
                    state['component_sources_sha256'][key] = sha(overlay / key.removeprefix('esp_lvgl_adapter/'))
    (root / 'manifest.json').write_text(json.dumps(state, indent=2) + '\n')
    print('Build passed:', root)


def parse_log(text):
    rows = []
    meters = {}
    driver_observations = {}
    stages = {}
    memory = {}
    suite_memory = {}
    buffers = {}
    active = None
    errors = []
    for raw in text.splitlines():
        line = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', raw)
        mem = re.search(r'MEMORY mode=(\S+) rotation=(\d+) phase=(\S+)', line)
        if mem:
            values = {k: int(v) for k, v in re.findall(r'(internal_total|internal_free|psram_total|psram_free|internal_min_free|psram_min_free)=(\d+)', line)}
            if mem[1] == 'SUITE':
                suite_memory[mem[3]] = values
            else:
                memory.setdefault((mem[1], int(mem[2])), {})[mem[3]] = values
        m = re.search(r'RUN mode=(\S+).*rotation=(\d+)', line)
        if m:
            active = (m[1], int(m[2]))
        observation = re.search(r'DRIVER_TRACE mode=(\S+) rotation=(\d+)', line)
        if observation:
            driver_observations[observation[1], int(observation[2])] = {k: int(v) for k, v in re.findall(
                r'\b(kind|bounce_bytes|requests|events|superseded|active_submits|target_diffs|unresolved|outside_pool|pending|saved)=(\d+)', line)}
        if 'BUFFER_AUDIT:' in line:
            identity = re.search(r'mode=(\S+) rotation=(\d+)', line)
            if identity:
                buffers[identity[1], int(identity[2])] = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', line)}
        if 'PRESENT_PROFILE:' in line and active:
            stage = re.search(r'stage=(\S+)', line)
            if stage:
                stages.setdefault(active, {})[stage[1]] = {k: int(v) for k, v in re.findall(r'(n|sum_us|avg_us|max_us|bytes|errors)=(\d+)', line)}
        if 'TE_METER:' in line and active:
            meters[active] = {k: int(v) for k, v in re.findall(r'(admitted|wait_avg_us|wait_max_us|tx_n|tx_avg_us|tx_max_us)=(\d+)', line)}
        m = re.search(r'(PASS|SKIP) mode=(\S+).*rotation=(\d+)', line)
        if m:
            row = dict(status=m[1], mode=m[2], rotation=int(m[3]))
            row.update({k: float(v) if '.' in v else int(v) for k, v in re.findall(
                r'(frames|transport_fps|lvgl_fps|cpu|render|flush|scenes|early_events|driver_events|wait_avg_us|wait_max_us|idle_frame_events|idle_vsync|active_write)=([\d.]+)', line)})
            row.update(meters.get((m[2], int(m[3])), {}))
            if (m[2], int(m[3])) in driver_observations:
                row['driver_trace'] = driver_observations[m[2], int(m[3])]
            if (m[2], int(m[3])) in buffers:
                row['buffer_audit'] = buffers[m[2], int(m[3])]
            if (m[2], int(m[3])) in memory:
                row['memory'] = memory[(m[2], int(m[3]))]
            if (m[2], int(m[3])) in stages:
                row['stages'] = stages[(m[2], int(m[3]))]
            if m[1] == 'SKIP':
                row['reason'] = line.split('reason=', 1)[-1]
            rows.append(row)
        if any(t in line for t in ('assert failed', 'Guru Meditation', 'ESP_ERROR_CHECK failed', 'Stack canary')) or (
                re.search(r'\bE \(', line) and 'GPIO isr service already installed' not in line):
            errors.append(line.strip())
    return dict(complete='SUITE_PASS ' in text or 'TE_SUITE_PASS ' in text or 'MATRIX_PASS ' in text or 'present_v2 smoke passed' in text,
                rows=rows, errors=errors, suite_memory=suite_memory)


def report(args):
    root = location(args)
    result = parse_log((root / 'live.log').read_text(errors='replace'))
    manifest = json.loads((root / 'manifest.json').read_text())
    if args.suite == 'benchmark':
        expected = {(c['name'], c['rotation']) for c in manifest['cases']}
        found = {(c['mode'], c['rotation']) for c in result['rows']}
        result['matrix_complete'] = expected == found and len(found) == len(result['rows'])
    elif args.suite == 'rgb-handoff':
        profiles = ('DOUBLE_FULL', 'TRIPLE_FULL', 'DOUBLE_DIRECT', 'DOUBLE_PARTIAL', 'TRIPLE_PARTIAL', 'AUTO')
        expected = {(f'{name}_C{core}_D{delay}', angle) for name in profiles
                    for core in (0, 1) for delay in (0, 50) for angle in (0, 90, 180, 270)}
        found = {(r['mode'], r['rotation']) for r in result['rows']}
        result['matrix_complete'] = (expected == found and len(result['rows']) == 96 and
                                     all(r.get('frames') == 4 and r.get('driver_events', 0) >= 4 and
                                         ('_D50' not in r['mode'] or r.get('early_events', 0) > 0)
                                         for r in result['rows']))
    elif args.suite == 'rgb-events':
        expected = {(f'RGB_{kind}_FB{n}_CORE{core}', 0) for kind in ('BOUNCE', 'DIRECT') for n in (1, 2, 3) for core in (0, 1)}
        found = {(r['mode'], r['rotation']) for r in result['rows']}
        result['matrix_complete'] = expected == found and len(result['rows']) == 12 and all(r.get('frames') == 80 for r in result['rows'])
    else:
        result['matrix_complete'] = result['complete']
    if args.suite == 'benchmark' and manifest.get('memory_schema') == 1:
        phases = ('before_open', 'after_open', 'after_run', 'after_close')
        fields = ('internal_total', 'internal_free', 'psram_total', 'psram_free', 'internal_min_free', 'psram_min_free')
        records = [r.get('memory', {}) for r in result['rows'] if r['status'] == 'PASS']
        result['memory_complete'] = all(all(p in m and all(k in m[p] for k in fields) for p in phases) for m in records)
        result['memory_complete'] &= all(p in result['suite_memory'] for p in ('before_init', 'after_init'))
        if not result['memory_complete']:
            result['errors'].append('Missing required memory measurements')
    if args.suite == 'benchmark' and manifest.get('buffer_schema') == 1:
        result['buffers_verified'] = True
        for row in result['rows']:
            if row['status'] != 'PASS':
                continue
            audit = row.get('buffer_audit', {})
            expected = manifest['expected_input_bytes'][f"{row['mode']}/{row['rotation']}"]
            valid = audit.get('flushes', 0) > 0 and audit.get('rendered', 0) > 0
            if expected is not None:
                valid &= audit.get('active_bytes') == audit.get('min_bytes') == expected == audit.get('max_bytes')
            if not valid:
                result['buffers_verified'] = False
                result['errors'].append(f"Missing or mismatched buffer audit: {row['mode']}/{row['rotation']}")
    if manifest.get('driver_trace'):
        passed = [row for row in result['rows'] if row['status'] == 'PASS']
        result['diagnostics_complete'] = bool(passed) and all(row.get('driver_trace', {}).get('requests', 0) > 0 and row.get('driver_trace', {}).get('events', 0) > 0 for row in passed)
        if not result['diagnostics_complete']:
            result['errors'].append('Missing active driver observation; diagnostic not verified')
        # Observations remain data, never corrective actions or hardware errors.
    (root / 'results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    if not result['complete'] or result['errors'] or not result['matrix_complete']:
        raise ValueError('Incomplete/failed run; results retained, not marked as passed')


def compare(args):
    base = args.work_root.resolve() / args.board / 'benchmark'
    results, manifests = {}, {}
    for impl in ('present', 'adapter'):
        root = base / variant_name(args, impl)
        results[impl] = json.loads((root / 'results.json').read_text())
        manifests[impl] = json.loads((root / 'manifest.json').read_text())
        if not results[impl]['complete'] or results[impl]['errors'] or not results[impl]['matrix_complete']:
            raise ValueError('Cannot compare incomplete/failed suites')
    for key in ('xip_mode', 'driver_trace', 'rgb_bounce_lines', 'board', 'cases', 'idf_commit', 'lvgl_benchmark_sha256', 'comparable_sdkconfig_sha256', 'defaults_sha256', 'test_sources_sha256'):
        if manifests['present'].get(key) != manifests['adapter'].get(key):
            raise ValueError('Comparison configurations differ: ' + key)
    if manifests['present'].get('buffer_policy', 'legacy-physical-50') != manifests['adapter'].get('buffer_policy', 'legacy-physical-50'):
        raise ValueError('Comparison allocation policies differ')
    if manifests['present'].get('memory_schema') != manifests['adapter'].get('memory_schema'):
        raise ValueError('Comparison memory schemas differ')
    lookup = {impl: {(r['mode'], r['rotation']): r for r in result['rows']} for impl, result in results.items()}
    if manifests['present'].get('buffer_schema') != manifests['adapter'].get('buffer_schema'):
        raise ValueError('Comparison buffer schemas differ')
    if manifests['present'].get('buffer_schema') == 1:
        for impl in ('present', 'adapter'):
            if not results[impl].get('buffers_verified'):
                raise ValueError('Comparison requires verified buffer audits')
        for key in lookup['present'].keys() & lookup['adapter'].keys():
            left, right = lookup['present'][key], lookup['adapter'][key]
            if left['status'] != 'PASS' or right['status'] != 'PASS':
                continue
            for field in ('active_bytes', 'min_bytes', 'max_bytes', 'internal', 'psram'):
                if left.get('buffer_audit', {}).get(field) != right.get('buffer_audit', {}).get(field):
                    raise ValueError(f'Comparison input buffers differ: {key}/{field}')
    rows = []
    for c in manifests['present']['cases']:
        row = dict(board=args.board, mode=c['name'], rotation=c['rotation'])
        for impl in ('present', 'adapter'):
            value = lookup[impl].get((c['name'], c['rotation']))
            row[impl + '_fps'] = '-' if value and value['status'] == 'SKIP' else value.get('lvgl_fps', '未测') if value else '未测'
        for impl in ('present', 'adapter'):
            value = lookup[impl].get((c['name'], c['rotation']))
            mem = value.get('memory', {}) if value else {}
            skipped = value and value['status'] == 'SKIP'
            for heap in ('internal', 'psram'):
                opened, before, run = (mem.get(p, {}) for p in ('after_open', 'before_open', 'after_run'))
                prefix = impl + '_' + heap
                row[prefix + '_open_used_bytes'] = '-' if skipped else opened[heap + '_total'] - opened[heap + '_free'] if opened else '未测'
                row[prefix + '_open_delta_bytes'] = '-' if skipped else before[heap + '_free'] - opened[heap + '_free'] if before and opened else '未测'
                row[prefix + '_min_free_bytes'] = '-' if skipped else run.get(heap + '_min_free', '未测')
        rows.append(row)
    output_name = 'comparison' + ('-xip-' + args.xip if getattr(args, 'xip', 'default') != 'default' else '') + ('-driver-trace' if getattr(args, 'driver_trace', None) else '')
    with (base / (output_name + '.csv')).open('w') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    lines = ['| Board | Mode | Angle | present FPS | adapter FPS |', '|---|---|---:|---:|---:|']
    lines += [f"| {r['board']} | {r['mode']} | {r['rotation']} | {r['present_fps']} | {r['adapter_fps']} |" for r in rows]
    (base / (output_name + '.md')).write_text('\n'.join(lines) + '\n')
    print('\n'.join(lines))


def esptool(args, before, after, *command):
    if not args.port:
        raise ValueError('--port is required; ports are never assumed')
    return [sys.executable, '-m', 'esptool', '--chip', BOARDS[args.board]['target'], '-p', args.port,
            '-b', '921600', '--before', before, '--after', after,
            *command]


def identify(args):
    command = esptool(args, 'default-reset', 'no-reset', 'read-mac')
    command[command.index('-b') + 1] = '115200'
    p = subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(p.stdout)
    if BOARDS[args.board]['mac'] not in p.stdout.lower():
        raise ValueError('MAC mismatch: no flash contents changed')


def hardware(args):
    root = location(args)
    backup = args.work_root.resolve() / args.board / 'backup'
    backup.mkdir(parents=True, exist_ok=True)
    image, record = backup / 'original.bin', backup / 'manifest.json'
    policy_path = args.work_root.resolve() / 'hardware-policy.json'
    if args.action == 'restore' and policy_path.exists():
        policy = json.loads(policy_path.read_text())
        if policy.get('restore_original') is False:
            (backup / 'restored.json').write_text(json.dumps(dict(
                verified_full_flash=False, skipped_by_user=True,
                reason=policy.get('reason', 'Restore disabled by work-root policy'),
                retained_firmware=str(root),
            ), ensure_ascii=False, indent=2) + '\n')
            print('Original firmware restore skipped by user policy; no serial access')
            return
    identify(args)
    before = 'no-reset' if args.port.startswith('/dev/ttyACM') else 'default-reset'
    if args.action == 'backup':
        if image.exists() or record.exists():
            raise ValueError('Backup exists; select a fresh --work-root instead of overwriting')
        run(esptool(args, before, 'no-reset', 'read-flash', '0', hex(BOARDS[args.board]['flash_bytes']), str(image)))
        run(esptool(args, before, 'hard-reset', 'verify-flash', '0', str(image)))
        record.write_text(json.dumps(dict(board=args.board, mac=BOARDS[args.board]['mac'], sha256=sha(image), bytes=image.stat().st_size, verified=True), indent=2)+'\n')
        return
    info = json.loads(record.read_text())
    if info.get('board') != args.board or info.get('mac') != BOARDS[args.board]['mac'] or not info.get('verified') or info['sha256'] != sha(image) or image.stat().st_size != BOARDS[args.board]['flash_bytes']:
        raise ValueError('Valid verified backup for this MAC is required')
    if args.action == 'flash':
        if not (root / 'manifest.json').is_file():
            raise ValueError('Build first')
        manifest = json.loads((root / 'manifest.json').read_text())
        app = json.loads((root / 'build/flasher_args.json').read_text())['app']['file']
        if sha(root / 'build' / app) != manifest['firmware_sha256']:
            raise ValueError('Firmware changed since build manifest was recorded; rebuild first')
        run(esptool(args, before, 'hard-reset', 'write-flash', '@flash_args'), cwd=root / 'build')
    elif args.action == 'restore':
        with (backup / 'restore.log').open('w') as log:
            run(esptool(args, before, 'no-reset', 'write-flash', '--flash-mode', 'keep', '--flash-freq', 'keep', '--flash-size', 'keep', '0', str(image)), stdout=log, stderr=subprocess.STDOUT)
        with (backup / 'restore-verify.log').open('w') as log:
            run(esptool(args, before, 'hard-reset', 'verify-flash', '0', str(image)), stdout=log, stderr=subprocess.STDOUT)
        (backup / 'restored.json').write_text(json.dumps(dict(verified_full_flash=True, sha256=info['sha256']), indent=2)+'\n')


def capture(args):
    import serial
    from esptool.reset import HardReset
    if not args.port:
        raise ValueError('--port required')
    root = location(args)
    if not (root / 'manifest.json').exists():
        raise ValueError('Build first')
    identify(args)
    serial_port = serial.Serial(port=None, baudrate=115200, timeout=.2)
    serial_port.dtr = serial_port.rts = False
    serial_port.port = args.port
    serial_port.open()
    HardReset(serial_port, uses_usb=args.port.startswith('/dev/ttyACM'))()
    start = last = time.monotonic()
    pending = b''
    stop = ('SUITE_PASS ', 'TE_SUITE_PASS ', 'MATRIX_PASS ', 'MATRIX_DONE ', 'present_v2 smoke passed')
    with serial_port, (root / 'live.log').open('wb') as log:
        while time.monotonic()-start < args.timeout:
            data = serial_port.read(serial_port.in_waiting or 1)
            log.write(data)
            log.flush()
            pending += data
            while b'\n' in pending:
                raw, pending = pending.split(b'\n', 1)
                line = raw.decode(errors='replace')
                if any(k in line for k in ('RUN ', 'PASS ', 'SKIP ', 'RGB_EVENT_BEGIN ', 'TE_METER:', 'PRESENT_PROFILE:', 'FAIL', 'E (')):
                    print(line.strip(), flush=True)
                if any(k in line for k in stop):
                    return
                if parse_log(line)['errors']:
                    raise ValueError('Hardware error; raw log retained')
            if time.monotonic()-last > 20:
                print(f'Capturing {args.board}/{args.suite}/{args.implementation}: {int(time.monotonic()-start)}s', flush=True)
                last = time.monotonic()
    raise ValueError('Capture timed out; not a pass')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('list', 'prepare', 'build', 'backup', 'flash', 'capture', 'report', 'compare', 'restore', 'host', 'format'))
    p.add_argument('--board', choices=BOARDS)
    p.add_argument('--suite', choices=SUITES, default='benchmark')
    p.add_argument('--implementation', choices=('present', 'adapter'), default='present')
    p.add_argument('--work-root', type=Path, default=Path(os.environ.get('PRESENT_TEST_WORK_ROOT', '/tmp/esp-display-present-tests')))
    p.add_argument('--cache', type=Path, default=Path.home()/'.cache/Espressif/ComponentManager/service_d92d8f1e')
    p.add_argument('--lvgl-dir', type=Path)
    p.add_argument('--angles', type=lambda v: [int(x) for x in v.split(',')], default=[0, 90, 180, 270])
    p.add_argument('--profiles', type=lambda v: v.split(','))
    p.add_argument('--buffer-policy', choices=POLICIES, default='logical-50', help='PARTIAL byte-capacity alignment; legacy policy reproduces old allocation')
    p.add_argument('--xip', choices=('default', 'on', 'off'), default='default', help='Isolated RGB/DSI benchmark XIP configuration')
    p.add_argument('--driver-trace', type=Path, help='Verified read-only driver observer overlay; separate diagnostic output')
    p.add_argument('--profile', action='store_true', help='Benchmark-only stage counters; isolated present-profile output')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--check', action='store_true')
    p.add_argument('--port')
    p.add_argument('--timeout', type=int, default=2400)
    args = p.parse_args()
    if args.action == 'compare' and args.profile:
        p.error('Compare uninstrumented baseline runs; profiling has separate output')
    if args.action not in ('list', 'prepare', 'host', 'format') and not args.board:
        p.error('--board required')
    if any(a not in (0, 90, 180, 270) for a in args.angles) or not args.angles or len(set(args.angles)) != len(args.angles):
        p.error('--angles requires distinct 0,90,180,270 values')
    # Never put firmware backups or build products in the source tree.
    if args.work_root.resolve().is_relative_to(DEV.parents[4].resolve()):
        p.error('--work-root must be outside the repository checkout')
    if args.action in ('host', 'format'):
        command = [sys.executable, DEV / 'tools/dev.py', 'test' if args.action == 'host' else 'format']
        if args.action == 'host':
            command += ['--build-dir', args.work_root.resolve() / 'host']
            if args.sanitize: command += ['--sanitize']
            local_lvgl = args.lvgl_dir or args.cache / 'lvgl__lvgl_9.6.0~1_7d824107'
            if (local_lvgl / 'lvgl.h').exists(): command += ['--lvgl-dir', local_lvgl]
        elif args.check: command += ['--check']
        run(command)
    elif args.action == 'list':
        print(json.dumps(dict(boards=BOARDS, suites=SUITES,
             specialist_tools={'host': 'dev/tools/dev.py test --sanitize --with-lvgl',
                               'legacy': 'dev/test_apps/legacy_compat/README.md',
                               'scanout-boundary': 'dev/test_apps/scanout_boundary/README.md'}), ensure_ascii=False, indent=2))
    elif args.action == 'prepare':
        prepare(args)
    elif args.action in ('backup', 'flash', 'restore'):
        hardware(args)
    else:
        globals()[args.action](args)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error)) from None
