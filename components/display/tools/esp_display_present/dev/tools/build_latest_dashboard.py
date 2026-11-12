# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Export the recorded five-board snapshot without hardware or historical logs."""
import argparse
import json
from pathlib import Path

DEFAULT = Path(__file__).resolve().parents[1] / 'evidence/display-latest-20261111/matrix-data.json'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, default=DEFAULT)
    parser.add_argument('--accounting', type=Path, help='Optional measured whole-image SRAM accounts')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.snapshot.read_bytes()
    data = json.loads(raw)
    if data.get('schema') != 'display-dashboard-v2' or len(data.get('boards', [])) != 5 or data.get('angles') != [0, 90, 180, 270]:
        parser.error('Expected the recorded five-board, four-angle snapshot')
    if args.accounting:
        accounts = json.loads(args.accounting.read_text())['boards']
        for board in data['boards']:
            item = accounts.get(board['id'])
            if item:
                board.update(memoryAccounting=item['accounts'], accountingNote=item['note'])
        raw = (json.dumps(data, ensure_ascii=False, indent=2) + '\n').encode()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(raw)
    print('exported', args.output)


if __name__ == '__main__':
    main()
