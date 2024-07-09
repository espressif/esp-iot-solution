# SPDX-FileCopyrightText: 2024Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/bluetooth/ble_hci/test_apps -t esp32
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/bluetooth/ble_hci/test_apps --target esp32
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32')
@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32c6')
@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_ble_hci(dut: Dut)-> None:
    dut.run_all_single_board_cases()

@pytest.mark.target('esp32c2')
@pytest.mark.env('xtal_26mhz')
@pytest.mark.parametrize(
    'config , baud',
    [
        ('c2_xtal_26mhz', '74880'),
    ],
)
def test_ble_hci_esp32c2(dut: Dut)-> None:
    dut.run_all_single_board_cases()
