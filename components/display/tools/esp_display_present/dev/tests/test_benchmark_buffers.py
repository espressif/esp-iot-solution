# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('buffers', Path(__file__).parents[1] / 'tools/benchmark_buffers.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


class BufferPolicyTest(unittest.TestCase):
    def test_rectangular_partial_matches_exactly_at_every_angle(self):
        for w, h in [(800, 480), (1024, 600), (480, 480)]:
            for angle in [0, 90, 180, 270]:
                c = {'mode': 'B_DOUBLE_PARTIAL', 'rotation': angle}
                board = {'width': w, 'height': h}
                expected = (h if angle in (90, 270) else w) * 50 * 2
                for impl in ['present', 'adapter']:
                    self.assertEqual(b.expected_input_bytes(board, c, impl), expected)
                if w != h and angle in (90, 270):
                    self.assertNotEqual(expected, b.expected_input_bytes(board, c, 'adapter', 'legacy-physical-50'))

    def test_full_frames_do_not_change_capacity_with_rotation(self):
        for mode in ['B_DOUBLE_FULL', 'B_TRIPLE_FULL']:
            for angle in [0, 90, 180, 270]:
                for impl in ['present', 'adapter']:
                    self.assertEqual(b.expected_input_bytes({'width': 1024, 'height': 600}, {'mode': mode, 'rotation': angle}, impl), 1228800)

    def test_native_te_layout_is_not_forced_equivalent(self):
        self.assertIsNone(b.expected_input_bytes({'width': 480, 'height': 480}, {'mode': 'B_TE', 'rotation': 90}, 'adapter'))

    def test_shim_changes_only_the_allocation_expression_and_fails_closed(self):
        source = 'unchanged fence before\n' + b.ANCHOR + '\nunchanged callback after\n'
        output = b.align_adapter_allocation(source)
        self.assertEqual(output, source.replace(b.ANCHOR, b.ALIGNED))
        for invalid in ['', source + b.ANCHOR]:
            with self.assertRaises(ValueError):
                b.align_adapter_allocation(invalid)


if __name__ == '__main__':
    unittest.main()
