# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
import subprocess
import time
from pytest_embedded import Dut

@pytest.mark.target('esp32c2')
@pytest.mark.env('xtal_26mhz')
@pytest.mark.parametrize(
    'baud',
    [
        ('74880'),
    ],
)
def test_relinker_example_c2(dut: Dut)-> None:
    dut.expect('Relinker Example End', timeout = 120)

@pytest.mark.target('esp32c3')
@pytest.mark.env('generic')
def test_relinker_example_c3(dut: Dut)-> None:
    dut.expect('Relinker Example End', timeout = 120)
