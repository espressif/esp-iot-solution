# SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/sensors/gas/bme690/test_apps -t esp32c5
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/sensors/gas/bme690/test_apps --target esp32c5
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32c5')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_bme690(dut: Dut)-> None:
    dut.run_all_single_board_cases()
