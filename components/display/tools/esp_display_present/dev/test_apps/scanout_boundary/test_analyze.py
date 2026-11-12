# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
import unittest
import tempfile
from pathlib import Path
from prepare import patch
import analyze as a


def trace(buffers=2, bounce=False):
    case = {'name': 'synthetic', 'scope': 'v2', 'dropped': 0, 'records': [], 'ended': True}
    def emit(kind, buffer=0, detail=0):
        sequence = len(case['records'])
        case['records'].append((sequence, sequence * 10, kind, buffer, detail, 0))
    for index in range(buffers):
        emit(a.REGISTER, index + 100, index)
    active = 100
    emit(a.SELECT, active)
    emit(a.READ_BEGIN, active)
    if bounce:
        emit(a.READ_END, active)
    for frame in range(6):
        target = 100 + (frame + 1) % buffers
        emit(a.WRITE_BEGIN, target)
        emit(a.WRITE_END, target)
        emit(a.SUBMIT, target, frame + 1)
        emit(a.ARM, target, frame + 1)
        if not bounce:
            emit(a.READ_END, active)
        emit(a.SELECT, target)
        emit(a.READ_BEGIN, target)
        if bounce:
            emit(a.READ_END, target)
            emit(a.BOUNCE_EOF)
        emit(a.FRAME_DONE, target)
        emit(a.VSYNC)
        emit(a.CALLBACK, detail=1)
        if not bounce:
            emit(a.READ_END, target)
            emit(a.READ_BEGIN, target)
        emit(a.FRAME_DONE, target)
        emit(a.VSYNC)
        emit(a.CALLBACK, detail=1)
        emit(a.CONFIRM, detail=frame + 1)
        emit(a.RELEASE, active)
        active = target
    emit(a.STOP)
    case['count'] = len(case['records'])
    return case


class AnalyzeTest(unittest.TestCase):
    def test_valid_matrix(self):
        for count in (2, 3):
            for bounce in (False, True):
                with self.subTest(count=count, bounce=bounce):
                    self.assertEqual(a.analyze_case(trace(count, bounce))['result'], 'TRACE_PASS')

    def test_release_active(self):
        case = trace()
        records = case['records']
        index = next(i for i, r in enumerate(records) if r[2] == a.RELEASE)
        record = list(records[index]); record[3] = 101; records[index] = tuple(record)
        self.assertEqual(a.analyze_case(case)['result'], 'FAIL')

    def test_write_during_read(self):
        case = trace()
        index = next(i for i, r in enumerate(case['records']) if r[2] == a.WRITE_BEGIN)
        record = list(case['records'][index]); record[3] = 100; case['records'][index] = tuple(record)
        self.assertEqual(a.analyze_case(case)['result'], 'FAIL')

    def test_early_confirmation(self):
        case = trace()
        index = next(i for i, r in enumerate(case['records']) if r[2] == a.CALLBACK)
        record = list(case['records'][index]); record[4] = 0; case['records'][index] = tuple(record)
        self.assertEqual(a.analyze_case(case)['result'], 'FAIL')

    def test_wrong_cookie(self):
        case = trace()
        index = next(i for i, r in enumerate(case['records']) if r[2] == a.CONFIRM)
        record = list(case['records'][index]); record[4] = 999; case['records'][index] = tuple(record)
        self.assertEqual(a.analyze_case(case)['result'], 'FAIL')

    def test_missing_evidence(self):
        for field, value in (('dropped', 1), ('ended', False), ('count', 999)):
            case = trace(); case[field] = value
            self.assertEqual(a.analyze_case(case)['result'], 'INCOMPLETE')

    def test_underrun(self):
        case = trace()
        record = list(case['records'][-1]); record[2] = a.UNDERRUN; case['records'][-1] = tuple(record)
        self.assertEqual(a.analyze_case(case)['result'], 'FAIL')

    def test_nonmonotonic(self):
        case = trace()
        record = list(case['records'][3]); record[0] = 999; case['records'][3] = tuple(record)
        self.assertEqual(a.analyze_case(case)['result'], 'INCOMPLETE')

    def test_parse_truncation_and_completion(self):
        case = trace()
        text = f"BOUNDARY_BEGIN,synthetic,v2,{case['count']},0\n"
        text += '\n'.join('BT,' + ','.join(map(str, r)) for r in case['records']) + '\n'
        cases, complete = a.parse(text)
        self.assertFalse(complete)
        self.assertEqual(a.analyze_case(cases[0])['result'], 'INCOMPLETE')
        cases, complete = a.parse(text + 'BOUNDARY_END,synthetic\nBOUNDARY_SUITE_COMPLETE\n')
        self.assertTrue(complete)
        self.assertEqual(a.analyze_case(cases[0])['result'], 'TRACE_PASS')

    def test_instrumentation_patch_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'driver.c'
            path.write_text('anchor anchor')
            with self.assertRaises(RuntimeError):
                patch(path, 'anchor', 'changed')
            self.assertEqual(path.read_text(), 'anchor anchor')
            with self.assertRaises(RuntimeError):
                patch(path, 'missing', 'changed')
            self.assertEqual(path.read_text(), 'anchor anchor')
            patch(path, 'anchor anchor', 'instrumented')
            self.assertEqual(path.read_text(), 'instrumented')

    def test_raw_insufficient_evidence(self):
        case = trace()
        case['scope'] = 'raw'
        self.assertEqual(a.analyze_case(case)['result'], 'INCOMPLETE')

    def test_oneshot_waits_for_actual_vsync(self):
        case = {'name': 'rgb_oneshot', 'scope': 'raw', 'dropped': 0, 'ended': True, 'records': []}
        def emit(kind, buffer=0):
            index = len(case['records'])
            case['records'].append((index, index * 10, kind, buffer, 0, 0))
        emit(a.REGISTER, 100)
        emit(a.REGISTER, 101)
        emit(a.CHAIN)
        for _ in range(6):
            emit(a.RAW_REQUEST, 101)
            emit(a.FRAME_DONE, 101)
            emit(a.VSYNC)
        emit(a.STOP)
        case['count'] = len(case['records'])
        self.assertEqual(a.analyze_case(case)['result'], 'CHARACTERIZED')
        self.assertEqual(a.analyze_case(case)['eof_to_vsync_us'], {'min': 10, 'max': 10})
        records = case['records']
        index = next(i for i, record in enumerate(records) if record[2] == a.VSYNC)
        record = list(records[index]); record[2] = a.CHAIN; records[index] = tuple(record)
        self.assertEqual(a.analyze_case(case)['result'], 'FAIL')

    def test_matrix_cannot_duplicate_a_mode(self):
        cases = []
        for count in (2, 3):
            for contract in ('partition', 'full'):
                case = trace(count)
                case['name'] = f'S3_fb{count}_{contract}_r90'
                cases.append(case)
        self.assertEqual(a.matrix_errors(cases, 4), [])
        cases[1] = cases[0]
        self.assertTrue(a.matrix_errors(cases, 4))

    def test_restart_request_requires_observation(self):
        case = trace()
        index = len(case['records'])
        case['records'].append((index, 10000, a.INJECT, 0, 0, 0))
        case['count'] = len(case['records'])
        self.assertEqual(a.analyze_case(case)['result'], 'INCOMPLETE')

    def test_raw_cannot_claim_v2(self):
        case = trace()
        case['scope'] = 'raw'
        for index in range(6):
            case['records'].append((len(case['records']), 10000 + index, a.RAW_REQUEST, 100, 0, 0))
        case['records'].append((len(case['records']), 10010, a.CHAIN, 0, 3, 0))
        case['count'] = len(case['records'])
        self.assertEqual(a.analyze_case(case)['result'], 'CHARACTERIZED')


if __name__ == '__main__':
    unittest.main()
