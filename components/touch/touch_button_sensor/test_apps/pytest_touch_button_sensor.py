# SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/touch/touch_button_sensor/test_apps -t esp32s3
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/touch/touch_button_sensor/test_apps --target esp32s3
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
def test_touch_button_sensor(dut: Dut)-> None:
    dut.run_all_single_board_cases()

@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco_default')
@pytest.mark.parametrize(
    'config',
    [
        'p4rev3',
    ],
)
def test_touch_button_sensor_esp32p4_rev3(dut: Dut)-> None:
    dut.run_all_single_board_cases()

@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_touch_button_sensor_esp32p4_defaults(dut: Dut)-> None:
    dut.run_all_single_board_cases()
