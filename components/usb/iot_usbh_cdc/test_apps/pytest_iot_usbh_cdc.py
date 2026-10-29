# SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/usb/iot_usbh_cdc/test_apps -t esp32s3
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/usb/iot_usbh_cdc/test_apps --target esp32s3
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.timeout(60 * 60)
def test_iot_usbh_cdc(dut: Dut)-> None:
    dut.run_all_single_board_cases(group='auto')

# Test on ESP32-P4 ECO4
@pytest.mark.target('esp32p4')
@pytest.mark.env('usb_4g_ec801e_modem')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
@pytest.mark.timeout(60 * 60)
def test_iot_usbh_cdc_p4_eco4(dut: Dut)-> None:
    dut.run_all_single_board_cases(group='auto')

# Test on ESP32-P4 ECO5
# @pytest.mark.target('esp32p4')
# @pytest.mark.env('usb_4g_ec801e_modem')
# @pytest.mark.parametrize(
#     'config',
#     [
#         'p4rev3',
#     ],
# )
# @pytest.mark.timeout(60 * 60)
# def test_iot_usbh_cdc_p4_eco_default(dut: Dut)-> None:
#     dut.run_all_single_board_cases(group='auto')
