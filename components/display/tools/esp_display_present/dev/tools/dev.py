# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""One build/test entry point; tests link whole production translation units."""
import argparse
import os
import shutil
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('build', 'test', 'idf-build', 'format'))
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--target', default='esp32s3', help='ESP-IDF compile target')
    parser.add_argument('--with-lvgl', action='store_true', help='Use existing local LVGL source; never download')
    parser.add_argument('--lvgl-dir', type=Path, help='Explicit LVGL 9 source directory (implies --with-lvgl)')
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 1, 8))
    parser.add_argument('--check', action='store_true', help='Check formatting without writing (format action only)')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    if args.check and args.action != 'format':
        parser.error('--check requires format')
    if args.action == 'format':
        formatter = shutil.which('astyle_py')
        if not formatter:
            parser.error('astyle_py not found; install the repository formatting tool')
        subprocess.run([formatter, '--astyle-version=3.4.7', '--version'], check=True)
        rules = ROOT.parents[4] / 'tools/ci/astyle-rules.yml'
        files = sorted(p for directory in ('../src', '../include', '../../esp_lv_present/src', '../../esp_lv_present/include', 'tests', 'test_apps/smoke/main', 'test_apps/benchmark/main', 'test_apps/benchmark/components/hw_init', 'test_apps/te_lifecycle/main', 'test_apps/rgb_events/main', '../test_apps/main')
                       for p in (ROOT / directory).rglob('*') if p.suffix in ('.c', '.h', '.cpp'))
        # Preserve the five stable application headers byte-for-byte on writes;
        # check mode still checks them against the repository rules.
        if not args.check:
            stable = {(ROOT / '../include').resolve(), (ROOT / '../../esp_lv_present/include').resolve()}
            files = [p for p in files if p.parent.resolve() not in stable]
        command = [formatter, '--astyle-version=3.4.7', f'--rules={rules}']
        if args.check:
            command.append('--dry-run')
        subprocess.run([*command, *(str(p) for p in files)], check=True)
        return
    lvgl = None
    if args.with_lvgl or args.lvgl_dir:
        candidates = [args.lvgl_dir] if args.lvgl_dir else [
            ROOT.parents[4] / 'examples/display/gui/lvgl_present_benchmark/managed_components/lvgl__lvgl',
            ROOT.parents[1] / 'esp_lvgl_adapter/test_apps/lvgl9/managed_components/lvgl__lvgl',
            *sorted((Path.home() / '.cache/Espressif/ComponentManager').glob('*/lvgl__lvgl_9.5.0_*')),
        ]
        lvgl = next((p.resolve() for p in candidates if p and (p / 'lvgl.h').is_file()), None)
        if lvgl is None:
            parser.error('Local LVGL not found; use --lvgl-dir PATH. No dependency will be downloaded.')
    if args.action == 'idf-build':
        if args.sanitize:
            parser.error('--sanitize applies only to host build/test')
        idf_path = os.environ.get('IDF_PATH')
        if not idf_path:
            parser.error('Activate ESP-IDF with its export.sh before idf-build')
        suffix = '-lvgl' if lvgl else ''
        build = (args.build_dir or Path(os.environ.get('PRESENT_TEST_WORK_ROOT', '/tmp/esp-display-present-tests')) / 'smoke' / f'{args.target}{suffix}').resolve()
        env = os.environ.copy()
        env['IDF_COMPONENT_MANAGER'] = '0'
        subprocess.run([sys.executable, str(Path(idf_path) / 'tools/idf.py'),
                        '--preview', '-C', str(ROOT / 'test_apps/smoke'), '-B', str(build),
                        '-D', f'IDF_TARGET={args.target}',
                        '-D', f"PRESENT_LVGL_DIR={lvgl or ''}",
                        '-D', f"SDKCONFIG={build / 'sdkconfig'}", 'build'], check=True, env=env)
        return
    mode = ('sanitize' if args.sanitize else 'host') + ('-lvgl' if lvgl else '')
    build = (args.build_dir or Path(os.environ.get('PRESENT_TEST_WORK_ROOT', '/tmp/esp-display-present-tests')) / mode).resolve()
    subprocess.run(['cmake', '-S', str(ROOT), '-B', str(build),
                    '-DCMAKE_BUILD_TYPE=Debug',
                    f"-DPRESENT_LVGL_DIR={lvgl or ''}",
                    f"-DPRESENT_SANITIZE={'ON' if args.sanitize else 'OFF'}"], check=True)
    subprocess.run(['cmake', '--build', str(build), '--parallel', str(args.jobs)], check=True)
    if args.action == 'test':
        subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], check=True)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.returncode) from None
