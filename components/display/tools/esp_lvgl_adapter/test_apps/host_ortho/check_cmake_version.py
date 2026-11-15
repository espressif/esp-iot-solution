# SPDX-FileCopyrightText: 2026 youkorr
#
# SPDX-License-Identifier: MIT
"""Exercise the adapter's actual LVGL discovery/version guards with CMake.

Only IDF property access is stubbed. The discovery/version code and public
component requirements are extracted verbatim from the production CMake file.
No LVGL sources are patched, downloaded or built by this check.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ADAPTER = HERE.parents[1]


def _production_blocks(cmake_path):
    source = cmake_path.read_text()
    start = 'idf_build_get_property(build_components BUILD_COMPONENTS)'
    end = '\nset(ADAPTER_SRCS\n'
    if source.count(start) != 1 or source.count(end) != 1:
        raise ValueError('Cannot locate the production LVGL discovery/version block')
    block = source[source.index(start):source.index(end)]
    requirements = re.findall(r'^set\(ADAPTER_REQ\s+[^)]*\)', source, re.M)
    if len(requirements) != 1:
        raise ValueError('Cannot locate the production public component requirements')
    return block, requirements[0]


def _quote(value):
    return str(value).replace('\\', '/').replace('"', '\\"')


def run_cmake_checks(work, cmake_path=None):
    """Write results below work/test and return the successful case records."""
    work = Path(work).resolve()
    cmake_path = Path(cmake_path or ADAPTER / 'CMakeLists.txt').resolve()
    compiler = shutil.which('cmake')
    if compiler is None:
        raise RuntimeError('CMake is required for the adapter configuration regression')
    before = hashlib.sha256(cmake_path.read_bytes()).hexdigest()
    block, requirements = _production_blocks(cmake_path)
    root = work / 'test' / 'cmake-version-checks'
    root.mkdir(parents=True, exist_ok=True)
    stub = '''
function(idf_build_get_property output property)
    if(property STREQUAL "BUILD_COMPONENTS")
        if(CMAKE_BUILD_EARLY_EXPANSION)
            set(${output} "" PARENT_SCOPE)
        else()
            set(${output} "lvgl" PARENT_SCOPE)
        endif()
    elseif(property STREQUAL "BUILD_COMPONENT_DIRS")
        set(${output} "${CASE_LVGL_DIR}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unexpected IDF build property: ${property}")
    endif()
endfunction()
function(idf_component_get_property output component property)
    if(NOT component STREQUAL "lvgl")
        message(FATAL_ERROR "Wrong LVGL component discovery: ${component}")
    endif()
    if(property STREQUAL "COMPONENT_DIR")
        set(${output} "${CASE_LVGL_DIR}" PARENT_SCOPE)
    elseif(property STREQUAL "COMPONENT_VERSION")
        set(${output} "${CASE_MANIFEST_VERSION}" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unexpected IDF component property: ${property}")
    endif()
endfunction()
'''
    cases = []
    for version in ('9.5.0', '9.6.0'):
        for enabled in (False, True):
            cases.append(dict(name='local-' + version + ('-on' if enabled else '-off'),
                              header=version, enabled=enabled, expected=version))
    cases += [
        dict(name='local-9.4-rejected', header='9.4.2', enabled=True, reject=True),
        dict(name='local-9.4-disabled', header='9.4.2', enabled=False, expected='9.4.2'),
        dict(name='local-9.7-rejected', header='9.7.0', enabled=True, reject=True),
        dict(name='early-discovery-deferred', header='9.4.2', enabled=True, early=True, expected='9.0.0'),
        dict(name='managed-version-9.6', header='9.4.2', manifest='9.6.1', enabled=True, expected='9.6.1'),
        dict(name='early-managed-deferred', header='9.4.2', manifest='9.4.2', enabled=True,
             early=True, expected='9.4.2'),
        dict(name='environment-version-rejected', header='9.6.0', override='9.4.2', enabled=True, reject=True),
    ]
    results = []
    for case in cases:
        folder = root / case['name']
        lvgl = folder / 'lvgl'
        header = lvgl / ('include/lvgl/lv_version.h' if case['header'].startswith('9.6') else 'lv_version.h')
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_text(''.join(f'#define LVGL_VERSION_{name} {value}\n'
                                  for name, value in zip(('MAJOR', 'MINOR', 'PATCH'), case['header'].split('.'))))
        script = folder / 'check.cmake'
        script.write_text('cmake_minimum_required(VERSION 3.16)\n'
                          + f'set(CASE_LVGL_DIR "{_quote(lvgl)}")\n'
                          + f'set(CASE_MANIFEST_VERSION "{case.get("manifest", "")}")\n'
                          + 'set(CMAKE_BUILD_EARLY_EXPANSION ' + ('TRUE' if case.get('early') else 'FALSE') + ')\n'
                          + 'set(CONFIG_ESP_LVGL_ADAPTER_ROTATE_IN_DRAW ' + ('TRUE' if case['enabled'] else 'FALSE') + ')\n'
                          + stub + '\n' + block + '\n' + requirements + '\n'
                          + 'if(NOT "espressif__esp_lcd_touch" IN_LIST ADAPTER_REQ)\n'
                          + '  message(FATAL_ERROR "Touch must remain a public requirement")\nendif()\n'
                          + 'message(STATUS "DETECTED=${lvgl_ver};COMPONENT=${lvgl_name};PUBLIC_REQ=${ADAPTER_REQ}")\n')
        env = dict(os.environ)
        env.pop('LVGL_VERSION', None)
        if 'override' in case:
            env['LVGL_VERSION'] = case['override']
        result = subprocess.run([compiler, '-P', str(script)], capture_output=True, text=True,
                                env=env, timeout=20)
        diagnostic = result.stdout + result.stderr
        if case.get('reject'):
            passed = (result.returncode != 0
                      and 'ROTATE_IN_DRAW supports LVGL 9.5.x and 9.6.x' in diagnostic)
        else:
            passed = (result.returncode == 0
                      and f'DETECTED={case["expected"]};COMPONENT=lvgl;' in diagnostic
                      and 'espressif__esp_lcd_touch' in result.stdout)
        row = dict(case=case['name'], passed=passed, returncode=result.returncode,
                   stdout=result.stdout, stderr=result.stderr)
        results.append(row)
        (root / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        if not passed:
            raise AssertionError(f'Production CMake regression failed: {row}')
    if hashlib.sha256(cmake_path.read_bytes()).hexdigest() != before:
        raise AssertionError('Production CMake changed during regression')
    receipt = dict(source=str(cmake_path), source_sha256=before,
                   actual_production_blocks=True, idf_property_access_stubbed=True,
                   full_idf_configuration=False, cases=results)
    (work / 'test' / 'cmake-version-checks.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'  CMake discovery/version/public requirements: {len(results)} cases passed', flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cmake', type=Path, help='Production adapter CMakeLists.txt')
    parser.add_argument('--build-dir', type=Path, help='Retain the small configuration fixtures')
    args = parser.parse_args()
    if args.build_dir:
        run_cmake_checks(args.build_dir, args.cmake)
    else:
        with tempfile.TemporaryDirectory(prefix='adapter-cmake-') as folder:
            run_cmake_checks(folder, args.cmake)


if __name__ == '__main__':
    main()
