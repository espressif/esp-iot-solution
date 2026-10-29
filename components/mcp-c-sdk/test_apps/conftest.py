# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Local pytest helpers for mcp-c-sdk test_apps.

This suite relies on pytest-embedded's auto-flash path. Some pytest-embedded
releases still pass hyphenated esptool verbs such as ``write-flash`` while the
esptool package available in our ESP-IDF environment only accepts the
underscore form ``write_flash``. Patch the local process once so the whole
matrix remains runnable without modifying site-packages.
"""

from __future__ import annotations

from typing import Iterable, List

import esptool


_ESPTTOOL_VERB_MAP = {
    'chip-id': 'chip_id',
    'dump-mem': 'dump_mem',
    'erase-flash': 'erase_flash',
    'erase-region': 'erase_region',
    'flash-id': 'flash_id',
    'get-security-info': 'get_security_info',
    'image-info': 'image_info',
    'load-ram': 'load_ram',
    'make-image': 'make_image',
    'merge-bin': 'merge_bin',
    'read-flash': 'read_flash',
    'read-flash-sfdp': 'read_flash_sfdp',
    'read-flash-status': 'read_flash_status',
    'read-mac': 'read_mac',
    'read-mem': 'read_mem',
    'verify-flash': 'verify_flash',
    'write-flash': 'write_flash',
    'write-flash-status': 'write_flash_status',
    'write-mem': 'write_mem',
}


def _normalize_esptool_argv(argv: Iterable[str]) -> List[str]:
    return [_ESPTTOOL_VERB_MAP.get(arg, arg) for arg in argv]


if not getattr(esptool.main, '_mcp_hyphen_compat', False):
    _orig_esptool_main = esptool.main

    def _compat_esptool_main(argv, *args, **kwargs):
        return _orig_esptool_main(_normalize_esptool_argv(argv), *args, **kwargs)

    _compat_esptool_main._mcp_hyphen_compat = True  # type: ignore[attr-defined]
    esptool.main = _compat_esptool_main
