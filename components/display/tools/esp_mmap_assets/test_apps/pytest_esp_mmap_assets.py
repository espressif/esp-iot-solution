# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/display/tools/esp_mmap_assets/test_apps -t esp32s3
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/display/tools/esp_mmap_assets/test_apps --target esp32s3
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s3')
@pytest.mark.env('esp32_s3_lcd_ev_board')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_esp_mmap_assets(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[assets]')
    dut.expect_unity_test_output(timeout = 10000)
