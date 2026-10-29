# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Verify the aligned embedded ELF symbol in every built example firmware ELF."""

import argparse
from pathlib import Path
import re
import subprocess
import sys


EMBEDDED_SYMBOL = re.compile(
    r'^_binary_(?:test_elf_bin|test_riscv_elf|test_xtensa_elf)_start$'
)


def inspect_elf(elf_file):
    symbols = subprocess.check_output(
        ['readelf', '-Ws', str(elf_file)], text=True
    )
    addresses = []
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) >= 8 and EMBEDDED_SYMBOL.fullmatch(fields[-1]):
            addresses.append((fields[-1], int(fields[1], 16)))
    return addresses


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('project', type=Path)
    args = parser.parse_args()
    files = sorted(
        [*args.project.rglob('elf-loader.elf'),
         *args.project.rglob('elf_loader.elf')]
    )
    if not files:
        parser.error('No ELF loader firmware artifacts were built')

    failures = []
    for file in files:
        addresses = inspect_elf(file)
        if not addresses:
            failures.append(f"{file}: embedded ELF symbol not found")
        for symbol, address in addresses:
            if address % 4:
                failures.append(
                    f"{file}: {symbol}=0x{address:x} is not 4-byte aligned"
                )

    if failures:
        print('\n'.join(failures), file=sys.stderr)
        return 1

    print(f"Verified aligned embedded ELF symbols in {len(files)} firmware builds")
    return 0


if __name__ == '__main__':
    sys.exit(main())
