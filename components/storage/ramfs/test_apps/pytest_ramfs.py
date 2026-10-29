# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0
import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32')
@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32c6')
@pytest.mark.env('generic')
@pytest.mark.timeout(60 * 60)
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
    indirect=True,
)
def test_ramfs(dut: Dut) -> None:
    dut.run_all_single_board_cases(group='ramfs')


@pytest.mark.target('esp32c2')
@pytest.mark.env('xtal_26mhz')
@pytest.mark.timeout(60 * 60)
@pytest.mark.parametrize(
    'config, baud',
    [
        ('defaults', '74880'),
    ],
    indirect=True,
)
def test_ramfs_esp32c2(dut: Dut) -> None:
    dut.run_all_single_board_cases(group='ramfs')

# Test on ESP32-P4 ECO4
@pytest.mark.target('esp32p4')
@pytest.mark.env('rev_default')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
@pytest.mark.timeout(60 * 60)
def test_ramfs_p4_eco4(dut: Dut)-> None:
    dut.run_all_single_board_cases(group='ramfs')
