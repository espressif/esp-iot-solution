# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Command-line entry point for the EAF/AAF inspector."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.eaf_inspector.parser import main


if __name__ == '__main__':
    raise SystemExit(main())
