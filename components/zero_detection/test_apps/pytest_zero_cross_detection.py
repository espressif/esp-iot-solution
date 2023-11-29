# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/zero_detection/test_apps -t esp32s3
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/zero_detection/test_apps --target esp32s3
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s3')
@pytest.mark.target('esp32c3')
@pytest.mark.env('zero_detection')
def test_zero_detection(dut: Dut)-> None:
    dut.run_all_single_board_cases(group='zero_cross_detecion')
