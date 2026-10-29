# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Decode relocated ELF crash PCs using the optional unstripped ELF sidecar.

For Xtensa, --runtime-base is the executable .text alias; for RISC-V, it is
psegment (the allocation address of the first / lowest PT_LOAD segment).
"""

import argparse
from pathlib import Path
import struct
import subprocess


PT_LOAD = 1
PF_X = 1
EM_XTENSA = 94
EM_RISCV = 243
SHF_EXECINSTR = 4
SHF_ALLOC = 2


class DebugELF:
    def __init__(self, path):
        self.path = Path(path)
        data = self.path.read_bytes()
        if len(data) < 52 or data[:4] != b'\x7fELF' or data[4] != 1 or data[5] not in (1, 2):
            raise ValueError('not an ELF32 image with supported endianness')
        fmt = '<' if data[5] == 1 else '>'
        self.machine = struct.unpack_from(fmt + 'H', data, 18)[0]
        if self.machine not in (EM_XTENSA, EM_RISCV):
            raise ValueError(f'unsupported ELF machine {self.machine}')
        phoff, shoff = struct.unpack_from(fmt + 'II', data, 28)
        phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from(fmt + 'HHHHH', data, 42)
        if (phnum and (phentsize != 32 or phoff + phnum * 32 > len(data))) or (
                shnum == 0 or shentsize != 40 or shoff + shnum * 40 > len(data) or
                shstrndx == 0 or shstrndx >= shnum):
            raise ValueError('invalid ELF header table boundaries')
        self.segments = []
        for i in range(phnum):
            kind, offset, vaddr, _paddr, file_size, mem_size, flags, _align = struct.unpack_from(
                fmt + '8I', data, phoff + 32 * i)
            if kind == PT_LOAD:
                if file_size > mem_size or offset + file_size > len(data):
                    raise ValueError('invalid PT_LOAD file range')
                self.segments.append((vaddr, file_size, flags))
        if not self.segments:
            raise ValueError('no loadable segments')
        self.sections = {}
        headers = [struct.unpack_from(fmt + '10I', data, shoff + 40 * i) for i in range(shnum)]
        strhdr = headers[shstrndx]
        if strhdr[1] != 3 or strhdr[4] + strhdr[5] > len(data):
            raise ValueError('invalid section string table')
        names = data[strhdr[4]:strhdr[4] + strhdr[5]]
        for entry in headers:
            nameoff = entry[0]
            if nameoff >= len(names):
                raise ValueError('section name offset out of range')
            end = names.find(b'\x00', nameoff)
            if end == -1:
                raise ValueError('unterminated ELF section name')
            name = names[nameoff:end].decode('utf-8', errors='replace')
            self.sections[name] = entry

    @property
    def has_debug_info(self):
        return any(name in self.sections for name in ('.debug_line', '.zdebug_line'))

    def virtual_address(self, pc, runtime_base):
        if pc < 0 or runtime_base < 0 or pc < runtime_base or pc > 0xffffffff or runtime_base > 0xffffffff:
            raise ValueError('PC lies before the ELF runtime base or exceeds 32 bits')
        displacement = pc - runtime_base
        if self.machine == EM_RISCV:
            # esp_elf_load_segment() allocates the lowest PT_LOAD address first.
            elf_base = min(vaddr for vaddr, _length, _flags in self.segments)
            virtual = elf_base + displacement
            valid = any(flags & PF_X and vaddr <= virtual < vaddr + length
                        for vaddr, length, flags in self.segments)
        else:
            # Xtensa loads .text separately. Data and instruction aliases may
            # differ (S2/S3 PSRAM), so the caller must pass executable base.
            text = self.sections.get('.text')
            if text is None or (text[2] & (SHF_ALLOC | SHF_EXECINSTR)) != (
                    SHF_ALLOC | SHF_EXECINSTR):
                raise ValueError('no allocated executable .text section')
            virtual = text[3] + displacement
            valid = text[3] <= virtual < text[3] + text[5]
        if virtual > 0xffffffff or not valid:
            raise ValueError('PC is not inside file-backed executable ELF memory')
        return virtual


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', required=True, type=Path, help='unstripped .app.elf.debug / .so.debug')
    parser.add_argument('--pc', required=True, action='append', type=lambda value: int(value, 0),
                        help='crash PC; repeat for multiple stack frames')
    parser.add_argument('--runtime-base', required=True, type=lambda value: int(value, 0),
                        help='runtime executable .text address (Xtensa) or psegment address (RISC-V)')
    parser.add_argument('--addr2line', required=True,
                        help='matching target-toolchain addr2line executable')
    args = parser.parse_args(argv)
    try:
        elf = DebugELF(args.elf)
        if not elf.has_debug_info:
            raise ValueError('no .debug_line: rebuild with -DELF_LOADER_KEEP_DEBUG_ELF=ON and DWARF enabled')
        for pc in args.pc:
            vaddr = elf.virtual_address(pc, args.runtime_base)
            result = subprocess.run([args.addr2line, '-f', '-C', '-e', str(args.elf),
                                     f'0x{vaddr:08x}'], check=True, text=True,
                                    capture_output=True)
            source_lines = result.stdout.strip().splitlines()
            if len(source_lines) < 2 or source_lines[-1] in ('??:0', '??:?'):
                raise ValueError('addr2line returned no source location: check matching ELF and toolchain')
            print(f'PC 0x{pc:08x} -> ELF 0x{vaddr:08x}\n{result.stdout.rstrip()}')
        return 0
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f'elf_addr2line: {error}\n')


if __name__ == '__main__':
    raise SystemExit(main())
