# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Benchmark-only allocation policy; never patch the checkout or display fences."""
POLICIES = ('logical-50', 'legacy-physical-50')
ANCHOR = 'cfg->draw_buf_pixels = (size_t)profile->hor_res * profile->buffer_height;'
ALIGNED = '''/* Benchmark allocation alignment only: same logical-width tile as present. */
        size_t input_width = (profile->rotation == ESP_LV_ADAPTER_ROTATE_90 ||
                              profile->rotation == ESP_LV_ADAPTER_ROTATE_270) ? profile->ver_res : profile->hor_res;
        cfg->draw_buf_pixels = input_width * profile->buffer_height;'''


def align_adapter_allocation(source):
    """Only change the PARTIAL allocation expression in an isolated build overlay."""
    if source.count(ANCHOR) != 1:
        raise ValueError('Adapter PARTIAL allocation changed; review the benchmark shim')
    return source.replace(ANCHOR, ALIGNED)


def expected_input_bytes(board, case, implementation, policy='logical-50'):
    """None means the native GRAM/TE input layout is not forced equivalent."""
    width, height = board['width'], board['height']
    if case['mode'] in ('B_DOUBLE_FULL', 'B_TRIPLE_FULL'):
        return width * height * 2
    if case['mode'] in ('B_DOUBLE_PARTIAL', 'B_TRIPLE_PARTIAL'):
        if case['rotation'] in (90, 270) and (implementation == 'present' or policy == 'logical-50'):
            width = height
        return width * 50 * 2
    return None
