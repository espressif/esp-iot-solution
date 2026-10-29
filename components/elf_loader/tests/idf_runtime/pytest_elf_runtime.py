# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Run the actual C++ ELF constructor/destructor lifecycle on ESP32-S3/P4."""

import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize('config', ['defaults'])
def test_elf_cpp_lifecycle_esp32s3(dut: Dut) -> None:
    dut.run_all_single_board_cases()


@pytest.mark.target('esp32p4')
@pytest.mark.env('generic,eco4')
@pytest.mark.parametrize('config', ['defaults'])
def test_elf_cpp_lifecycle_esp32p4(dut: Dut) -> None:
    dut.run_all_single_board_cases()
