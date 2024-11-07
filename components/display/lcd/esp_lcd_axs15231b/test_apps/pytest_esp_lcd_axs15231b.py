# SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s3')
@pytest.mark.env('esp32_s3_lcd_ev_board')
def test_esp_lcd_axs15231b_i80(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[I80]')
    dut.expect_unity_test_output(timeout = 1000)

@pytest.mark.target('esp32s3')
@pytest.mark.env('esp32_s3_lcd_ev_board')
def test_esp_lcd_axs15231b_spi(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[spi]')
    dut.expect_unity_test_output(timeout = 1000)

@pytest.mark.target('esp32s3')
@pytest.mark.env('esp32_s3_lcd_ev_board')
def test_esp_lcd_axs15231b_qspi(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[qspi]')
    dut.expect_unity_test_output(timeout = 1000)
