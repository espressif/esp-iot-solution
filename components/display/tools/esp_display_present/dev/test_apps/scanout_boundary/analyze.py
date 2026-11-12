# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Validate recorded source access/retirement; never infer optical presentation."""
import argparse
from collections import Counter
import json
import re
from pathlib import Path

REGISTER, READ_BEGIN, READ_END, SELECT, BOUNCE_EOF, FRAME_DONE, VSYNC, LINK_SWITCH, RESTART, UNDERRUN, SUBMIT, ARM, CONFIRM, RELEASE, WRITE_BEGIN, WRITE_END, CHAIN, RAW_REQUEST, STOP, INJECT, CALLBACK = range(1, 22)
NAMES = {value: name for name, value in list(globals().items()) if name.isupper() and isinstance(value, int)}


def analyze_case(case):
    errors = []
    incomplete = []
    counts = Counter()
    records = case['records']
    if not case.get('ended') or len(records) != case['count']:
        incomplete.append('truncated capture')
    if case['dropped']:
        incomplete.append('trace overflow; evidence lost')
    buffers = set()
    writers = set()
    readers = Counter()
    selected = None
    pending = None
    armed = False
    callbacks = 0
    confirmed_target = None
    last_time = -1
    margins = []
    last_read_end = {}
    scope = case['scope']
    oneshot = scope == 'raw' and 'oneshot' in case['name']
    raw_pending = None
    raw_eof_time = None
    eof_to_vsync = []
    for index, record in enumerate(records):
        sequence, timestamp, kind, buffer, detail, isr = record
        counts[NAMES.get(kind, f'UNKNOWN_{kind}')] += 1
        if sequence != index or timestamp < last_time:
            incomplete.append(f'nonmonotonic sequence/time at {sequence}')
        last_time = timestamp
        if kind not in NAMES:
            incomplete.append(f'unknown event {kind}')
        if oneshot and kind == RAW_REQUEST:
            if raw_pending is not None:
                errors.append('single-shot restarted before EOF and VSYNC')
            raw_pending = buffer
            raw_eof_time = None
        elif oneshot and kind == FRAME_DONE and raw_pending == buffer:
            raw_eof_time = timestamp
        elif oneshot and kind == VSYNC and raw_eof_time is not None:
            eof_to_vsync.append(timestamp - raw_eof_time)
            raw_pending = None
            raw_eof_time = None
        if kind == REGISTER:
            buffers.add(buffer)
        elif kind == READ_BEGIN:
            if buffer in writers:
                errors.append(f'read overlaps writer: {buffer:#x} at {sequence}')
            readers[buffer] += 1
        elif kind == READ_END:
            # First end may retire initial DMA started before trace registration.
            readers[buffer] = max(0, readers[buffer] - 1)
            last_read_end[buffer] = timestamp
        elif kind == SELECT:
            if buffer in writers:
                errors.append(f'DMA selects writable FB: {buffer:#x} at {sequence}')
            selected = buffer
        elif kind == RESTART:
            # RGB VSYNC ISR may abort a partially filled frame before refilling.
            # SELECT still protects its source; this is not proof of an actual last bus beat.
            readers[buffer] = 0
        elif kind == WRITE_BEGIN:
            if buffer not in buffers:
                errors.append(f'unregistered write target {buffer:#x}')
            if buffer in writers or readers[buffer] or selected == buffer:
                errors.append(f'unsafe write admission: {buffer:#x} at {sequence}')
            writers.add(buffer)
        elif kind == WRITE_END:
            if buffer not in writers:
                errors.append(f'write end without begin at {sequence}')
            writers.discard(buffer)
        elif kind == SUBMIT:
            if pending:
                errors.append('more than one pending scanout submission')
            if buffer in writers:
                errors.append('submit before construction finished')
            pending = (buffer, detail)
            confirmed_target = None
            armed = False
        elif kind == ARM:
            if pending != (buffer, detail):
                errors.append('arm does not match submission')
            armed = True
            callbacks = 0
        elif kind == CALLBACK and detail:
            callbacks += 1
        elif kind == CONFIRM:
            if not pending or pending[1] != detail or not armed:
                errors.append('confirmation has no matching armed submission')
            else:
                if callbacks < 2:
                    errors.append('default two-boundary fence confirmed too early')
                if selected != pending[0]:
                    errors.append('confirmation does not match observed selected FB')
                confirmed_target = pending[0]
            pending = None
            armed = False
        elif kind == RELEASE:
            if confirmed_target is None or buffer == confirmed_target:
                errors.append('release without confirmed successor')
            if selected == buffer or readers[buffer]:
                errors.append(f'release while source still selected/read: {buffer:#x}')
            if buffer not in last_read_end:
                incomplete.append(f'no last-read evidence for released FB {buffer:#x}')
            else:
                margins.append(timestamp - last_read_end[buffer])
        elif kind == UNDERRUN:
            errors.append('driver underrun observed')
    if counts['INJECT'] and not counts['RESTART']:
        incomplete.append('restart requested but not observed')
    if not counts['STOP']:
        incomplete.append('missing successful stop')
    if scope == 'v2':
        for name in ('REGISTER', 'READ_BEGIN', 'READ_END', 'SELECT', 'SUBMIT', 'ARM', 'CONFIRM', 'RELEASE', 'WRITE_BEGIN', 'WRITE_END', 'CALLBACK', 'FRAME_DONE', 'VSYNC'):
            if not counts[name]:
                incomplete.append(f'missing {name} evidence')
        if pending or writers or counts['SUBMIT'] != counts['CONFIRM'] or counts['WRITE_BEGIN'] != counts['WRITE_END']:
            incomplete.append('unfinished frame/transaction')
        if counts['CONFIRM'] < 6 or len(buffers) not in (2, 3):
            incomplete.append('insufficient frame/buffer coverage')
    else:
        if counts['RAW_REQUEST'] != 6 or not counts['CHAIN'] or not counts['VSYNC'] or not counts['FRAME_DONE'] or len(buffers) not in (2, 3):
            incomplete.append('missing raw request/topology/frame evidence')
    if oneshot and (raw_pending is not None or len(eof_to_vsync) != 6):
        incomplete.append('missing six EOF-to-VSYNC single-shot completions')
    return {
        'name': case['name'], 'scope': scope,
        'result': 'FAIL' if errors else 'INCOMPLETE' if incomplete else 'TRACE_PASS' if scope == 'v2' else 'CHARACTERIZED',
        'errors': errors, 'incomplete': sorted(set(incomplete)), 'counts': dict(counts),
        'min_release_margin_us': min(margins) if margins else None,
        'eof_to_vsync_us': {'min': min(eof_to_vsync), 'max': max(eof_to_vsync)} if eof_to_vsync else None,
        'note': 'Driver-observed source access only; not optical/no-tearing proof. Raw mode does not validate V2 retirement.',
    }


def parse(text):
    cases = []
    current = None
    complete = False
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith('BOUNDARY_BEGIN,'):
            if current:
                cases.append(current)
            _, name, scope, count, dropped = line.split(',')
            current = {'name': name, 'scope': scope, 'count': int(count), 'dropped': int(dropped), 'records': [], 'ended': False}
        elif line.startswith('BT,') and current:
            fields = line.split(',')[1:]
            if len(fields) != 6:
                raise ValueError(f'malformed record: {line}')
            current['records'].append(tuple(int(field, 0) for field in fields))
        elif line.startswith('BOUNDARY_END,') and current:
            if line.split(',', 1)[1] != current['name']:
                raise ValueError('mismatched case end')
            current['ended'] = True
            cases.append(current)
            current = None
        elif line == 'BOUNDARY_SUITE_COMPLETE':
            complete = True
    if current:
        cases.append(current)
    return cases, complete


def matrix_errors(cases, expected):
    if len(cases) != expected:
        return ['wrong number of cases']
    scopes = {case['scope'] for case in cases}
    if scopes == {'v2'} and expected in (4, 16):
        pattern = r'(.+)_fb([23])_(partition|full)_r(0|90|180|270)'
        matches = [re.fullmatch(pattern, case['name']) for case in cases]
        if not all(matches) or len({match[1] for match in matches}) != 1:
            return ['unknown/mixed board matrix']
        actual = {(int(match[2]), match[3], int(match[4])) for match in matches}
        rotations = (90,) if expected == 4 else (0, 90, 180, 270)
        required = {(count, contract, rotation) for count in (2, 3)
                    for contract in ('partition', 'full') for rotation in rotations}
        return [] if actual == required else ['missing or duplicated V2 mode']
    if scopes == {'raw'} and expected == 2:
        counts = {len({record[3] for record in case['records'] if record[2] == REGISTER}) for case in cases}
        return [] if counts == {2, 3} else ['raw matrix must include both two and three FBs']
    return ['unsupported/mixed matrix scope']


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--expected-cases', type=int, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    raw_text = args.log.read_text(errors='replace')
    cases, complete = parse(raw_text)
    firmware_fault = any(marker in raw_text for marker in ('Guru Meditation', 'assert failed:', 'abort() was called', 'Task watchdog got triggered'))
    results = [analyze_case(case) for case in cases]
    matrix = matrix_errors(cases, args.expected_cases)
    valid = not matrix and not firmware_fault and complete and len(cases) == args.expected_cases and all(result['result'] in ('TRACE_PASS', 'CHARACTERIZED') for result in results)
    report = {'matrix_errors': matrix, 'firmware_fault': firmware_fault, 'capture_complete': complete, 'expected_cases': args.expected_cases, 'cases': results, 'accepted': valid}
    rendered = json.dumps(report, indent=2, ensure_ascii=False) + '\n'
    print(rendered, end='')
    if args.output:
        args.output.write_text(rendered)
    raise SystemExit(0 if valid else 1)
