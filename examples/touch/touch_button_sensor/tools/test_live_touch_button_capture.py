# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Focused parser checks for the public touch-button capture tool."""

import importlib.util
from pathlib import Path
import unittest


SCRIPT = Path(__file__).with_name('live_touch_button_capture.py')
SPEC = importlib.util.spec_from_file_location('live_touch_button_capture', SCRIPT)
PLOT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PLOT)


class TouchButtonCaptureTests(unittest.TestCase):
    def test_public_amplitude(self):
        row = PLOT.parse_line(
            'I (1200) touch_button_example: channel=6 amplitude=12'
        )
        self.assertEqual(row['kind'], 'amplitude')
        self.assertEqual(row['ch'], 6)
        self.assertEqual(row['amplitude'], 12)
        self.assertNotIn('noise_std', row)

    def test_public_event(self):
        row = PLOT.parse_line(
            'W (1300) touch_button_example: channel=7 '
            'state=ACTIVE amplitude=650'
        )
        self.assertEqual(row['kind'], 'event')
        self.assertEqual(row['state'], 1)

    def test_private_fsm_record_is_not_consumed(self):
        self.assertIsNone(PLOT.parse_line(
            'vl,1300,6,0,5000,5000,0,0,0,500,250,0,0,0,0,0'
        ))

    def test_stats_without_amplitude_is_ignored(self):
        self.assertIsNone(PLOT.parse_line(
            'I (1400) touch_button_example: STATS channel=6 '
            'state=INACTIVE presses=0 releases=0'
        ))

    def test_concatenated_public_records(self):
        rows = PLOT.parse_records(
            'I (1500) touch_button_example: channel=6 amplitude=10 '
            'I (1510) touch_button_example: channel=7 amplitude=20'
        )
        self.assertEqual([row['ch'] for row in rows], [6, 7])


if __name__ == '__main__':
    unittest.main()
