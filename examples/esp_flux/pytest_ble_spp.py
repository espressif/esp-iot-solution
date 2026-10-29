# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
#
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases locally:

1. Setup Environment:
   - . ${IDF_PATH}/export.sh
   - pip install -r tools/requirements/requirement.pytest.txt

2. Build Applications:
   - pip install idf_build_apps
   - python tools/build_apps.py examples/ble_spp/ble_spp_server examples/ble_spp/ble_spp_client -t esp32c2
   # Or build manually:
   # cd examples/ble_spp/ble_spp_server && idf.py build -t esp32c2
   # cd examples/ble_spp/ble_spp_client && idf.py build -t esp32c2

3. Run Tests:
   # Run all tests:
   - pytest examples/ble_spp/pytest_ble_spp.py --target esp32c2

   # Run specific test:
   - pytest examples/ble_spp/pytest_ble_spp.py::test_target_test_ble_spp --target esp32c2

   # Run with verbose output:
   - pytest examples/ble_spp/pytest_ble_spp.py --target esp32c2 -v -s

Note: The test uses @pytest.mark.parametrize to automatically handle app_path and count,
so you don't need to specify --app-path or --count on the command line.
'''

import os.path
import pexpect
import pytest
from typing import Tuple
from pytest_embedded_idf.dut import IdfDut


@pytest.mark.target('esp32c2')
@pytest.mark.env('xtal_26mhz')
@pytest.mark.env('two_duts')
@pytest.mark.parametrize(
    'count, app_path, baud, erase_all',
    [
        (
            2,
            f'{os.path.join(os.path.dirname(__file__), "ble_spp_server")}|{os.path.join(os.path.dirname(__file__), "ble_spp_client")}',
            '74880',
            'y',  # Erase entire flash before flashing
        ),
    ],
    indirect=True,
)
@pytest.mark.timeout(10 * 60)  # 10 minutes timeout
def test_target_test_ble_spp_esp32c2(dut: Tuple[IdfDut, IdfDut]) -> None:
    """
    Test BLE SPP connection stability.
    This test verifies that the connection remains stable without restart after connection is established.
    dut[0] is the server (peripheral), dut[1] is the client (central).
    """
    server = dut[0]
    client = dut[1]

    # Wait for server to receive all fragments
    server.expect('All fragments received', timeout=60)

    # Wait for client to send completion
    client.expect('Fragment send completed', timeout=60)

    # Wait for 100 seconds and check if there's any restart on the server (peripheral)
    # This is similar to esp-idf-v5.5 target_test_ble_spp which checks peripheral stability
    output = server.expect(pexpect.TIMEOUT, timeout=100)
    # Check that there's no restart indication in the output
    assert 'rst:' not in str(output) and 'boot:' not in str(output)

    print('BLE SPP connection/transfer test passed successfully')


@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32c6')
@pytest.mark.target('esp32c61')
@pytest.mark.target('esp32s3')
@pytest.mark.target('esp32h2')
@pytest.mark.env('two_duts')
@pytest.mark.parametrize(
    'count, app_path, erase_all',
    [
        (
            2,
            f'{os.path.join(os.path.dirname(__file__), "ble_spp_server")}|{os.path.join(os.path.dirname(__file__), "ble_spp_client")}',
            'y',  # Erase entire flash before flashing
        ),
    ],
    indirect=True,
)
@pytest.mark.timeout(10 * 60)  # 10 minutes timeout
def test_target_test_ble_spp(dut: Tuple[IdfDut, IdfDut]) -> None:
    """
    Test BLE SPP connection stability.
    This test verifies that the connection remains stable without restart after connection is established.
    dut[0] is the server (peripheral), dut[1] is the client (central).
    """
    server = dut[0]
    client = dut[1]

    # Wait for server to receive all fragments
    server.expect('All fragments received', timeout=60)

    # Wait for client to send completion
    client.expect('Fragment send completed', timeout=60)

    # Wait for 100 seconds and check if there's any restart on the server (peripheral)
    # This is similar to esp-idf-v5.5 target_test_ble_spp which checks peripheral stability
    output = server.expect(pexpect.TIMEOUT, timeout=100)
    # Check that there's no restart indication in the output
    assert 'rst:' not in str(output) and 'boot:' not in str(output)

    print('BLE SPP connection/transfer test passed successfully')
