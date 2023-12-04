# SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py examples/usb/device/usb_uf2_ota -t esp32s3
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest examples/usb/device/usb_uf2_ota --target esp32s3
'''

import pytest
import subprocess
import time
from pytest_embedded import Dut

def check_usb_device(product_name):
    try:
        # Try running the lsusb command
        output = subprocess.check_output(['lsusb']).decode('utf-8')
        print(output)
        lines = output.split('\n')
        for line in lines:
            if product_name in line:
                return True
        return False
    except FileNotFoundError:
        try:
            # Install usbutils package
            subprocess.run(['sudo', 'apt-get', 'update'])
            subprocess.run(['sudo', 'apt-get', 'install', '-y', 'usbutils'])

            # Retry running the lsusb command
            output = subprocess.check_output(['lsusb']).decode('utf-8')
            print(output)
            lines = output.split('\n')
            for line in lines:
                if product_name in line:
                    return True
            return False
        except subprocess.CalledProcessError as e:
            print('Error executing lsusb:', e)
            return False

@pytest.mark.target('esp32s2')
@pytest.mark.target('esp32s3')
@pytest.mark.env('usb-otg_camera')
def test_usb_uf2_ota(dut: Dut)-> None:
    usb_name = 'Espressif ESP TinyUF2'
    dut.expect(r'TUF2: Enable USB console, log will be output to USB', timeout=5)
    time.sleep(3)
    assert check_usb_device(usb_name), f"USB disk with name '{usb_name}' not found on the test machine"
