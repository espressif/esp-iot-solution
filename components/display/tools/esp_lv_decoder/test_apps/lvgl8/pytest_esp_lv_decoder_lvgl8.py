# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_esp_lv_decoder_s3(dut: Dut)-> None:
    dut.run_all_single_board_cases()

@pytest.mark.target('esp32c3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_esp_lv_decoder_c3(dut: Dut)-> None:
    dut.run_all_single_board_cases()

@pytest.mark.target('esp32p4')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_esp_lv_decoder_p4(dut: Dut)-> None:
    dut.run_all_single_board_cases()
