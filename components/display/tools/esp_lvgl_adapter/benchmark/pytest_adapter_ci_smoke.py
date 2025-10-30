# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
ESP LVGL Adapter FPS Benchmark Test Runner

This test suite runs FPS benchmark tests in headless mode.
No physical hardware required - uses virtual display with dummy draw.
"""

from typing import Callable
import json
import pytest
from pytest_embedded import Dut

def test_adapter_fps_benchmark(dut: Dut, record_property: Callable[[str, object], None]) -> None:
    """
    FPS benchmark test - headless mode.
    Tests multiple display resolutions without physical hardware.
    Total: 53 resolutions from 128x64 to 2048x1080.
    """
    dut.expect('ESP LVGL Adapter FPS Benchmark Test', timeout=30)
    idf_target = dut.expect(r'ESP-IDF Target:\s+(esp\w+)', timeout=5).group(1).decode('utf-8')
    idf_version = dut.expect(r'ESP-IDF Version:\s+(v[\w\-\.\_]+)', timeout=5).group(1).decode('utf-8')
    pixel_format = dut.expect(r'Pixel format:\s+(RGB888|RGB565)', timeout=5).group(1).decode('utf-8')
    print(f'Target: {idf_target}, ESP-IDF Version: {idf_version}, Pixel format: {pixel_format}')

    record_property('idf_target', idf_target)
    record_property('idf_version', idf_version)
    record_property('pixel_format', pixel_format)

    dut.expect_exact('Press ENTER to see the list of tests.', timeout=30)
    dut.write('[fps]')
    dut.expect('FPS Benchmark Suite', timeout=30)

    resolution_count = 53

    for i in range(resolution_count):
        test_num = i + 1

        # Updated pattern to match new log format
        test_info = dut.expect(rf'\[{test_num}/{resolution_count}\] Resolution: (\d+)x(\d+)', timeout=30)
        width = test_info.group(1).decode('utf-8')
        height = test_info.group(2).decode('utf-8')
        resolution = f'{width}x{height}'

        print(f'[{test_num}/{resolution_count}] Testing {resolution}...')
        record_property('fps_progress', f'{test_num}/{resolution_count} {resolution}')

        dut.expect(f'Benchmarking {resolution}', timeout=10)

        stable_log_count = 0
        while True:
            testing_status = dut.expect(r'(\[STABLE\]|Performance Report)', timeout=50).group(1).decode('utf-8')
            if testing_status == 'STABLE':
                stable_log_count += 1
                if stable_log_count > 7:
                    raise Exception('expected Performance Report, but got \'STABLE\'')
                continue
            elif testing_status == 'Performance Report':
                break

        # Match performance report format
        samples_result = dut.expect(r'Samples:\s+(\d+)\s+\(filtered:\s+(\d+)\)', timeout=5)
        average_result = dut.expect(r'Average:\s+(\d+) fps', timeout=1)
        range_result = dut.expect(r'Range:\s+(\d+)\s+-\s+(\d+) fps', timeout=1)
        distrubution_result = dut.expect(r'Distribution:\s+P10=(\d+)\s*\|\s*P25=(\d+)\s*\|\s*P50=(\d+)\s*\|\s*P75=(\d+)\s*\|\s*P90=(\d+)', timeout=1)

        samples_count = samples_result.group(1).decode('utf-8')
        filtered_count = samples_result.group(2).decode('utf-8')
        average_fps = average_result.group(1).decode('utf-8')
        range_fps_min = range_result.group(1).decode('utf-8')
        range_fps_max = range_result.group(2).decode('utf-8')
        p10_fps = distrubution_result.group(1).decode('utf-8')
        p25_fps = distrubution_result.group(2).decode('utf-8')
        p50_fps = distrubution_result.group(3).decode('utf-8')
        p75_fps = distrubution_result.group(4).decode('utf-8')
        p90_fps = distrubution_result.group(5).decode('utf-8')

        print(f'[{test_num}/{resolution_count}] {resolution}: AVG={average_fps} MIN={range_fps_min} P10={p10_fps} P25={p25_fps} P50={p50_fps} P75={p75_fps} P90={p90_fps} MAX={range_fps_max}')

        record_property(
            'fps_benchmark',
            json.dumps({
                'resolution': resolution,
                'samples': int(samples_count),
                'filtered': int(filtered_count),
                'avg': int(average_fps),
                'min': int(range_fps_min),
                'max': int(range_fps_max),
                'p10': int(p10_fps),
                'p25': int(p25_fps),
                'p50': int(p50_fps),
                'p75': int(p75_fps),
                'p90': int(p90_fps),
            })
        )

    dut.expect('Done', timeout=30)
    print(f'All {resolution_count} resolution tests finished!')
    record_property('fps_benchmark_completed', 'true')


@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32c5')
@pytest.mark.target('esp32c6')
@pytest.mark.target('esp32s3')
@pytest.mark.target('esp32p4')
@pytest.mark.env('generic')
@pytest.mark.timeout(3600 * 2) # 2 hours
def test_adapter_fps_benchmark_all(dut: Dut, record_property: Callable[[str, object], None]) -> None:
    test_adapter_fps_benchmark(dut, record_property)


@pytest.mark.target('esp32c2')
@pytest.mark.env('xtal_40mhz')
@pytest.mark.timeout(3600 * 2) # 2 hours
def test_adapter_fps_benchmark_c2_xtal_40mhz(dut: Dut, record_property: Callable[[str, object], None]) -> None:
    test_adapter_fps_benchmark(dut, record_property)


@pytest.mark.target('esp32c2')
@pytest.mark.env('xtal_26mhz')
@pytest.mark.timeout(3600 * 2) # 2 hours
@pytest.mark.parametrize(
    'baud',
    [
        ('74880'),
    ],
)
def test_adapter_fps_benchmark_c2_xtal_26mhz(dut: Dut, record_property: Callable[[str, object], None]) -> None:
    test_adapter_fps_benchmark(dut, record_property)
