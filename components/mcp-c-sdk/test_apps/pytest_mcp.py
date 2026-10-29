# SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

'''
Steps to run these cases:
- Build
  - . ${IDF_PATH}/export.sh
  - pip install idf_build_apps
  - python tools/build_apps.py components/mcp-c-sdk/test_apps -t esp32
- Test
  - pip install -r tools/requirements/requirement.pytest.txt
  - pytest components/mcp-c-sdk/test_apps --target esp32
'''

import os
import re
import pexpect
import pytest
from pytest_embedded import Dut

UNITY_READY_LINE = 'Press ENTER to see the list of tests'
UNITY_MENU_HEADER = "Here's the test menu, pick your combo:"
UNITY_MENU_FOOTER = 'Enter test for running.'


def _extract_unity_menu_text(raw_menu: str) -> str:
    case_regex = re.compile(r'\((\d+)\)\s\"(.+)\"\s(\[.+\])+')
    subcase_regex = re.compile(r'\t\((\d+)\)\s\"(.+)\"')

    menu_blocks = []
    current_block = []
    seen_lines = set()

    for raw_line in raw_menu.splitlines():
        line = raw_line.rstrip('\r')
        stripped = line.strip()

        if not stripped:
            continue

        if UNITY_MENU_HEADER in stripped:
            if current_block:
                menu_blocks.append(current_block)
            current_block = []
            seen_lines = set()
            continue

        if UNITY_MENU_FOOTER in stripped:
            if current_block:
                menu_blocks.append(current_block)
            current_block = []
            seen_lines = set()
            continue

        if case_regex.match(line) or subcase_regex.match(line):
            if line not in seen_lines:
                seen_lines.add(line)
                current_block.append(line)

    if current_block:
        menu_blocks.append(current_block)

    if not menu_blocks:
        raise NotImplementedError('No parseable unity menu block found')

    return '\n'.join(max(menu_blocks, key=len))


def _install_unity_menu_parser_workaround(dut: Dut) -> None:
    """
    Work around flaky Unity menu captures where menu headers can appear twice
    in one captured block and break pytest-embedded's strict parser.
    """
    original_parser = dut._parse_unity_menu_from_str

    def _patched_parser(raw_menu: str):
        try:
            return original_parser(raw_menu)
        except NotImplementedError:
            return original_parser(_extract_unity_menu_text(raw_menu))

    # Override only on current dut instance.
    dut._parse_unity_menu_from_str = _patched_parser


def _capture_unity_menu(dut: Dut, idle_timeout: float = 1.5) -> None:
    try:
        dut.expect_exact(UNITY_MENU_HEADER, timeout=0.5)
    except Exception:
        # Some boards need an explicit ENTER to emit the menu.
        dut.write('\n')
        dut.expect_exact(UNITY_MENU_HEADER, timeout=5)

    raw_menu = dut.expect(pexpect.TIMEOUT, timeout=idle_timeout, return_what_before_match=True)
    dut._test_menu = dut._parse_unity_menu_from_str(raw_menu.decode('utf8', errors='ignore'))


def _prepare_unity_menu(dut: Dut) -> None:
    _install_unity_menu_parser_workaround(dut)

    # Parse menu once and cache it for the full pytest session.
    if getattr(dut, '_test_menu', None) is None:
        dut.expect_exact(UNITY_READY_LINE, timeout=10)
        _capture_unity_menu(dut)
        # Keep behavior aligned with pytest-embedded's `test_menu` property:
        # after menu parse, hard reset to enter a fresh run state.
        dut._hard_reset()


@pytest.mark.target('esp32c3')
@pytest.mark.target('esp32s3')
@pytest.mark.env('generic')
@pytest.mark.timeout(1000)
@pytest.mark.parametrize(
    'config',
    [
        'api_test',
        'default',
        'toolcall_min',
        'toolcall_max',
        'toollist_min',
        'toollist_max',
        'no_http_server',
        'no_http_client',
        'sse_off',
        'auth_on',
        'auth_scope_on',
        'auth_jwt_claims',
        'auth_jwt_sig',
        'oauth_meta_off',
        'session_ttl_min',
        'protocol_default_2024',
    ],
)
def test_mcp(dut: Dut)-> None:
    _prepare_unity_menu(dut)

    unity_filter = os.getenv('UNITY_TEST_FILTER', '*')
    unity_filter = unity_filter.strip()

    if not unity_filter or unity_filter == '*':
        dut.run_all_single_board_cases(timeout=1000)
        return

    # Support legacy unity menu-style filters like:
    # - [gate]
    # - [gate][session]   (AND semantics)
    if '[' in unity_filter and ']' in unity_filter:
        group_expr = unity_filter.replace('][', '&').replace('[', '').replace(']', '').strip()
        dut.run_all_single_board_cases(group=group_expr, timeout=1000)
        return

    # Support case-name filters, with '|' as OR separator.
    names = [item.strip() for item in unity_filter.split('|') if item.strip()]
    if len(names) == 1:
        dut.run_all_single_board_cases(name=names[0], timeout=1000)
    else:
        dut.run_all_single_board_cases(name=names, timeout=1000)
