# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
#SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s3')
@pytest.mark.env('led_indicator')
@pytest.mark.timeout(1000)
def test_led_indicator(dut: Dut)-> None:
    dut.run_all_single_board_cases(timeout = 1000)
