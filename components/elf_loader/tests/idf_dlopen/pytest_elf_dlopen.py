# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Verify #776 dlopen/dlsym and real S3 PSRAM executable aliases."""

import pytest
from pytest_embedded import Dut


@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.parametrize('config', ['defaults'])
def test_elf_dlopen_internal_iram(dut: Dut) -> None:
    dut.run_all_single_board_cases()


@pytest.mark.target('esp32s3')
@pytest.mark.env('esp-box-3')
@pytest.mark.parametrize('config', ['psram'])
def test_elf_dlopen_psram(dut: Dut) -> None:
    # ESP-BOX-3 has a write-restricted USB console; the firmware runs Unity
    # automatically in PSRAM mode and announces the checked result.
    dut.expect('ELF_DLOPEN_PSRAM=PASS', timeout=90)
