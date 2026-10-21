# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Verify project_elf retained 32-bit static ctor/dtor arrays and relocations.

Example:
    python3 components/elf_loader/tests/idf_cpp_lifecycle/check_elf_arrays.py \
        /tmp/build/elf_cpp_lifecycle.app.elf
"""

import argparse
from pathlib import Path
import struct


def verify_elf(path):
    image = path.read_bytes()
    assert image[:4] == b'\x7fELF' and image[4] == 1, 'expected ELF32 image'
    assert image[5] in (1, 2), 'invalid ELF endianness'
    endian = '<' if image[5] == 1 else '>'

    def unpack(fmt, offset):
        return struct.unpack_from(endian + fmt, image, offset)

    section_offset = unpack('I', 32)[0]
    section_size, section_count, names_index = unpack('HHH', 46)
    assert section_size == 40 and 0 < names_index < section_count
    assert section_offset + section_size * section_count <= len(image)
    sections = [unpack('10I', section_offset + i * section_size)
                for i in range(section_count)]
    name_start, name_len = sections[names_index][4:6]
    assert name_start + name_len <= len(image)
    strings = image[name_start:name_start + name_len]
    named = {}
    for sec in sections:
        end = strings.find(b'\0', sec[0])
        assert end != -1, 'invalid section name'
        named.setdefault(strings[sec[0]:end].decode('ascii'), []).append(sec)

    def required_alternative(names):
        match = [(name, sec) for name in names for sec in named.get(name, [])
                 if sec[5] > 0]
        assert len(match) == 1, 'expected exactly one of: ' + ', '.join(names)
        name, sec = match[0]
        assert sec[2] & 2, name + ' is not allocated'
        assert sec[3] % 4 == 0 and sec[5] % 4 == 0
        assert sec[4] + sec[5] <= len(image)
        return name, sec

    initial = required_alternative(('.init_array', '.ctors'))
    final = required_alternative(('.fini_array', '.dtors'))
    targets = (initial, final)
    relocated = {name: set() for name, _ in targets}
    for section in sections:
        if section[1] != 4:  # SHT_RELA
            continue
        offset, length = section[4:6]
        assert length % 12 == 0 and offset + length <= len(image)
        for rel in range(offset, offset + length, 12):
            virtual = unpack('I', rel)[0]
            for name, array in targets:
                if array[3] <= virtual < array[3] + array[5]:
                    assert (virtual - array[3]) % 4 == 0, (
                        name + ' has an unaligned function pointer relocation')
                    relocated[name].add(virtual)
    for name, section in targets:
        expected = set(range(section[3], section[3] + section[5], 4))
        assert relocated[name] == expected, (
            name + ' is missing function pointer relocations')
        print(f'{name}: {section[5] // 4} callbacks, {len(relocated[name])} relocations')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path, help='project_elf() ELF output')
    args = parser.parse_args()
    verify_elf(args.image)
