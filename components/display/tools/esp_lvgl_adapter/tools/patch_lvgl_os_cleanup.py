# SPDX-FileCopyrightText: 2026 youkorr
# SPDX-License-Identifier: MIT
"""Release LVGL 9's OS mutex before its memory/global state is discarded."""
import argparse
from pathlib import Path
import re

MARKER = 'esp_lvgl_adapter: release the global OS mutex'
BEFORE = '    lv_mem_deinit();'
AFTER = """#if LV_USE_OS != LV_OS_NONE && defined(ESP_LVGL_ADAPTER_OS_MUTEX_CLEANUP)
    /* esp_lvgl_adapter: release the global OS mutex after all users stop. */
    lv_mutex_delete(&LV_GLOBAL_DEFAULT()->lv_general_mutex);
#endif

    lv_mem_deinit();"""


def patch(path):
    source = path.read_text()
    if MARKER in source:
        return 'already patched'
    if re.search(r'\blv_os_deinit\s*\(', source) or re.search(
            r'lv_mutex_delete\s*\([^;]*lv_general_mutex', source):
        return 'upstream cleanup present'
    if source.count(BEFORE) != 1:
        raise ValueError('Unrecognized LVGL deinit layout; OS cleanup was not changed')
    path.write_text(source.replace(BEFORE, AFTER, 1))
    return 'patched'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('lv_init', type=Path)
    args = parser.parse_args()
    print(patch(args.lv_init))
