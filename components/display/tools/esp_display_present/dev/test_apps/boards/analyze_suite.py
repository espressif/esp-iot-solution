# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Summarize completed five-mode cycles; fail if evidence is missing or bad."""
import argparse
import json
import re
from pathlib import Path

MODES = {'DOUBLE_FULL', 'TRIPLE_FULL', 'DOUBLE_DIRECT', 'DOUBLE_PARTIAL', 'TRIPLE_PARTIAL'}
p = argparse.ArgumentParser()
p.add_argument('log', type=Path)
p.add_argument('--min-cycles', type=int, default=2)
a = p.parse_args()
s = a.log.read_text(errors='replace')
if re.search(r'\bFAIL\b|PIXEL mode=|Guru Meditation|assert failed|CORRUPT HEAP', s):
    raise SystemExit('Failure found in console; inspect the log')
if s.count('V2 SUITE2 ') > 1:
    raise SystemExit('Unexpected reboot/restart during capture')
passes = re.findall(r'PASS cycle=(\d+) mode=(\w+) frames=(\d+) pixels=ALL max_wait_us=(\d+) pending=0 sticky=ESP_OK', s)
clean = re.findall(r'CLEAN cycle=(\d+) mode=(\w+) internal=(\d+) psram=(\d+)', s)
complete = re.findall(r'SUITE_PASS cycle=(\d+) modes=5 confirmed_frames=240', s)
if len(set(complete)) < a.min_cycles:
    raise SystemExit(f'Only {len(set(complete))} complete cycles, need {a.min_cycles}')
summary = {}
for cycle in complete:
    pc = {m for c,m,f,t in passes if c == cycle and int(f) == 48}
    cc = {m for c,m,i,e in clean if c == cycle}
    if pc != MODES or cc != MODES or sum(c == cycle for c,m,f,t in passes) != 5:
        raise SystemExit(f'Incomplete case evidence for cycle {cycle}')
for mode in sorted(MODES):
    pp = [(int(f),int(t)) for c,m,f,t in passes if m == mode and c in complete]
    cc = [(int(i),int(e)) for c,m,i,e in clean if m == mode and c in complete]
    if len(set(cc)) != 1:
        raise SystemExit(f'Cleanup heap varies for {mode}: {cc}')
    summary[mode] = {'frames':sum(f for f,t in pp), 'max_wait_us':max(t for f,t in pp),
                     'cleanup_internal':cc[0][0], 'cleanup_psram':cc[0][1]}
print(json.dumps({'complete_cycles':len(set(complete)), 'modes':summary}, indent=2))
