# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import pytest
import subprocess
import time
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.target('esp32c2')
@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32c6')
@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.target('esp32h2')
@pytest.mark.env('generic')

def test_lightbulb_example(dut: Dut)-> None:
    dut.expect('Lightbulb Example End', timeout = 120)
