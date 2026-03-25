# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - cd components/esp_xiaozhi/test_apps && idf.py build
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/esp_xiaozhi/test_apps --target esp32s3 --port /dev/ttyACM0
'''

import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.timeout(300)
def test_esp_xiaozhi(dut: Dut) -> None:
    dut.run_all_single_board_cases(timeout=300)
