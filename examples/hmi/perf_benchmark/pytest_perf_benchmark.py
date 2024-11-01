# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'perf_esp32',
    ],
)
def test_perf_benchmark_esp32(dut: Dut)-> None:
    dut.expect('Returned from app_main', timeout=1000)

@pytest.mark.target('esp32c3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'perf_esp32c3',
    ],
)
def test_perf_benchmark_esp32c3(dut: Dut)-> None:
    dut.expect('Returned from app_main', timeout=1000)

@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'perf_esp32s3',
    ],
)
def test_perf_benchmark_esp32s3(dut: Dut)-> None:
    dut.expect('Returned from app_main', timeout=1000)
