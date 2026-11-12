# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Collect whole-image static SRAM and common-start heap baselines from benchmark artifacts."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def collect(variant):
    elf = variant / 'build/present_benchmark.elf'
    output = subprocess.check_output(['readelf', '-SW', str(elf)], text=True)
    sections = {}
    for line in output.splitlines():
        match = re.match(r'\s*\[\s*\d+\]\s+(\.\S+)\s+\S+\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)', line)
        if match:
            sections[match[1]] = int(match[2], 16)
    iram = sum(v for k, v in sections.items() if k.startswith('.iram') and '.dummy' not in k)
    data = sum(v for k, v in sections.items() if k.startswith('.dram') and '.dummy' not in k) + sections.get('.noinit', 0)
    # Dummy sections alias already-counted IRAM/PSRAM reservations. RTC and
    # P4 SPM are separate regions, not part of this ordinary SRAM subtotal.
    baselines = {}
    log = (variant / 'live.log').read_text(errors='replace')
    for phase in ('before_init', 'after_init'):
        matches = re.findall(r'MEMORY mode=SUITE rotation=0 phase=' + phase + r' ([^\n]+)', log)
        if len(matches) != 1:
            raise ValueError(f'Expected one suite baseline: {variant}/{phase}')
        baselines[phase] = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', matches[0])}
    project = json.loads((variant / 'build/project_description.json').read_text())
    components = project['build_components']
    jpeg_components = [c for c in components if re.search(r'jpeg|esp_new_jpg|esp_lv_decoder', c, re.I)]
    symbols = subprocess.check_output(['readelf', '-sW', str(elf)], text=True)
    jpeg_symbols = sorted(set(re.findall(r'\b(?:esp_jpeg\w*|esp_new_jpg\w*|esp_lv_decoder\w*|jpeg_(?:create|read|start)\w*)\b', symbols)))
    return {'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
            'iram_bytes': iram, 'dram_static_bytes': data, 'sram_sections_bytes': iram + data,
            'sections': {k: v for k, v in sections.items() if k.startswith(('.iram', '.dram')) or k == '.noinit'},
            'suite_baselines': baselines, 'jpeg_components': jpeg_components, 'jpeg_symbols': jpeg_symbols}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', action='append', required=True, metavar='BOARD:IMPLEMENTATION:PATH')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    data = {'schema': 1, 'scope': 'whole benchmark firmware; not component-exclusive', 'boards': {}}
    for item in args.variant:
        board, implementation, path = item.split(':', 2)
        if implementation not in ('present', 'adapter') or implementation in data['boards'].get(board, {}):
            raise ValueError('Invalid or duplicate implementation')
        data['boards'].setdefault(board, {})[implementation] = collect(Path(path))
    args.output.write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    main()
