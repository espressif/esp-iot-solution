# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py examples/usb/device/usb_uf2_nvs -t esp32s2
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest examples/usb/device/usb_uf2_nvs --target esp32s2
'''

import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.env('usb-otg_camera')
@pytest.mark.parametrize(
    'config',
    [
        'esp32s3_usb_otg',
    ],
)
def test_usb_uf2_nvs(dut: Dut)-> None:
    dut.expect(r'TUF2: UF2 Updater install succeed, Version: (\d+).(\d+).(\d+)', timeout=5)
    dut.expect(r'TUF2: USB Mounted', timeout=10)
