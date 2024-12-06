# SPDX-FileCopyrightText: 2024Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/motor/servo/test_apps -t esp32
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/motor/servo/test_apps --target esp32
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.target('esp32s2')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_servo(dut: Dut)-> None:
    dut.run_all_single_board_cases()
