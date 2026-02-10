# SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32p4')
@pytest.mark.env('generic')
def test_esp_lcd_er88577(dut: Dut)-> None:
    dut.run_all_single_board_cases()
