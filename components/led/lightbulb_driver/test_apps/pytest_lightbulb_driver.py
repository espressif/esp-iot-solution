# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
#SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.target('esp32c2')
@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32c6')
@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.target('esp32h2')
@pytest.mark.env('generic')

def test_lightbulb_driver(dut: Dut)-> None:
    dut.run_all_single_board_cases()
