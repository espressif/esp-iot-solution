# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize('config', ['defaults'], indirect=True)
def test_esp_lcd_touch_mux(dut: Dut) -> None:
    dut.run_all_single_board_cases(group='touch_mux')
