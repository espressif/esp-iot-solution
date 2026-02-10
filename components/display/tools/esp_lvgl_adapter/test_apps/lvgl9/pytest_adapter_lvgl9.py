# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""
ESP LVGL Adapter Feature Test Runner

This test suite validates core adapter features on real hardware with physical LCD panels.
Tests include: basic functionality, image decoding, FreeType fonts, and dummy draw mode.
"""

import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32c3')
@pytest.mark.env('esp32c3_lcdkit')
def test_adapter_esp32c3(dut: Dut) -> None:
    """
    Feature test for ESP32-C3 - basic adapter functionality.
    Tests adapter initialization, display registration, and basic operations.
    """
    dut.expect('ESP LVGL Adapter Feature Test Suite', timeout=30)
    dut.expect_exact('Press ENTER to see the list of tests.', timeout=5)
    dut.write('[adapter]')

    # Test 1: Basic adapter functionality
    dut.expect('Running basic adapter test', timeout=10)
    print('[1/2] Testing basic adapter demo...')
    dut.expect('Basic adapter test completed', timeout=10)
    print('[1/2] Basic adapter demo: PASSED')

    # Test 2: Dummy draw functionality
    dut.expect('Running dummy draw test', timeout=10)
    print('[2/2] Testing dummy draw mode...')
    dut.expect('Dummy draw test completed', timeout=10)
    print('[2/2] Dummy draw mode: PASSED')

    print('ESP32-C3 adapter tests finished!')


@pytest.mark.target('esp32s3')
@pytest.mark.env('esp-box-3')
def test_adapter_esp32s3(dut: Dut) -> None:
    """
    Feature test for ESP32-S3 - full feature set.
    Tests adapter, image decoder, FreeType fonts, and dummy draw.
    """
    dut.expect('ESP LVGL Adapter Feature Test Suite', timeout=30)
    dut.expect_exact('Press ENTER to see the list of tests.', timeout=10)
    dut.write('[adapter]')

    # Test 1: Basic adapter functionality
    dut.expect('Running basic adapter test', timeout=15)
    print('[1/4] Testing basic adapter demo...')
    dut.expect('Basic adapter test completed', timeout=10)
    print('[1/4] Basic adapter demo: PASSED')

    # Test 2: Image decoder
    dut.expect('Running image decoder test', timeout=10)
    print('[2/4] Testing image decoder...')
    dut.expect(r'Loading image: I:red_png\.png', timeout=5)
    dut.expect(r'Loading image: I:red_jpg\.jpg', timeout=5)
    dut.expect('Image decoder test completed', timeout=10)
    print('[2/4] Image decoder: PASSED')

    # Test 3: Dummy draw
    dut.expect('Running dummy draw test', timeout=10)
    print('[3/4] Testing dummy draw mode...')
    dut.expect('Drawing Red screen', timeout=5)
    dut.expect('Drawing Green screen', timeout=5)
    dut.expect('Drawing Blue screen', timeout=5)
    dut.expect('Dummy draw test completed', timeout=10)
    print('[3/4] Dummy draw mode: PASSED')

    # Test 4: FreeType fonts
    dut.expect('Running FreeType font test', timeout=10)
    print('[4/4] Testing FreeType fonts...')
    dut.expect('Initializing 30px font', timeout=5)
    dut.expect('Initializing 48px font', timeout=5)
    dut.expect('Cleaning up fonts', timeout=5)
    dut.expect('FreeType font test completed', timeout=10)
    print('[4/4] FreeType fonts: PASSED')

    print('ESP32-S3 adapter tests finished!')


def test_adapter_esp32p4_template(dut: Dut) -> None:
    """
    Feature test for ESP32-P4 - full feature set with hardware acceleration.
    Tests adapter, image decoder, FreeType fonts, and dummy draw.
    """
    dut.expect('ESP LVGL Adapter Feature Test Suite', timeout=30)
    dut.expect_exact('Press ENTER to see the list of tests.', timeout=5)
    dut.write('[adapter]')

    # Test 1: Basic adapter functionality
    dut.expect('Running basic adapter test', timeout=10)
    print('[1/4] Testing basic adapter demo...')
    dut.expect('Basic adapter test completed', timeout=10)
    print('[1/4] Basic adapter demo: PASSED')

    # Test 2: Image decoder
    dut.expect('Running image decoder test', timeout=10)
    print('[2/4] Testing image decoder...')
    dut.expect(r'Loading image: I:red_png\.png', timeout=5)
    dut.expect(r'Loading image: I:red_jpg\.jpg', timeout=5)
    dut.expect('Image decoder test completed', timeout=10)
    print('[2/4] Image decoder: PASSED')

    # Test 3: Dummy draw
    dut.expect('Running dummy draw test', timeout=10)
    print('[3/4] Testing dummy draw mode...')
    dut.expect('Drawing Red screen', timeout=5)
    dut.expect('Drawing Green screen', timeout=5)
    dut.expect('Drawing Blue screen', timeout=5)
    dut.expect('Dummy draw test completed', timeout=10)
    print('[3/4] Dummy draw mode: PASSED')

    # Test 4: FreeType fonts
    dut.expect('Running FreeType font test', timeout=10)
    print('[4/4] Testing FreeType fonts...')
    dut.expect('Initializing 30px font', timeout=5)
    dut.expect('Initializing 48px font', timeout=5)
    dut.expect('Cleaning up fonts', timeout=5)
    dut.expect('FreeType font test completed', timeout=10)
    print('[4/4] FreeType fonts: PASSED')

    print('ESP32-P4 adapter tests finished!')

@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32p4_function_ev_board,screen,touch,eco4')
@pytest.mark.parametrize(
    'config',
    [
        'defaults',
    ],
)
def test_adapter_esp32p4_eco4(dut: Dut) -> None:
    test_adapter_esp32p4_template(dut)

@pytest.mark.target('esp32p4')
@pytest.mark.env('esp32p4_function_ev_board,screen,touch,eco_default')
@pytest.mark.parametrize(
    'config',
    [
        'p4rev3',
    ],
)
def test_adapter_esp32p4_eco_default(dut: Dut) -> None:
    test_adapter_esp32p4_template(dut)
