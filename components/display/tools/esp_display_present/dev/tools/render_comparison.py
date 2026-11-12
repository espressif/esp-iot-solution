# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Render the comparison dashboard from recorded data, without rerunning tests."""
import argparse
import csv
import gzip
import hashlib
import json
import os
import re
from pathlib import Path

MODES = ['DOUBLE_FULL', 'TRIPLE_FULL', 'DOUBLE_DIRECT',
         'DOUBLE_PARTIAL', 'TRIPLE_PARTIAL', 'AUTO']
ANGLES = [0, 90, 180, 270]
BOARDS = {
    's3-rgb': ('S3 RGB', '480 × 480 · R16', '10 行 bounce · FB_IRAM 状态见测试来源'),
    's31-rgb': ('S31 Korvo RGB', '800 × 480', '非 bounce · AXI link-switch'),
    'p4-dsi': ('P4 MIPI DSI', '1024 × 600', 'DW GDMA'),
}


def apply_lifecycle(payload, evidence, output_path):
    """Overlay only measured cases; retain the dated reference and diagnostics."""
    expected = {(m, a) for m in ('DOUBLE_FULL', 'TRIPLE_FULL', 'DOUBLE_DIRECT', 'AUTO') for a in (0, 90)}
    hashes = {}
    for board in payload['boards']:
        for cohort in board['cohorts'].values():
            for rows in cohort.values():
                for row in rows:
                    row['origin'] = '历史 11-10'
        path = evidence / board['id'] / 'results.json'
        raw = path.read_bytes()
        result = json.loads(raw)
        hashes[board['id']] = hashlib.sha256(raw).hexdigest()
        if not result['complete'] or result['errors'] or len(result['rows']) != 8:
            raise ValueError('Incomplete lifecycle suite')
        if {(r['mode'], r['rotation']) for r in result['rows']} != expected:
            raise ValueError('Unexpected lifecycle coverage')
        manifest = json.loads((path.parent / 'manifest.json').read_text())
        if manifest['xip_mode'] != 'on' or manifest['driver_trace'] is not None:
            raise ValueError('Expected clean XIP ON firmware')
        log = gzip.open(path.parent / 'live.log.gz', 'rt').read()
        scores = {}
        for mode, angle, name, fps in re.findall(
                r'SCENE mode=(\w+) rotation=(\d+) name="([^"]+)" fps=(\d+)', log):
            scores.setdefault((mode, int(angle)), []).append({'name': name, 'fps': int(fps)})
        current = {(r['mode'], r['rotation']): r for r in result['rows']}
        rows = board['cohorts']['clean']['present']
        for index, previous in enumerate(rows):
            key = (previous['mode'], previous['rotation'])
            if key not in current:
                continue
            row = dict(current[key])
            if row['status'] != 'PASS' or row['scenes'] != 16 or len(scores.get(key, [])) != 16:
                raise ValueError('Unfinished lifecycle case or missing scene scores')
            row['scene_scores'] = scores[key]
            row['origin'] = '新测 11-11'
            row['source_commit'] = 'aadc0d799'
            row['previous_open_psram_bytes'] = (previous['memory']['before_open']['psram_free']
                                                - previous['memory']['after_open']['psram_free'])
            rows[index] = row
        for row in board['cohorts']['clean']['adapter']:
            row.pop('transport_fps', None)  # Adapter logger does not measure this metric.
        board['originNote'] = '11-11 新测 8 项；其余 present、adapter、静态 SRAM 和交接诊断保留 11-10 来源'
    payload['lifecycle'] = {
        'count': 24, 'scenes': 384, 'sourceCommit': 'aadc0d799', 'hashes': hashes,
        'report': os.path.relpath(evidence / 'README.md', output_path.parent).replace(os.sep, '/'),
    }
    payload['roundNote'] = ('11-11：present 三板各完整运行 8 项（FULL / DIRECT / AUTO，0° / 90°），合计 24 项、384 场景。'
                            '新测单元格标注日期；其他 present 成绩、adapter、静态 SRAM 和独立交接诊断保留 11-10 历史数据。'
                            '本轮未重测 180° / 270° 和显式 PARTIAL，不是同版本完整矩阵。')
    payload['updated'] = '2026-11-11'
    payload['downloads'] = {'csv': output_path.with_suffix('.csv').name, 'json': output_path.with_suffix('.json').name}


def render_unified(source, raw, data_path, output_path):
    """Validate a five-board snapshot without claiming every record was rerun."""
    if source.get('angles') != ANGLES or len(source.get('boards', [])) != 5:
        raise ValueError('Expected five boards and four angles')
    payload = source.copy()
    payload['unified'] = True
    payload['dataSha256'] = hashlib.sha256(raw).hexdigest()
    payload['downloads'] = {'csv': output_path.with_suffix('.csv').name, 'json': output_path.with_suffix('.json').name}
    payload['linksPrefix'] = './'
    payload['dataFilename'] = data_path.name
    totals = {'clean': 0, 'diagnostic': 0, 'scenes': 0, 'skip': 0, 'cleanFailed': 0, 'diagnosticFailed': 0}
    for board in payload['boards']:
        modes = board['modes']
        if board['scanout'] and 'NONE' in modes:
            raise ValueError('Scanout NONE is excluded')
        expected = {(mode, angle) for mode in modes for angle in ANGLES}
        for cohort in ('clean', 'diagnostic'):
            for impl in ('present', 'adapter'):
                rows = board['cohorts'][cohort][impl]
                if len(rows) != len(expected) or {(r['mode'], r['rotation']) for r in rows} != expected:
                    raise ValueError(f'Missing/duplicate cells: {board["id"]}/{cohort}/{impl}')
                for row in rows:
                    status = row['status']
                    if status == 'PASS':
                        if row.get('scenes') != 16 or not row.get('origin'):
                            raise ValueError('Every score requires a complete suite and source label')
                        if cohort == 'clean':
                            if len(row.get('scene_scores', [])) != 16 or not all(phase in row.get('memory', {}) for phase in ('before_open', 'after_open', 'after_run', 'after_close')):
                                raise ValueError('Score lacks scene or memory evidence')
                        elif 'address' not in row:
                            raise ValueError('Diagnostic PASS lacks observer data')
                        totals[cohort] += 1
                        totals['scenes'] += 16
                    elif status == 'FAIL':
                        if not row.get('failure_reason'):
                            raise ValueError('Failure reason is required')
                        totals[cohort+'Failed'] += 1
                    elif status == 'SKIP':
                        totals['skip'] += 1
                    elif status not in ('UNPAIRED', 'MISSING') or not row.get('reason'):
                        raise ValueError('Unmeasured/unpaired cell requires an explanation')
    payload['totals'] = totals
    embedded = json.dumps(payload, ensure_ascii=False, allow_nan=False, separators=(',', ':')).replace('<', '\u003c')
    output_path.parent.mkdir(parents=True, exist_ok=True)
    template = Path(__file__).with_name('comparison_dashboard.html').read_text()
    output_path.write_text(template.replace('__REPORT_DATA__', embedded))
    output_path.with_suffix('.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2)+'\n')
    phases = ('before_open', 'after_open', 'after_run', 'after_close')
    with output_path.with_suffix('.csv').open('w', newline='') as stream:
        writer = csv.writer(stream, lineterminator='\n')
        writer.writerow(['board', 'implementation', 'mode', 'rotation', 'status', 'origin', 'condition', 'reason', 'lvgl_fps', 'transport_fps', *[f'{h}_{p}_remaining_bytes' for h in ('internal', 'psram') for p in phases]])
        for board in payload['boards']:
            for impl in ('present', 'adapter'):
                for row in board['cohorts']['clean'][impl]:
                    mem = row.get('memory', {})
                    writer.writerow([board['id'], impl, row['mode'], row['rotation'], row['status'], row.get('origin', ''), row.get('condition', ''), row.get('reason', row.get('failure_reason', '')), row.get('lvgl_fps', ''), row.get('transport_fps', ''), *[mem.get(p, {}).get(h+('_min_free' if p=='after_run' else '_free'), '') for h in ('internal', 'psram') for p in phases]])
    return {'output': str(output_path), 'data_sha256': payload['dataSha256'], **totals}


def render(data_path, output_path, lifecycle=None):
    raw = data_path.read_bytes()
    source = json.loads(raw)
    if source.get('schema') == 'display-dashboard-v2':
        return render_unified(source, raw, data_path, output_path)
    if not source.get('complete') or source.get('xip') != 'on' or source.get('angles') != ANGLES:
        raise ValueError('Expected a complete four-angle XIP ON dataset')
    payload = {
        'updated': source['updated'], 'sourceCommit': source['source_commit'],
        'dataSha256': hashlib.sha256(raw).hexdigest(), 'modes': MODES, 'angles': ANGLES,
        'boards': [], 'totals': {'clean': 0, 'diagnostic': 0, 'scenes': 0, 'skip': 0, 'cleanFailed': 0, 'diagnosticFailed': 0},
        'linksPrefix': os.path.relpath(data_path.parent, output_path.parent).replace(os.sep, '/') + '/',
        'dataFilename': data_path.name,
        'roundNote': source.get('round_note', '全部四角度重新完整运行，未用历史 0° 或 XIP OFF 成绩补表。'),
    }
    expected = {(mode, angle) for mode in MODES for angle in ANGLES}
    for key, (name, resolution, config) in BOARDS.items():
        original = source['boards'][key]
        board = {'id': key, 'name': name, 'resolution': resolution, 'config': original.get('config', config),
                 'method': source['methods'][key], 'cohorts': {},
                 'originNote': original.get('origin_note', ''),
                 'frameBytes': {'s3-rgb': 480 * 480 * 2, 's31-rgb': 800 * 480 * 2, 'p4-dsi': 1024 * 600 * 2}[key],
                 'memoryAccounting': source.get('memory_accounting', {}).get('boards', {}).get(key)}
        for cohort in ('clean', 'diagnostic'):
            board['cohorts'][cohort] = {}
            for implementation in ('present', 'adapter'):
                entries = original['cohorts'][cohort][implementation]['rows']
                if len(entries) != len(expected) or {(r['mode'], r['rotation']) for r in entries} != expected:
                    raise ValueError(f'Missing or duplicate cases: {key}/{cohort}/{implementation}')
                rows = []
                for entry in entries:
                    fields = ('mode', 'rotation', 'status', 'lvgl_fps', 'transport_fps', 'memory', 'scene_scores', 'failure_reason') if cohort == 'clean' else (
                        'mode', 'rotation', 'status', 'address', 'supply', 'address_events', 'failure_reason')
                    rows.append({field: entry[field] for field in fields if field in entry})
                    if entry['status'] == 'SKIP':
                        if implementation != 'adapter' or entry['mode'] != 'AUTO':
                            raise ValueError('Unexpected skipped case')
                        payload['totals']['skip'] += 1
                    elif entry['status'] == 'FAIL' and source.get('current_round'):
                        payload['totals'][cohort + 'Failed'] += 1
                    elif entry['status'] == 'PASS' and entry['scenes'] == 16:
                        payload['totals'][cohort] += 1
                        payload['totals']['scenes'] += entry['scenes']
                    else:
                        raise ValueError('Unfinished case cannot be published')
                board['cohorts'][cohort][implementation] = rows
        payload['boards'].append(board)
    if lifecycle is not None:
        apply_lifecycle(payload, lifecycle, output_path)
    if source.get('current_round'):
        if lifecycle is not None:
            raise ValueError('A current full round cannot mix lifecycle overlays')
        payload['currentRound'] = source['current_round']
        payload['downloads'] = {'csv': output_path.with_suffix('.csv').name, 'json': output_path.with_suffix('.json').name}
        for board in payload['boards']:
            for row in board['cohorts']['clean']['adapter']:
                row.pop('transport_fps', None)
    template = Path(__file__).with_name('comparison_dashboard.html').read_text()
    embedded = json.dumps(payload, ensure_ascii=False, allow_nan=False, separators=(',', ':')).replace('<', '\\u003c')
    page = template.replace('__REPORT_DATA__', embedded)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(page)
    if lifecycle is not None or source.get('current_round'):
        output_path.with_suffix('.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2) + '\n')
        with output_path.with_suffix('.csv').open('w', newline='') as stream:
            writer = csv.writer(stream, lineterminator='\n')
            writer.writerow(['board', 'implementation', 'mode', 'rotation', 'origin', 'status', 'failure_reason',
                             'lvgl_fps', 'transport_fps', *[f'{heap}_{phase}_remaining_bytes'
                                                          for heap in ('internal', 'psram')
                                                          for phase in ('before_open', 'after_open', 'after_run', 'after_close')]])
            for board in payload['boards']:
                for impl, rows in board['cohorts']['clean'].items():
                    for row in rows:
                        memory = row.get('memory', {})
                        writer.writerow([board['id'], impl, row['mode'], row['rotation'], row.get('origin', source.get('current_round', {}).get('label', '')), row['status'], row.get('failure_reason', ''),
                                         row.get('lvgl_fps', ''), row.get('transport_fps', ''),
                                         *[memory.get(phase, {}).get(heap + ('_min_free' if phase == 'after_run' else '_free'), '')
                                           for heap in ('internal', 'psram')
                                           for phase in ('before_open', 'after_open', 'after_run', 'after_close')]])
    return {'output': str(output_path), 'data_sha256': payload['dataSha256'], **payload['totals']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--lifecycle', type=Path, help='Optional completed 11-11 targeted regression directory')
    args = parser.parse_args()
    print(json.dumps(render(args.data.resolve(), args.output.resolve(), args.lifecycle.resolve() if args.lifecycle else None), ensure_ascii=False))


if __name__ == '__main__':
    main()
