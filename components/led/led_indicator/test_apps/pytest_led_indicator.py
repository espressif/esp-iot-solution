# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
#SPDX-License-Identifier: Apache-2.0

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s3')
@pytest.mark.env('led_indicator')
def test_usb_stream(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('*')
    dut.expect_unity_test_output(timeout = 1000)
