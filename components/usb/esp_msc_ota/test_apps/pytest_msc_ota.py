# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/usb/esp_msc_ota/test_apps -t esp32s3
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/usb/esp_msc_ota/test_apps --target esp32s3
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s3')
@pytest.mark.env('msc-ota')
@pytest.mark.timeout(120)
def test_usb_stream(dut: Dut)-> None:
    dut.run_all_single_board_cases(group='Auto')
