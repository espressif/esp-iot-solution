# SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32p4')
@pytest.mark.env('generic')
def test_esp_lcd_st7121(dut: Dut)-> None:
    dut.run_all_single_board_cases()
