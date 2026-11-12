# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Check private header self-containment and intentional dependency boundaries."""
import argparse
import pathlib
import shlex
import subprocess
import tempfile

HEADERS = (
    'pixel/present_pixel_types.h',
    'pixel/present_pixel_ops.h',
    'pixel/present_pixel_ops_internal.h',
    'pixel/present_pixel_engine.h',
    'pixel/present_executor.h',
    'backend/present_backend_types.h',
    'backend/present_backend_config.h',
    'backend/present_backend.h',
    'platform/lcd/present_lcd_io.h',
    'platform/lcd/present_lcd_fb_scanout.h',
    'platform/lcd/present_lcd_rgb_info.h',
    'core/present_transfer_tracker.h',
    'core/present_engine_types.h',
    'config/present_engine_config.h',
    'core/present_engine.h',
    'core/present_session.h',
    'core/present_control.h',
    'core/present_producer.h',
    'core/present_completion_tracker.h',
    'resources/present_memory.h',
    'config/present_mode.h',
    'config/present_plan.h',
    'backend/present_output_caps.h',
)
FORBIDDEN = {
    'pixel/present_pixel_types.h': ('pixel/present_pixel_engine.h', 'pixel/present_pixel_ops.h', 'platform/present_os.h', 'resources/present_memory.h'),
    'pixel/present_pixel_ops.h': ('pixel/present_pixel_engine.h', 'platform/present_os.h', 'resources/present_memory.h'),
    'pixel/present_pixel_ops_internal.h': ('pixel/present_pixel_engine.h', 'platform/present_os.h'),
    'pixel/present_executor.h': ('pixel/present_pixel_engine.h',),
    'backend/present_backend_types.h': ('pixel/present_pixel_engine.h', 'pixel/present_pixel_ops.h', 'backend/present_backend.h', 'backend/present_backend_config.h', 'platform/te/present_te_timing.h'),
    'backend/present_backend_config.h': ('backend/present_backend.h',),
    'backend/present_backend.h': ('backend/present_backend_config.h',),
    'config/present_engine_config.h': ('backend/present_backend.h',),
    'core/present_engine.h': ('backend/present_backend.h',),
    'core/present_control.h': ('backend/present_backend.h', 'backend/present_backend_config.h'),
    'core/present_producer.h': ('backend/present_backend.h', 'backend/present_backend_config.h'),
    'platform/lcd/present_lcd_io.h': ('backend/present_backend.h', 'backend/present_backend_config.h'),
    'platform/lcd/present_lcd_fb_scanout.h': ('backend/present_backend.h', 'backend/present_backend_config.h'),
    'core/present_transfer_tracker.h': ('backend/present_backend.h', 'backend/present_backend_config.h'),
    'resources/present_memory.h': (
        'core/present_engine.h', 'config/present_engine_config.h', 'backend/present_backend.h'),
    'core/present_engine_types.h': ('config/present_engine_config.h', 'backend/present_backend.h'),
    'core/present_session.h': ('config/present_engine_config.h', 'backend/present_backend.h'),
    'backend/present_output_caps.h': ('backend/present_backend.h',),
    'config/present_mode.h': ('core/present_engine.h', 'esp_display_present.h', 'backend/present_backend.h'),
    'config/present_plan.h': ('core/present_engine.h', 'esp_display_present.h', 'backend/present_backend.h'),
    'core/present_completion_tracker.h': ('core/present_engine.h', 'config/present_engine_config.h', 'backend/present_backend_config.h'),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cc', required=True)
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    component = pathlib.Path(__file__).resolve().parents[2]
    flags = ['-DPRESENT_HOST=1', '-I' + str(component / 'src'),
             '-I' + str(component / 'include'), '-I' + str(component / 'dev/tests/support')]
    with tempfile.TemporaryDirectory(prefix='present-header-contract-') as temp:
        for header in HEADERS:
            for compiler, suffix, standard in ((args.cc, '.c', 'c11'), (args.cxx, '.cpp', 'c++17')):
                source = pathlib.Path(temp) / ('header' + suffix)
                source.write_text('#include "' + header + '"\n')
                command = [compiler, '-std=' + standard, '-Wall', '-Wextra', '-Werror', *flags]
                subprocess.run([*command, '-fsyntax-only', str(source)], check=True)
                output = subprocess.check_output([*command, '-M', str(source)], text=True)
                dependencies = {pathlib.Path(p).resolve() for p in shlex.split(output.replace('\\\n', ' '))[1:]}
                for forbidden in FORBIDDEN.get(header, ()):
                    search = 'include' if forbidden == 'esp_display_present.h' else 'src'
                    assert component / search / forbidden not in dependencies, (header, forbidden, suffix)
    print(f'{len(HEADERS)} private headers compile independently in C/C++; dependency boundaries hold')


if __name__ == '__main__':
    main()
