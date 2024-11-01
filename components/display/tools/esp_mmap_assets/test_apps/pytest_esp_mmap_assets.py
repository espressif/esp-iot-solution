# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32s3')
@pytest.mark.target('esp32p4')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_esp_mmap_assets(dut: Dut)-> None:
    dut.run_all_single_board_cases()
