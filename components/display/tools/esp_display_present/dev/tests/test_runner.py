# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch
import subprocess

spec = importlib.util.spec_from_file_location('present_run', Path(__file__).parents[1] / 'tools/run.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class RunnerTest(unittest.TestCase):
    def test_cancel_restore_never_identifies_or_accesses_serial(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'hardware-policy.json').write_text(json.dumps({'restore_original': False, 'reason': 'User cancelled restore'}))
            args = SimpleNamespace(work_root=root, board='s3-rgb', suite='benchmark', implementation='present', action='restore', port='/dev/invalid')
            with patch.object(runner, 'identify', side_effect=AssertionError('Must not access serial')), patch.object(runner, 'run', side_effect=AssertionError('Must not run esptool')):
                runner.hardware(args)
            record = json.loads((root / 's3-rgb/backup/restored.json').read_text())
            self.assertTrue(record['skipped_by_user'])
            self.assertFalse(record['verified_full_flash'])

    def test_xip_variants_are_isolated_and_effective(self):
        args = SimpleNamespace(work_root=Path('/tmp/test-xip'), board='s31-rgb', suite='benchmark', implementation='present', xip='on')
        enabled = runner.location(args)
        args.xip = 'off'
        self.assertNotEqual(enabled, runner.location(args))
        clean = runner.location(args)
        args.driver_trace = Path('/tmp/observer')
        self.assertNotEqual(clean, runner.location(args))
        for mode in ('on', 'off'):
            sdk = runner.xip_defaults(mode)
            runner.validate_xip(sdk, mode)
            with self.assertRaises(ValueError):
                runner.validate_xip(sdk, 'off' if mode == 'on' else 'on')
        with self.assertRaises(ValueError):
            runner.validate_xip('CONFIG_SPIRAM_XIP_FROM_PSRAM=y\n', 'on')

    def test_driver_trace_is_data_not_error_handling(self):
        log = ('DRIVER_TRACE mode=DOUBLE_DIRECT rotation=90 requests=4 events=5 target_diffs=2 outside_pool=1 raw=0x1234\n'
               'PASS mode=DOUBLE_DIRECT rotation=90 lvgl_fps=20 scenes=16\nSUITE_PASS board=s31-rgb implementation=present\n')
        result = runner.parse_log(log)
        self.assertFalse(result['errors'])
        self.assertEqual(result['rows'][0]['driver_trace']['target_diffs'], 2)
        self.assertEqual(result['rows'][0]['driver_trace']['outside_pool'], 1)
        self.assertNotIn('raw', result['rows'][0]['driver_trace'])

    def test_board_matrix(self):
        self.assertEqual(len(runner.cases_for('s3-rgb', [0, 90, 180, 270])), 24)
        for board in ('s3-rgb', 's31-rgb', 'p4-dsi'):
            self.assertTrue(all(c['name'] != 'NONE' for c in runner.cases_for(board, [0, 90, 180, 270])))
            with self.assertRaises(ValueError):
                runner.cases_for(board, [0, 90, 180, 270], ['NONE'])
        self.assertEqual(runner.BOARDS['s31-rgb']['width'], 800)
        self.assertEqual(runner.cases_for('s31-rgb', [0, 90, 180, 270]), runner.cases_for('s3-rgb', [0, 90, 180, 270]))
        self.assertEqual(len(runner.cases_for('box3-spi', [0, 90, 180, 270])), 20)
        self.assertEqual(len(runner.cases_for('s31-qspi', [0, 90, 180, 270])), 16)

    def test_adapter_none_rotation_and_auto_not_faked(self):
        a = runner.cases_for('s3-rgb', [0, 90], ['DOUBLE_DIRECT', 'AUTO'])
        self.assertTrue(next(c for c in a if c['name'] == 'DOUBLE_DIRECT' and c['rotation'] == 90)['adapter_supported'])
        self.assertTrue(all(not c['adapter_supported'] for c in a if c['name'] == 'AUTO'))

    def test_explicit_profile_filter(self):
        self.assertEqual(len(runner.cases_for('s31-qspi', [0, 90], ['TE_C2_T2'])), 2)
        with self.assertRaises(ValueError):
            runner.cases_for('box3-spi', [0], ['TE_C2_T2'])

    def test_adapter_native_te_modes_are_not_faked_ct_equivalents(self):
        rows = runner.cases_for('s31-qspi', [0, 90, 180, 270], ['ADAPTER_TE_SYNC_FULL', 'ADAPTER_TE_ASYNC_FULL', 'ADAPTER_TE_ASYNC_BOUNCE'])
        self.assertEqual(len(rows), 12)
        self.assertEqual(sum(c['adapter_supported'] for c in rows), 9)
        self.assertTrue(all(c['compose'] == 0 and c['adapter_te_pipeline'] == 1 and c['adapter_supported'] for c in rows if c['name'] == 'ADAPTER_TE_ASYNC_FULL'))
        self.assertTrue(all(not c['adapter_supported'] for c in rows if c['name'] == 'ADAPTER_TE_SYNC_FULL' and c['rotation']))
        self.assertEqual(len(runner.cases_for('s31-qspi', [0, 90, 180, 270])), 16)

    def test_gate_and_render_metrics_separate(self):
        log = 'RUN mode=TE_C2_T2 rotation=90\nTE_METER: admitted=5 wait_avg_us=1700 tx_avg_us=24000\nPASS mode=TE_C2_T2 rotation=90 frames=5 lvgl_fps=30 scenes=16\nSUITE_PASS board=x\n'
        value = runner.parse_log(log)
        self.assertTrue(value['complete'])
        self.assertEqual(value['rows'][0]['wait_avg_us'], 1700)
        self.assertEqual(value['rows'][0]['tx_avg_us'], 24000)

    def test_profile_metrics_and_output_isolation(self):
        log = 'RUN mode=NONE rotation=0\nPRESENT_PROFILE: label=NONE stage=begin n=5 sum_us=100 avg_us=20 max_us=24 bytes=0 errors=0\nPASS mode=NONE rotation=0 lvgl_fps=50 scenes=16\nSUITE_PASS board=x'
        parsed = runner.parse_log(log)
        self.assertEqual(parsed['rows'][0]['stages']['begin']['avg_us'], 20)
        args = SimpleNamespace(work_root=Path('/tmp/present-test'), board='s3-rgb', suite='benchmark', implementation='present', profile=True)
        self.assertEqual(runner.location(args).name, 'present-profile')

    def test_memory_metrics_keep_phases_heaps_and_rotations_separate(self):
        log = ('MEMORY mode=SUITE rotation=0 phase=after_init internal_total=1000 internal_free=900 psram_total=8000 psram_free=7000 internal_min_free=0 psram_min_free=0\n'
               'MEMORY mode=DOUBLE_FULL rotation=90 phase=after_open internal_total=1000 internal_free=800 psram_total=8000 psram_free=6000 internal_min_free=0 psram_min_free=0\n'
               'MEMORY mode=DOUBLE_FULL rotation=90 phase=after_run internal_total=1000 internal_free=750 psram_total=8000 psram_free=5900 internal_min_free=700 psram_min_free=5000\n'
               'PASS mode=DOUBLE_FULL rotation=90 lvgl_fps=31 scenes=16\n'
               'SKIP mode=NONE rotation=90 reason=unsupported\nSUITE_PASS board=x')
        result = runner.parse_log(log)
        self.assertEqual(result['suite_memory']['after_init']['internal_free'], 900)
        self.assertEqual(result['rows'][0]['memory']['after_open']['psram_free'], 6000)
        self.assertEqual(result['rows'][0]['memory']['after_run']['internal_min_free'], 700)
        self.assertNotIn('memory', result['rows'][1])

    def test_new_memory_schema_rejects_missing_samples(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = SimpleNamespace(work_root=Path(tmp), board='s3-rgb', suite='benchmark', implementation='present')
            root = runner.location(args)
            root.mkdir(parents=True)
            (root / 'manifest.json').write_text(json.dumps(dict(memory_schema=1, cases=runner.cases_for('s3-rgb', [0], ['DOUBLE_FULL']))))
            (root / 'live.log').write_text('PASS mode=DOUBLE_FULL rotation=0 lvgl_fps=20 scenes=16\nSUITE_PASS board=x')
            with self.assertRaises(ValueError):
                runner.report(args)
            result = json.loads((root / 'results.json').read_text())
            self.assertTrue(result['matrix_complete'])
            self.assertFalse(result['memory_complete'])

    def test_rgb_probe_rejects_partial_matrix_without_benchmark_memory(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = SimpleNamespace(work_root=Path(tmp), board='s31-rgb', suite='rgb-events', implementation='present')
            root = runner.location(args)
            root.mkdir(parents=True)
            (root / 'manifest.json').write_text(json.dumps(dict(memory_schema=0)))
            rows = [f'PASS mode=RGB_{kind}_FB{n}_CORE{core} rotation=0 frames=80 idle_frame_events=0 wait_avg_us=1000' for kind in ('BOUNCE', 'DIRECT') for n in (1, 2, 3) for core in (0, 1)]
            (root / 'live.log').write_text('\n'.join(rows[:-1] + ['RGB_EVENT_SUITE_PASS board=s31-rgb']))
            with self.assertRaises(ValueError):
                runner.report(args)
            (root / 'live.log').write_text('\n'.join(rows + ['RGB_EVENT_SUITE_PASS board=s31-rgb']))
            runner.report(args)
            result = json.loads((root / 'results.json').read_text())
            self.assertTrue(result['matrix_complete'])
            self.assertEqual(result['rows'][0]['wait_avg_us'], 1000)
            self.assertNotIn('memory_complete', result)

    def test_rgb_handoff_requires_every_case_and_early_event_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = SimpleNamespace(work_root=Path(tmp), board='s31-rgb', suite='rgb-handoff', implementation='present')
            root = runner.location(args)
            root.mkdir(parents=True)
            (root / 'manifest.json').write_text(json.dumps(dict(memory_schema=0)))
            names = ('DOUBLE_FULL', 'TRIPLE_FULL', 'DOUBLE_DIRECT', 'DOUBLE_PARTIAL', 'TRIPLE_PARTIAL', 'AUTO')
            rows = [f'PASS mode={name}_C{core}_D{delay} rotation={angle} frames=4 driver_events=5 early_events={int(delay != 0)}'
                    for name in names for core in (0, 1) for delay in (0, 50) for angle in (0, 90, 180, 270)]
            marker = 'RGB_HANDOFF_SUITE_PASS board=x cases=96'
            for invalid in (rows[:-1], rows + [rows[0]], [row.replace('early_events=1', 'early_events=0') for row in rows]):
                (root / 'live.log').write_text('\n'.join(invalid + [marker]))
                with self.assertRaises(ValueError):
                    runner.report(args)
            (root / 'live.log').write_text('\n'.join(rows + [marker]))
            runner.report(args)
            result = json.loads((root / 'results.json').read_text())
            self.assertTrue(result['matrix_complete'])
            self.assertFalse(result['errors'])

    def test_error_is_not_pass(self):
        self.assertTrue(runner.parse_log('E (14) worker: error\nSUITE_PASS board=x')['errors'])
        self.assertFalse(runner.parse_log('E (14) gpio: GPIO isr service already installed')['errors'])
        self.assertFalse(runner.parse_log('PASS mode=NONE rotation=0 lvgl_fps=1')['complete'])

    def test_suite_markers(self):
        for marker in ['MATRIX_PASS board=x', 'TE_SUITE_PASS normal_cases=8', 'present_v2 smoke passed']:
            self.assertTrue(runner.parse_log(marker)['complete'])

    def test_wrong_mac_stops_before_write(self):
        args = SimpleNamespace(port='/dev/example', board='s3-rgb')
        with patch.object(runner.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, stdout='MAC: 00:00:00:00:00:00')) as run:
            with self.assertRaises(ValueError):
                runner.identify(args)
            self.assertEqual(run.call_count, 1)
            self.assertIn('read-mac', run.call_args.args[0])

    def test_uart_identity_retains_rom_reconnect_baud(self):
        args = SimpleNamespace(port='/dev/example', board='s3-rgb')
        with patch.object(runner.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, stdout=runner.BOARDS['s3-rgb']['mac'])) as call:
            runner.identify(args)
            command = call.call_args.args[0]
            self.assertEqual(command[command.index('-b') + 1], '115200')
            self.assertNotIn('--no-stub', command)
        args.board = 's31-qspi'
        # S31 uses the supported esptool RAM stub as well: its USB ROM
        # read path can stop before a complete backup has been collected.
        self.assertNotIn('--no-stub', runner.esptool(args, 'default-reset', 'no-reset', 'read-mac'))

    def test_compare_and_configuration_mismatch(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = SimpleNamespace(work_root=Path(tmp), board='s3-rgb')
            base = Path(tmp) / args.board / 'benchmark'
            cases = runner.cases_for('s3-rgb', [90], ['AUTO'])
            manifest = dict(board='s3-rgb', cases=cases, idf_commit='abc', lvgl_benchmark_sha256='def', comparable_sdkconfig_sha256='123', defaults_sha256={}, test_sources_sha256={})
            for impl in ['present', 'adapter']:
                root = base / impl
                root.mkdir(parents=True)
                (root / 'manifest.json').write_text(json.dumps(manifest))
                row = dict(mode='AUTO', rotation=90, status='SKIP' if impl == 'adapter' else 'PASS', lvgl_fps=20)
                (root / 'results.json').write_text(json.dumps(dict(complete=True, matrix_complete=True, errors=[], rows=[row])))
            runner.compare(args)
            self.assertIn(',20,-', (base / 'comparison.csv').read_text())
            for key in ['board', 'idf_commit', 'lvgl_benchmark_sha256', 'comparable_sdkconfig_sha256', 'defaults_sha256', 'test_sources_sha256']:
                different = dict(manifest)
                different[key] = 'different'
                (base / 'adapter/manifest.json').write_text(json.dumps(different))
                with self.assertRaises(ValueError):
                    runner.compare(args)

    def test_comparable_sdkconfig_includes_cache_and_disabled_options(self):
        common = 'CONFIG_LV_COLOR_DEPTH=16\nCONFIG_SPIRAM=y\n'
        baseline = runner.comparable_sdkconfig(common)
        for setting in ('CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB=y',
                        '# CONFIG_SPIRAM_XIP_FROM_PSRAM is not set',
                        'CONFIG_LCD_RGB_ISR_IRAM_SAFE=y'):
            self.assertNotEqual(baseline, runner.comparable_sdkconfig(common + setting + '\n'))
        self.assertEqual(baseline, runner.comparable_sdkconfig(common + 'CONFIG_LOG_DEFAULT_LEVEL=3\n'))
        self.assertEqual(baseline, runner.comparable_sdkconfig('CONFIG_SPIRAM=y\nCONFIG_LV_COLOR_DEPTH=16\n'))

    def test_buffer_audit_rejects_missing_or_wrong_capacity(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = SimpleNamespace(work_root=Path(tmp), board='s31-rgb', suite='benchmark', implementation='adapter')
            root = runner.location(args)
            root.mkdir(parents=True)
            manifest = dict(buffer_schema=1, cases=runner.cases_for('s31-rgb', [90], ['DOUBLE_PARTIAL']),
                            expected_input_bytes={'DOUBLE_PARTIAL/90': 48000})
            (root / 'manifest.json').write_text(json.dumps(manifest))
            tail = 'PASS mode=DOUBLE_PARTIAL rotation=90 lvgl_fps=20 scenes=16\nSUITE_PASS board=x'
            for capacity in [None, 80000, 48000]:
                audit = '' if capacity is None else f'BUFFER_AUDIT: mode=DOUBLE_PARTIAL rotation=90 active_bytes={capacity} min_bytes={capacity} max_bytes={capacity} flushes=20 rendered=4\n'
                (root / 'live.log').write_text(audit + tail)
                if capacity == 48000:
                    runner.report(args)
                    self.assertTrue(json.loads((root / 'results.json').read_text())['buffers_verified'])
                else:
                    with self.assertRaises(ValueError):
                        runner.report(args)

    def test_compare_rejects_different_buffer_capacity_or_memory(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = SimpleNamespace(work_root=Path(tmp), board='s31-rgb')
            base = Path(tmp) / args.board / 'benchmark'
            cases = runner.cases_for('s31-rgb', [90], ['DOUBLE_PARTIAL'])
            manifest = dict(board=args.board, cases=cases, idf_commit='x', lvgl_benchmark_sha256='x',
                            comparable_sdkconfig_sha256='x', defaults_sha256={}, test_sources_sha256={},
                            buffer_schema=1, buffer_policy='logical-50')
            for capacity, internal in [(80000, 1), (48000, 0), (48000, 1)]:
                for impl in ['present', 'adapter']:
                    root = base / impl
                    root.mkdir(parents=True, exist_ok=True)
                    (root / 'manifest.json').write_text(json.dumps(manifest))
                    value = capacity if impl == 'adapter' else 48000
                    memory = internal if impl == 'adapter' else 1
                    audit = dict(active_bytes=value, min_bytes=value, max_bytes=value, internal=memory, psram=1-memory)
                    row = dict(mode='DOUBLE_PARTIAL', rotation=90, status='PASS', lvgl_fps=20, buffer_audit=audit)
                    result = dict(complete=True, matrix_complete=True, buffers_verified=True, errors=[], rows=[row])
                    (root / 'results.json').write_text(json.dumps(result))
                if capacity == 48000 and internal == 1:
                    runner.compare(args)
                else:
                    with self.assertRaises(ValueError):
                        runner.compare(args)

    def test_partial_report_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            args = SimpleNamespace(work_root=Path(tmp), board='s3-rgb', suite='benchmark', implementation='present')
            root = runner.location(args)
            root.mkdir(parents=True)
            (root / 'manifest.json').write_text(json.dumps(dict(cases=runner.cases_for('s3-rgb', [0], ['DOUBLE_FULL']))))
            (root / 'live.log').write_text('SUITE_PASS board=x')
            with self.assertRaises(ValueError):
                runner.report(args)


if __name__ == '__main__':
    unittest.main()
