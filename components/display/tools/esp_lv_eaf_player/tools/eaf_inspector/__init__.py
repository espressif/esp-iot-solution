# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""EAF/AAF parser and validator matching the device-side decoder."""

from .parser import EafFile, inspect_file, parse_file

__all__ = ['EafFile', 'inspect_file', 'parse_file']
