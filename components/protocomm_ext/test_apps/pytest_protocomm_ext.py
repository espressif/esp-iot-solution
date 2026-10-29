# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/protocomm_ext/test_apps -t esp32
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/protocomm_ext/test_apps --target esp32
"""

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.env('protocomm_ext')
@pytest.mark.parametrize('config', ['defaults'])
def test_protocomm_ext(dut: Dut) -> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('*')
    dut.expect_unity_test_output(timeout=300)
