# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/usb/usb_stream/test_apps -t esp32s2
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/usb/usb_stream/test_apps --target esp32s2
'''

import pytest
from pytest_embedded import Dut

@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.env('usb_camera')
@pytest.mark.timeout(60 * 60)
@pytest.mark.parametrize(
    'config',
    [
        # Known to cause. assert failed: (rem_len == 0 || is_in)
        # '160mhz',
        '240mhz',
    ],
)
def test_usb_stream(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[devkit]')
    dut.expect_unity_test_output(timeout = 1000)

@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.env('usb-otg_camera')
@pytest.mark.timeout(60 * 60)
@pytest.mark.parametrize(
    'config',
    [
        # Known to cause. assert failed: (rem_len == 0 || is_in)
        # '160mhz',
        '240mhz',
    ],
)
def test_usb_stream_otg(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[otg]')
    dut.expect_unity_test_output(timeout = 3000)

@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.env('usb_camera_isoc')
@pytest.mark.timeout(60 * 60)
@pytest.mark.parametrize(
    'config',
    [
        # Known to cause. assert failed: (rem_len == 0 || is_in)
        # '160mhz',
        'quick',
    ],
)
def test_quick_start(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[quick]')
    dut.expect_unity_test_output(timeout = 3000)

@pytest.mark.target('esp32s3')
@pytest.mark.env('usb_camera_s3_eye')
@pytest.mark.timeout(60 * 60)
@pytest.mark.parametrize(
    'config',
    [
        # Known to cause. assert failed: (rem_len == 0 || is_in)
        # '160mhz',
        '240mhz',
    ],
)
def test_esp32s3_eye_camera_start(dut: Dut)-> None:
    dut.expect_exact('Press ENTER to see the list of tests.')
    dut.write('[uvc_only]')
    dut.expect_unity_test_output(timeout = 3000)
