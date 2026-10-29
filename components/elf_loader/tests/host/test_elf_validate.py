# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Host-only malformed ELF regression tests (no ESP-IDF toolchain required)."""

import ctypes
import os
import shutil
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


# GNU ld and the macOS linker use different dead-section removal options.
HOST_GC_LINKER_FLAG = '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections'

ROOT = Path(__file__).resolve().parents[4]
COMPONENT = ROOT / 'components' / 'elf_loader'
FIXTURES = {
    'riscv': ROOT / 'examples/elf_loader/elf_loader_example/main/test_riscv.elf',
    'xtensa': ROOT / 'examples/elf_loader/elf_loader_example/main/test_xtensa.elf',
}


def u16(data, offset):
    return struct.unpack_from('<H', data, offset)[0]


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def put16(data, offset, value):
    struct.pack_into('<H', data, offset, value)


def put32(data, offset, value):
    struct.pack_into('<I', data, offset, value)


def section(data, index):
    return u32(data, 32) + index * 40


def first_section_of_type(data, kind):
    for i in range(u16(data, 48)):
        off = section(data, i)
        if u32(data, off + 4) == kind:
            return off
    raise AssertionError(f"missing section type {kind}")


def first_load_segment(data):
    off = u32(data, 28)
    for i in range(u16(data, 44)):
        ph = off + 32 * i
        if u32(data, ph) == 1:
            return ph
    raise AssertionError('missing PT_LOAD')


def synthetic_lifecycle_image(arch, got_loc=False):
    """ELF32 fixture with file-backed startup arrays on both architectures."""
    entries = [
        ('.text', 1, 6, 0x1000, 8),
        ('.rodata', 1, 2, 0x1020, 8),
        ('.preinit_array', 16, 3, 0x1040, 4),
        ('.init_array', 14, 3, 0x1044, 4),
        ('.fini_array', 15, 3, 0x1048, 4),
    ]
    if got_loc:
        entries.append(('.got.loc', 1, 2, 0x1080, 12))
    shstr = b'\0'
    names = {}
    for name, _, _, _, _ in entries + [('.shstrtab', 3, 0, 0, 0)]:
        names[name] = len(shstr)
        shstr += name.encode() + b'\0'

    num_sections = len(entries) + 2  # null section and .shstrtab
    shoff = 52 + 32
    stroff = shoff + 40 * num_sections
    dataoff = (stroff + len(shstr) + 3) & ~3
    data = bytearray(dataoff + (0xa0 if got_loc else 0x80))
    data[:7] = b'\x7fELF\x01\x01\x01'
    put16(data, 16, 2)  # ET_EXEC
    put16(data, 18, 94 if arch == 'xtensa' else 243)
    put32(data, 20, 1)
    put32(data, 24, 0x1000)
    put32(data, 28, 52)
    put32(data, 32, shoff)
    put16(data, 40, 52)
    put16(data, 42, 32)
    put16(data, 44, 1)
    put16(data, 46, 40)
    put16(data, 48, num_sections)
    put16(data, 50, num_sections - 1)
    struct.pack_into('<IIIIIIII', data, 52, 1, dataoff, 0x1000, 0x1000,
                     len(data) - dataoff, len(data) - dataoff, 5, 4)
    data[stroff:stroff + len(shstr)] = shstr

    for i, (name, sec_type, flags, vaddr, sec_size) in enumerate(entries, 1):
        struct.pack_into('<IIIIIIIIII', data, shoff + 40 * i, names[name],
                         sec_type, flags, vaddr, dataoff + (vaddr - 0x1000),
                         sec_size, 0, 0, 4, 0)
    struct.pack_into('<IIIIIIIIII', data, shoff + 40 * (num_sections - 1),
                     names['.shstrtab'], 3, 0, 0, stroff, len(shstr), 0, 0, 1, 0)
    return data


class ElfValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='elf-validation-')
        cls.libs = {}
        cc = os.environ.get('CC', 'cc')
        for arch in FIXTURES:
            output = Path(cls.build.name) / f"libvalidate_{arch}.so"
            flags = [f"-DCONFIG_IDF_TARGET_ARCH_{arch.upper()}=1",
                     '-DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=1']
            if arch == 'xtensa':
                flags.append('-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1')
            subprocess.run([
                cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-fPIC', '-shared',
                *flags,
                '-I', str(COMPONENT / 'tests/host/include'),
                '-I', str(COMPONENT / 'include'),
                str(COMPONENT / 'src/esp_elf_validate.c'),
                '-o', str(output),
            ], check=True)
            lib = ctypes.CDLL(str(output))
            lib.esp_elf_validate.argtypes = (ctypes.c_void_p, ctypes.c_size_t)
            lib.esp_elf_validate.restype = ctypes.c_int
            cls.libs[arch] = lib

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def validate(self, arch, payload):
        data = bytes(payload)
        buf = ctypes.create_string_buffer(data, len(data) + 1)
        return self.libs[arch].esp_elf_validate(
            ctypes.cast(buf, ctypes.c_void_p), len(data)
        )

    def test_elf_pc_translation_respects_loaded_executable_ranges(self):
        import importlib.util
        script = COMPONENT / 'tool/elf_addr2line.py'
        spec = importlib.util.spec_from_file_location('elf_addr2line', script)
        tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(tool)
        for arch in FIXTURES:
            elf = tool.DebugELF(FIXTURES[arch])
            runtime = 0x42000000
            if arch == 'xtensa':
                start = elf.sections['.text'][3]
                length = elf.sections['.text'][5]
                self.assertGreater(length, 1)
                self.assertEqual(elf.virtual_address(runtime + 1, runtime), start + 1)
                with self.assertRaisesRegex(ValueError, 'not inside'):
                    elf.virtual_address(runtime + length, runtime)
            else:
                start = min(segment[0] for segment in elf.segments)
                executable = next((vaddr, size, flags) for vaddr, size, flags in elf.segments
                                  if (flags & tool.PF_X) and size >= 2)
                offset = executable[0] + 1 - start
                self.assertEqual(elf.virtual_address(runtime + offset, runtime), executable[0] + 1)
                with self.assertRaises(ValueError):
                    elf.virtual_address(runtime - 1, runtime)
            with self.assertRaises(ValueError):
                elf.virtual_address(0x100000000, runtime)
        bad = Path(self.build.name) / 'bad-debug.elf'
        bad.write_bytes(b'not an elf')
        with self.assertRaisesRegex(ValueError, 'not an ELF32'):
            tool.DebugELF(bad)

    def test_elf_addr2line_invokes_target_toolchain_with_rebased_pc(self):
        import contextlib
        import importlib.util
        import io
        from unittest import mock
        script = COMPONENT / 'tool/elf_addr2line.py'
        spec = importlib.util.spec_from_file_location('elf_addr2line', script)
        tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(tool)
        elf = tool.DebugELF(FIXTURES['xtensa'])
        addr = elf.sections['.text'][3] + 1
        output = io.StringIO()
        with mock.patch.object(tool.DebugELF, 'has_debug_info',
                               new_callable=mock.PropertyMock, return_value=True), \
             mock.patch.object(tool.subprocess, 'run', return_value=subprocess.CompletedProcess(
                 [], 0, 'loaded_function\nsrc/test.cpp:37\n', '')) as spawn, \
             contextlib.redirect_stdout(output):
            code = tool.main([
                '--elf', str(FIXTURES['xtensa']),
                '--pc', '0x42000001', '--runtime-base', '0x42000000',
                '--addr2line', '/tmp/xtensa-addr2line',
            ])
        self.assertEqual(code, 0)
        self.assertEqual(spawn.call_args.args[0][-1], f'0x{addr:08x}')
        self.assertIn('src/test.cpp:37', output.getvalue())
        self.assertIn('0x42000001', output.getvalue())

    def test_elf_symbolicator_rejects_missing_source_mapping(self):
        import contextlib
        import importlib.util
        import io
        from unittest import mock
        script = COMPONENT / 'tool/elf_addr2line.py'
        spec = importlib.util.spec_from_file_location('elf_addr2line', script)
        tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(tool)
        with mock.patch.object(tool.DebugELF, 'has_debug_info',
                               new_callable=mock.PropertyMock, return_value=True), \
             mock.patch.object(tool.subprocess, 'run', return_value=subprocess.CompletedProcess(
                 [], 0, '??\n??:0\n', '')):
            with mock.patch.object(tool.argparse.ArgumentParser, 'exit',
                                   side_effect=SystemExit(2)) as exit_mock:
                with contextlib.redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit) as result:
                        tool.main([
                            '--elf', str(FIXTURES['xtensa']), '--pc', '0x42000001',
                            '--runtime-base', '0x42000000', '--addr2line', 'false',
                        ])
            self.assertEqual(result.exception.code, 2)
            self.assertIn('no source location', exit_mock.call_args.args[1])

    def test_cpp_elf_mangled_name_and_overload_exports(self):
        executable = Path(self.build.name) / 'test_cpp_exports'
        subprocess.run([
            os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_cpp_exports.cpp'),
            '-o', str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)

    def test_cross_compiled_cpp_export_names_match_elf_symbols(self):
        import shutil
        symbols = (
            '_ZN11loader_test5plainEi',
            '_ZN11loader_test10overloadedEi',
            '_ZN11loader_test10overloadedEd',
            '_ZN11loader_test6Widget3runEi',
        )
        tested = 0
        for toolchain in ('riscv32-esp-elf', 'xtensa-esp32s3-elf'):
            compiler = shutil.which(f'{toolchain}-g++')
            nm = shutil.which(f'{toolchain}-nm')
            if not compiler or not nm:
                continue
            tested += 1
            output = Path(self.build.name) / f'{toolchain}-cpp-exports.o'
            subprocess.run([
                compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-c',
                '-I', str(COMPONENT / 'include'),
                str(COMPONENT / 'tests/host/test_cpp_exports.cpp'),
                '-o', str(output),
            ], check=True)
            defined = subprocess.run([nm, '--defined-only', str(output)],
                                     check=True, capture_output=True, text=True).stdout
            data = output.read_bytes()
            for name in symbols:
                with self.subTest(target=toolchain, symbol=name):
                    self.assertIn(name, defined)
                    self.assertIn(name.encode() + b'\0', data)
        if not tested:
            self.skipTest('Xtensa/RISC-V cross compiler unavailable')

    def test_named_c_export_compatibility(self):
        executable = Path(self.build.name) / 'test_named_c_export'
        subprocess.run([
            os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_named_c_export.c'),
            '-o', str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)

    def test_vprintf_export_matches_available_libc_function(self):
        """Exercise the default export-table entry with the host C runtime."""
        import re
        source = (COMPONENT / 'src/esp_elf_symbol.c').read_text()
        table = source.split('g_esp_libc_elfsyms[] = {', 1)[1].split('};', 1)[0]
        names = re.findall(r'ESP_ELFSYM_EXPORT\(([^)]+)\)', table)
        self.assertEqual(names.count('vprintf'), 1)
        for name in ('printf', 'vfprintf'):
            self.assertIn(name, names)
        # Compile entries read from the actual table, not a copy of its names.
        exported = '\n'.join(f'    ESP_ELFSYM_EXPORT({name}),' for name in names
                             if name in ('printf', 'vprintf', 'vfprintf'))
        code = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct export_sym { const char *name; const void *ptr; };
#define ESP_ELFSYM_EXPORT(x) { #x, (const void *)&x }
static const struct export_sym exports[] = {
""" + exported + r"""
};
static uintptr_t lookup(const char *name) {
    for (size_t i = 0; i < sizeof(exports) / sizeof(exports[0]); ++i) {
        if (strcmp(exports[i].name, name) == 0) return (uintptr_t)exports[i].ptr;
    }
    return 0;
}
int main(void) {
    assert(lookup("vprintf") == (uintptr_t)&vprintf);
    assert(lookup("printf") == (uintptr_t)&printf);
    assert(lookup("vfprintf") == (uintptr_t)&vfprintf);
    assert(!lookup("esp_crosscore_int_send_yield"));
    return 0;
}
"""
        path = Path(self.build.name) / 'test_libc_exports.c'
        executable = Path(self.build.name) / 'test_libc_exports'
        path.write_text(code)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra',
                        '-Werror', str(path), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)

    def test_valid_production_fixtures(self):
        for arch, path in FIXTURES.items():
            additional = ROOT / 'examples/elf_loader/elf_console_example/main/fs_image' / arch
            for image in (path, additional / 'test_app.elf', additional / 'test_so.elf',
                          additional / 'lib.so'):
                with self.subTest(arch=arch, image=image.name):
                    raw = image.read_bytes()
                    self.assertEqual(self.validate(arch, raw), 0)
                    self.assertEqual(self.validate(arch, raw + b'extra'), 0)

    def test_shared_symbol_names_do_not_rescan_string_table(self):
        import time

        raw = bytearray(FIXTURES['xtensa'].read_bytes())
        shoff = u32(raw, 32)
        shnum = u16(raw, 48)
        headers = bytes(raw[shoff:shoff + shnum * 40])
        symbol_count = 40000

        def image_with_shared_name(string_size):
            data = bytearray(raw)
            data.extend(bytes(-len(data) % 4))
            string_offset = len(data)
            data.extend(b'\0' + b'x' * (string_size - 2) + b'\0')
            data.extend(bytes(-len(data) % 4))
            sym_offset = len(data)
            # Every symbol refers to the same long name, beginning at index 1.
            for _ in range(symbol_count):
                data.extend(struct.pack('<IIIbbH', 1, 0, 0, 0, 0, 0))
            data.extend(bytes(-len(data) % 4))
            hdr_offset = len(data)
            data.extend(headers)
            data.extend(struct.pack(
                '<IIIIIIIIII', 0, 2, 0, 0, sym_offset, symbol_count * 16,
                shnum + 1, 0, 4, 16,
            ))
            data.extend(struct.pack(
                '<IIIIIIIIII', 0, 3, 0, 0, string_offset, string_size,
                0, 0, 1, 0,
            ))
            put32(data, 32, hdr_offset)
            put16(data, 48, shnum + 2)
            return data

        timings = []
        for length in (1024, 400000):
            image = image_with_shared_name(length)
            started = time.perf_counter()
            self.assertEqual(self.validate('xtensa', image), 0)
            timings.append(time.perf_counter() - started)

            image[u32(image, section(image, shnum + 1) + 16) + length - 1] = ord('X')
            self.assertNotEqual(self.validate('xtensa', image), 0)

        # Old behavior scanned ~16 GB for the large fixture; every symbol
        # must now be checked in O(1) after validating the trailing NUL.
        self.assertLess(timings[1], max(0.08, timings[0] * 12))

    def test_many_relocations_use_bounded_region_lookups(self):
        import time

        raw = bytearray(FIXTURES['xtensa'].read_bytes())
        shoff = u32(raw, 32)
        shnum = u16(raw, 48)
        shdrs = bytearray(raw[shoff:shoff + shnum * 40])
        rela = first_section_of_type(raw, 4)
        original_rela = raw[u32(raw, rela + 16):u32(raw, rela + 16) + 12]

        # Put the only .text section at the very end of a large section table.
        text_header = bytes(shdrs[6 * 40:7 * 40])
        shdrs[6 * 40:7 * 40] = bytes(40)

        def expanded_image(count):
            data = bytearray(raw)
            data.extend(bytes(-len(data) % 4))
            rela_start = len(data)
            data.extend(original_rela * count)
            data.extend(bytes(-len(data) % 4))
            header_start = len(data)
            headers = bytearray(shdrs)
            put32(headers, 4 * 40 + 16, rela_start)
            put32(headers, 4 * 40 + 20, count * 12)
            headers.extend(bytes(count * 40))
            headers.extend(text_header)
            data.extend(headers)
            put32(data, 32, header_start)
            put16(data, 48, shnum + count + 1)
            return data

        durations = []
        for count in (4000, 40000):
            payload = expanded_image(count)
            started = time.perf_counter()
            self.assertEqual(self.validate('xtensa', payload), 0)
            durations.append(time.perf_counter() - started)

        # A full section-table scan for each RELA is quadratic and exceeds
        # this generous Host budget at 40k x 40k. Cached regions are linear.
        self.assertLess(durations[1], max(1.5, durations[0] * 16))

    def test_riscv_large_program_header_index(self):
        raw = bytearray(FIXTURES['riscv'].read_bytes())
        phoff = u32(raw, 28)
        phnum = u16(raw, 44)
        headers = bytearray(raw[phoff:phoff + phnum * 32])
        headers.extend(bytes(16 * 32))  # Additional non-load headers.
        raw.extend(bytes(-len(raw) % 4))
        new_phoff = len(raw)
        raw.extend(headers)
        put32(raw, 28, new_phoff)
        put16(raw, 44, phnum + 16)
        self.assertEqual(self.validate('riscv', raw), 0)

    def test_cmake_embedded_elf_alignment(self):
        # No IDF source checkout is needed: the component owns its generator.
        with tempfile.TemporaryDirectory(prefix='elf-embed-') as dirname:
            build = Path(dirname)
            odd = build / 'odd.bin'
            odd.write_bytes(b'abcde')
            executable = build / 'test_riscv.elf'
            executable.write_bytes(FIXTURES['riscv'].read_bytes())
            script = COMPONENT / 'tool/embed_aligned_binary.cmake'
            objects = []

            for idx, (payload, kind) in enumerate(((odd, 'BINARY'), (executable, 'TEXT'))):
                assembly = build / f'embedded_{idx}.S'
                obj = build / f'embedded_{idx}.o'
                subprocess.run([
                    'cmake', f'-DDATA_FILE={payload}', f'-DSOURCE_FILE={assembly}',
                    f'-DFILE_TYPE={kind}', '-P', str(script),
                ], check=True)
                subprocess.run([
                    os.environ.get('CC', 'cc'), '-c', str(assembly), '-o', str(obj),
                ], check=True)
                objects.append(obj)

            merged = build / 'embedded.o'
            subprocess.run(['ld', '-r', *(str(obj) for obj in objects),
                            '-o', str(merged)], check=True)
            nm = subprocess.check_output(['nm', '-n', str(merged)], text=True)
            symbols = {}
            for line in nm.splitlines():
                fields = line.split()
                if len(fields) == 3:
                    symbols[fields[2]] = int(fields[0], 16)

            for name in ('odd_bin', 'test_riscv_elf'):
                self.assertEqual(symbols[f'_binary_{name}_start'] % 4, 0)
            self.assertEqual(
                symbols['_binary_test_riscv_elf_end'] -
                symbols['_binary_test_riscv_elf_start'],
                len(executable.read_bytes()) + 1,  # TEXT includes a NUL byte.
            )

    def test_riscv_exported_symbols_use_indexed_segments(self):
        import time

        raw = bytearray(
            (ROOT / 'examples/elf_loader/elf_console_example/main/fs_image/riscv/lib.so')
            .read_bytes()
        )
        phoff = u32(raw, 28)
        phnum = u16(raw, 44)
        original_phdrs = bytes(raw[phoff:phoff + phnum * 32])
        symhdr = first_section_of_type(raw, 11)
        original_sym_off = u32(raw, symhdr + 16)
        original_sym_size = u32(raw, symhdr + 20)
        original_symbols = bytes(
            raw[original_sym_off:original_sym_off + original_sym_size]
        )
        function = next(
            original_symbols[index:index + 16]
            for index in range(0, len(original_symbols), 16)
            if original_symbols[index + 12] == 0x12
            and u16(original_symbols, index + 14) != 0
        )

        def expanded_image(count):
            data = bytearray(raw)
            data.extend(bytes(-len(data) % 4))
            new_phoff = len(data)
            # A valid executable PT_LOAD appears after numerous inert headers.
            data.extend(bytes(count * 32))
            data.extend(original_phdrs)
            data.extend(bytes(-len(data) % 4))
            new_symoff = len(data)
            data.extend(original_symbols)
            data.extend(function * count)
            put32(data, 28, new_phoff)
            put16(data, 44, phnum + count)
            put32(data, symhdr + 16, new_symoff)
            put32(data, symhdr + 20, original_sym_size + count * 16)
            return data

        timings = []
        for count in (1000, 22000):
            payload = expanded_image(count)
            started = time.perf_counter()
            self.assertEqual(self.validate('riscv', payload), 0)
            timings.append(time.perf_counter() - started)

        # Old behavior scanned all Program Headers for each exported function.
        # The indexed implementation keeps large ELF validation near-linear.
        self.assertLess(timings[1], max(0.5, timings[0] * 65))

    def test_defined_function_exports_stay_in_executable_memory(self):
        for arch in FIXTURES:
            image = ROOT / 'examples/elf_loader/elf_console_example/main/fs_image' / arch / 'lib.so'
            raw = bytearray(image.read_bytes())
            with self.subTest(arch=arch, case='valid library'):
                self.assertEqual(self.validate(arch, raw), 0)

            symtab = first_section_of_type(raw, 11)
            entry_base = u32(raw, symtab + 16)
            entries = u32(raw, symtab + 20) // 16
            defined = None
            for index in range(entries):
                off = entry_base + 16 * index
                if raw[off + 12] == 0x12 and u16(raw, off + 14) != 0:
                    defined = off
                    break
            self.assertIsNotNone(defined, 'Expected a defined global function')
            # A shared object has no entry point: its exported functions are the entry points.
            self.assertEqual(u32(raw, 24), 0)

            outside = bytearray(raw)
            put32(outside, defined + 4, 0xfffffff0)
            with self.subTest(arch=arch, case='outside loaded code'):
                self.assertNotEqual(self.validate(arch, outside), 0)

            bad_section = bytearray(raw)
            put16(bad_section, defined + 14, 0xfffe)
            with self.subTest(arch=arch, case='invalid defining section'):
                self.assertNotEqual(self.validate(arch, bad_section), 0)
            oversized = bytearray(raw)
            put32(oversized, defined + 8, 0xfffffff0)
            with self.subTest(arch=arch, case='function extends outside executable memory'):
                self.assertNotEqual(self.validate(arch, oversized), 0)

            # A second named .dynsym would reinitialize the export table.
            duplicate = bytearray(raw)
            duplicate[section(duplicate, 0):section(duplicate, 0) + 40] = raw[
                symtab:symtab + 40
            ]
            with self.subTest(arch=arch, case='duplicate dynamic symbol section'):
                self.assertNotEqual(self.validate(arch, duplicate), 0)

            # Undefined imported functions are allowed but must not be exported.
            undefined = bytearray(raw)
            put16(undefined, defined + 14, 0)
            with self.subTest(arch=arch, case='undefined imported function'):
                self.assertEqual(self.validate(arch, undefined), 0)

    def test_shared_objects_require_dynamic_loading(self):
        output = Path(self.build.name) / 'libvalidate_without_shared.so'
        subprocess.run([
            os.environ.get('CC', 'cc'),
            '-std=c11', '-Wall', '-Wextra', '-Werror', '-fPIC', '-shared',
            '-DCONFIG_IDF_TARGET_ARCH_XTENSA=1',
            '-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1',
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'src/esp_elf_validate.c'),
            '-o', str(output),
        ], check=True)
        lib = ctypes.CDLL(str(output))
        lib.esp_elf_validate.argtypes = (ctypes.c_void_p, ctypes.c_size_t)
        lib.esp_elf_validate.restype = ctypes.c_int
        path = ROOT / 'examples/elf_loader/elf_console_example/main/fs_image/xtensa/lib.so'
        raw = path.read_bytes()
        buf = ctypes.create_string_buffer(raw)
        self.assertNotEqual(lib.esp_elf_validate(buf, len(raw)), 0)

    def test_exec_zero_entry_is_valid_in_executable_segment(self):
        raw = bytearray(FIXTURES['riscv'].read_bytes())
        segment = first_load_segment(raw)
        self.assertEqual(u32(raw, segment + 8), 0)
        self.assertGreater(u32(raw, segment + 16), 0)
        put16(raw, 16, 2)  # ET_EXEC
        put32(raw, 24, 0)
        self.assertEqual(self.validate('riscv', raw), 0)

    def test_reject_short_and_misaligned_buffers(self):
        for arch, path in FIXTURES.items():
            data = path.read_bytes()
            with self.subTest(arch=arch):
                for length in (0, 1, 51, len(data) - 1):
                    self.assertNotEqual(self.validate(arch, data[:length]), 0)
                buf = ctypes.create_string_buffer(b'\0' + data)
                ptr = ctypes.cast(ctypes.byref(buf, 1), ctypes.c_void_p)
                self.assertNotEqual(
                    self.libs[arch].esp_elf_validate(ptr, len(data)), 0
                )

    def test_reject_corrupted_structures_and_relocations(self):
        for arch, path in FIXTURES.items():
            raw = bytearray(path.read_bytes())
            ph = first_load_segment(raw)
            sym = first_section_of_type(raw, 11)  # SHT_DYNSYM
            rela = first_section_of_type(raw, 4)  # SHT_RELA
            strtab = section(raw, u32(raw, sym + 24))
            shstr = section(raw, u16(raw, 50))
            rela_offset = u32(raw, rela + 16)
            sym_offset = u32(raw, sym + 16)
            first_load_size = u32(raw, ph + 20)

            variants = []

            def variant(label, change):
                data = bytearray(raw)
                change(data)
                variants.append((label, data))

            variant('magic', lambda d: d.__setitem__(0, 0))
            variant('class', lambda d: d.__setitem__(4, 2))
            variant('endianness', lambda d: d.__setitem__(5, 2))
            variant('ident version', lambda d: d.__setitem__(6, 0))
            variant('ELF version', lambda d: put32(d, 20, 0))
            variant('machine', lambda d: put16(d, 18, 0))
            variant('ELF type', lambda d: put16(d, 16, 1))
            variant('ehsize', lambda d: put16(d, 40, 0))
            variant('phentsize', lambda d: put16(d, 42, 0))
            variant('shentsize', lambda d: put16(d, 46, 0))
            variant('phoff', lambda d: put32(d, 28, 0xFFFFFFFC))
            variant('shoff', lambda d: put32(d, 32, 0xFFFFFFFC))
            variant('shoff alignment', lambda d: put32(d, 32, u32(d, 32) + 1))
            variant('phnum', lambda d: put16(d, 44, 0xFFFF))
            variant('shnum', lambda d: put16(d, 48, 0xFFFF))
            variant('shstrndx', lambda d: put16(d, 50, 0xFFFF))
            variant('empty shstrtab', lambda d: put32(d, shstr + 20, 0))
            variant('shstrtab out of file', lambda d: put32(d, shstr + 16, len(d) + 1))
            variant('section name index', lambda d: put32(d, section(d, 1), u32(d, shstr + 20)))
            variant('section name without NUL', lambda d: d.__setitem__(
                u32(d, shstr + 16) + u32(d, shstr + 20) - 1, ord('X')
            ))
            variant('section data out of file', lambda d: put32(
                d, section(d, 1) + 16, len(d) + 1
            ))
            variant('section size out of file', lambda d: put32(d, section(d, 1) + 20, 0xFFFFFFFF))
            variant('PT_LOAD source out of file', lambda d: put32(d, ph + 4, len(d) + 1))
            variant('PT_LOAD memsz smaller than filesz', lambda d: put32(d, ph + 20, 1))
            variant('PT_LOAD vaddr overflow', lambda d: put32(d, ph + 8, 0xFFFFFFF0))
            variant('entry outside executable memory', lambda d: put32(d, 24, 0xFFFFFFFC))
            if arch == 'riscv':
                variant('entry in executable segment BSS', lambda d: (
                    put32(d, ph + 20, first_load_size + 8),
                    put32(d, 24, u32(d, ph + 8) + first_load_size + 2),
                ))
            variant('RELA length', lambda d: put32(d, rela + 20, 11))
            variant('RELA entsize', lambda d: put32(d, rela + 36, 2))
            variant('RELA wrong symbol table link', lambda d: put32(d, rela + 24, 0))
            variant('RELA missing symbol table', lambda d: put32(d, rela + 24, 0xFFFF))
            variant('SYMTAB length', lambda d: put32(d, sym + 20, 17))
            variant('SYMTAB offset alignment', lambda d: put32(d, sym + 16, sym_offset + 1))
            variant('SYMTAB bad strtab link', lambda d: put32(d, sym + 24, 0xFFFF))
            # A fake named .dynsym must not reach the shared-object export parser.
            variant('mis-typed dynsym named section', lambda d: (
                put32(d, sym + 4, 1),  # PROGBITS rather than DYNSYM
                put32(d, rela + 4, 1),  # remove RELA link, isolate name check
                put32(d, sym + 24, 0xFFFF),
            ))
            # .dynsym comes before its linked .dynstr: validate the target first.
            variant('forward strtab offset outside image', lambda d: put32(
                d, strtab + 16, len(d) + 4
            ))
            variant('forward strtab range crosses end', lambda d: (
                put32(d, strtab + 16, len(d) - 2),
                put32(d, strtab + 20, 8),
            ))
            variant('symbol name offset', lambda d: put32(
                d, sym_offset + 16, u32(d, strtab + 20) + 1
            ))
            variant('symbol name not terminated', lambda d: d.__setitem__(
                u32(d, strtab + 16) + u32(d, strtab + 20) - 1, ord('X')
            ))
            variant('relocation symbol index', lambda d: put32(
                d, rela_offset + 4, (0xFFFF << 8) | (u32(d, rela_offset + 4) & 0xFF)
            ))
            variant('relocation address outside file map', lambda d: put32(
                d, rela_offset, 0xFFFFFFFC
            ))
            variant('relocation address in unmapped hole', lambda d: put32(
                d, rela_offset, 0x300
            ))
            variant('unaligned relocation address', lambda d: put32(
                d, rela_offset, u32(d, rela_offset) + 1
            ))
            variant('truncated relocation target', lambda d: put32(
                d, rela_offset, (u32(d, ph + 8) + first_load_size - 2) & 0xFFFFFFFF
            ))

            if arch == 'xtensa':
                # Duplicate .text changes the section selected by the real loader.
                variant('duplicate .text section', lambda d: (
                    put32(d, section(d, 7), u32(d, section(d, 6))),
                    put32(d, section(d, 7) + 8, 0x6),
                ))

            for label, payload in variants:
                with self.subTest(arch=arch, mutation=label):
                    self.assertNotEqual(self.validate(arch, payload), 0, label)


    def test_riscv_unaligned_virtual_load_base_relocation(self):
        image = bytearray(FIXTURES['riscv'].read_bytes())
        self.assertEqual(self.validate('riscv', image), 0)
        first = first_load_segment(image)
        # This still keeps the executable entry and all relocation write
        # targets inside PT_LOAD, but base-relative writes lose alignment.
        put32(image, first + 8, u32(image, first + 8) + 1)
        self.assertNotEqual(self.validate('riscv', image), 0)

    def test_riscv_relocation_base_and_bounds(self):
        output = Path(self.build.name) / 'test_riscv_relocate'
        subprocess.run([
            os.environ.get('CC', 'cc'),
            '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
            '-DCONFIG_IDF_TARGET_ARCH_RISCV=1',
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_riscv_relocate.c'),
            str(COMPONENT / 'src/arch/esp_elf_riscv.c'),
            '-o', str(output),
        ], check=True)
        subprocess.run([str(output)], check=True)


    def test_cpp_lifecycle_elf_validation(self):
        for arch in ('xtensa', 'riscv'):
            with self.subTest(arch=arch):
                sample = synthetic_lifecycle_image(arch)
                self.assertEqual(self.validate(arch, sample), 0)
                for label, section_index, field_offset, value in (
                    ('partial-pointer', 4, 20, 3),
                    ('invalid-init-entsize', 4, 36, 8),
                    ('wrong-init-type', 4, 4, 1),
                    ('misaligned-vaddr', 4, 12, 0x1045),
                    ('lifecycle-overlap-rodata', 4, 12, 0x1020),
                    ('misaligned-file-offset', 4, 16, u32(sample, section(sample, 4) + 16) + 1),
                    ('missing-alloc', 3, 8, 0),
                    ('duplicate-init', 5, 4, 14),
                ):
                    broken = bytearray(sample)
                    put32(broken, section(broken, section_index) + field_offset, value)
                    with self.subTest(arch=arch, error=label):
                        self.assertNotEqual(self.validate(arch, broken), 0)
                if arch == 'riscv':
                    for label, field_offset, value in (
                        ('outside-load', 12, 0x2000),
                        ('file-offset-mismatch', 16, u32(sample, section(sample, 3) + 16) + 4),
                    ):
                        broken = bytearray(sample)
                        put32(broken, section(broken, 3) + field_offset, value)
                        with self.subTest(arch=arch, error=label):
                            self.assertNotEqual(self.validate(arch, broken), 0)

    def test_cpp_array_checker_requires_each_pointer_to_be_relocated(self):
        import importlib.util
        script = COMPONENT / 'tests/idf_cpp_lifecycle/check_elf_arrays.py'
        spec = importlib.util.spec_from_file_location('check_elf_arrays', script)
        checker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(checker)

        image = synthetic_lifecycle_image('riscv')
        # Expand .init_array to 2 callback words and move .fini_array away.
        put32(image, section(image, 4) + 20, 8)
        put32(image, section(image, 5) + 12, 0x1050)
        fini_file = u32(image, section(image, 5) + 16)
        put32(image, section(image, 5) + 16, fini_file + 8)
        offset = (len(image) + 3) & ~3
        image.extend(bytes(offset - len(image)))
        for target in (0x1044, 0x1048, 0x1050):
            image.extend(struct.pack('<III', target, 3, 0))
        # An SHT_RELA section at index zero is accepted by this fixture
        # checker; ELF executable input validation is tested independently.
        put32(image, section(image, 0) + 4, 4)
        put32(image, section(image, 0) + 16, offset)
        put32(image, section(image, 0) + 20, 36)
        valid = Path(self.build.name) / 'array-relocations-valid.elf'
        valid.write_bytes(image)
        checker.verify_elf(valid)

        # A duplicate relocation does not initialize the second callback.
        put32(image, offset + 12, 0x1044)
        broken = Path(self.build.name) / 'array-relocations-duplicate.elf'
        broken.write_bytes(image)
        with self.assertRaises(AssertionError):
            checker.verify_elf(broken)

    def test_s2_mmu_unmaps_executable_range_before_heap_free(self):
        output = Path(self.build.name) / 'test_mmu_teardown'
        subprocess.run([
            os.environ.get('CC', 'cc'),
            '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
            '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
            '-Wno-format', '-Wno-unused-variable', '-Wno-unused-parameter',
            '-Wno-unused-but-set-variable', '-Wno-sign-compare',
            '-ffunction-sections', '-fdata-sections', HOST_GC_LINKER_FLAG,
            '-DCONFIG_IDF_TARGET_ARCH_XTENSA=1',
            '-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1',
            '-DCONFIG_ELF_LOADER_SET_MMU=1',
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_mmu_teardown.c'),
            '-o', str(output),
        ], check=True)
        subprocess.run([str(output)], check=True)

    def test_s2_mmu_executable_span_includes_literal_pool_and_page_offset(self):
        output = Path(self.build.name) / 'test_mmu_span'
        subprocess.run([
            os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_mmu_span.c'),
            '-o', str(output),
        ], check=True)
        subprocess.run([str(output)], check=True)

    def test_s2_mmu_window_requires_successful_mapping(self):
        # Compile the production S2 MMU allocator with a simulated I-bus MMU
        # register array. Fragmented pages must return -EIO without changing
        # ELF mapping fields; a window ending at the last page must work.
        output = Path(self.build.name) / 'test_s2_mmu_window'
        subprocess.run([
            os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra',
            '-Werror', '-Wno-sign-compare',
            '-I', str(COMPONENT / 'tests/host/s2_mmu_stubs'),
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_s2_mmu_window.c'),
            '-o', str(output),
        ], check=True)
        subprocess.run([str(output)], check=True)

    def test_xtensa_real_toolchain_multi_plt_relocation(self):
        # Optional on generic CI hosts; synthetic .got.loc load/relocation
        # tests always run regardless of toolchain availability.
        compiler = shutil.which('xtensa-esp32-elf-gcc')
        stripper = shutil.which('xtensa-esp32-elf-strip')
        readelf = shutil.which('xtensa-esp32-elf-readelf')
        if not all((compiler, stripper, readelf)):
            self.skipTest('ESP32 Xtensa cross-compiler/strip/readelf not installed')
        output = Path(self.build.name) / 'elf_multi_plt.so'
        subprocess.run([
            compiler, '-shared', '-fPIC', '-nostdlib', '-Wl,-e,0',
            '-mlongcalls', '-mtext-section-literals',
            str(COMPONENT / 'tests/host/fixtures/plt_multi_imports.c'),
            '-o', str(output),
        ], check=True)
        subprocess.run([
            stripper, '--strip-unneeded', '--remove-section=.comment',
            '--remove-section=.dynamic', str(output),
        ], check=True)
        sections = subprocess.run([readelf, '-W', '-S', str(output)],
                                  check=True, capture_output=True, text=True).stdout
        self.assertIn('.got.loc', sections)
        self.assertIn('.rodata', sections)
        relocations = subprocess.run([readelf, '-r', str(output)],
                                     check=True, capture_output=True, text=True).stdout
        self.assertGreaterEqual(relocations.count('R_XTENSA_JMP_SLOT'), 3)
        self.assertEqual(self.validate('xtensa', output.read_bytes()), 0)

        # The actual host-compiled loader must map and relocate the real
        # Xtensa ELF, not merely accept its metadata in the validator.
        executable = Path(self.build.name) / 'test_multi_plt_load'
        subprocess.run([
            os.environ.get('CC', 'cc'),
            '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
            '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
            '-Wno-format', '-Wno-unused-variable', '-Wno-unused-parameter',
            '-Wno-unused-but-set-variable', '-Wno-sign-compare',
            '-ffunction-sections', '-fdata-sections', HOST_GC_LINKER_FLAG,
            '-DCONFIG_IDF_TARGET_ARCH_XTENSA=1',
            '-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1',
            '-DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=1',
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_multi_plt_load.c'),
            str(COMPONENT / 'src/arch/esp_elf_xtensa.c'),
            str(COMPONENT / 'src/esp_elf_validate.c'),
            '-o', str(executable),
        ], check=True)
        subprocess.run([str(executable), str(output)], check=True)

    def test_xtensa_got_loc_elf_validation(self):
        sample = synthetic_lifecycle_image('xtensa', got_loc=True)
        self.assertEqual(self.validate('xtensa', sample), 0)
        got = section(sample, 6)
        for label, value in (
            ('overlap-text', 0x1004),
            ('misaligned-literal-pool', 0x1082),
            ('overlap-rodata', 0x1020),
            ('overlap-init-array', 0x1044),
        ):
            broken = bytearray(sample)
            put32(broken, got + 12, value)
            with self.subTest(error=label):
                self.assertNotEqual(self.validate('xtensa', broken), 0)
        broken = bytearray(sample)
        put32(broken, got + 8, 0)  # non-allocated .got.loc is invalid
        self.assertNotEqual(self.validate('xtensa', broken), 0)

    def test_build_scripts_preserve_plt_literals(self):
        cmake = (COMPONENT / 'elf_loader.cmake').read_text()
        self.assertNotIn('--remove-section=.got.loc', cmake)

    def test_cpp_elf_constructor_destructor_lifecycle(self):
        for arch in ('riscv', 'xtensa'):
            for shared_loading in (False, True):
                with self.subTest(arch=arch, shared_loading=shared_loading):
                    output = Path(self.build.name) / f'test_lifecycle_{arch}_{shared_loading}'
                    flags = ['-DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=1'] if shared_loading else []
                    if arch == 'xtensa':
                        flags.append('-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1')
                    subprocess.run([
                        os.environ.get('CC', 'cc'),
                        '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
                        '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
                        '-Wno-format', '-Wno-unused-variable', '-Wno-unused-parameter',
                        '-Wno-unused-but-set-variable', '-Wno-sign-compare',
                        '-ffunction-sections', '-fdata-sections', HOST_GC_LINKER_FLAG,
                        f'-DCONFIG_IDF_TARGET_ARCH_{arch.upper()}=1', *flags,
                        '-I', str(COMPONENT / 'tests/host/include'),
                        '-I', str(COMPONENT / 'include'),
                        str(COMPONENT / 'tests/host/test_lifecycle.c'),
                        '-o', str(output),
                    ], check=True)
                    subprocess.run([str(output)], check=True)

    def test_module_local_symbol_resolution_independent_of_dlopen(self):
        for shared_loading in (False, True):
            with self.subTest(shared_loading=shared_loading):
                output = Path(self.build.name) / f'test_local_symbol_{shared_loading}'
                flags = ['-DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=1'] if shared_loading else []
                subprocess.run([
                    os.environ.get('CC', 'cc'),
                    '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
                    '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
                    '-Wno-format', '-Wno-unused-variable', '-Wno-unused-parameter',
                    '-Wno-unused-but-set-variable', '-Wno-sign-compare',
                    '-ffunction-sections', '-fdata-sections', HOST_GC_LINKER_FLAG,
                    '-DCONFIG_IDF_TARGET_ARCH_RISCV=1', *flags,
                    '-I', str(COMPONENT / 'tests/host/include'),
                    '-I', str(COMPONENT / 'include'),
                    str(COMPONENT / 'tests/host/test_local_symbol.c'),
                    '-o', str(output),
                ], check=True)
                subprocess.run([str(output)], check=True)

    def test_segment_first_load_nonzero_base(self):
        for shared_loading in (False, True):
            with self.subTest(shared_loading=shared_loading):
                output = Path(self.build.name) / f'test_segment_load_{shared_loading}'
                flags = ['-DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=1'] if shared_loading else []
                subprocess.run([
                    os.environ.get('CC', 'cc'),
                    '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
                    '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
                    '-Wno-format', '-Wno-unused-variable', '-Wno-unused-parameter',
                    '-Wno-unused-but-set-variable', '-Wno-sign-compare',
                    '-ffunction-sections', '-fdata-sections',
                    HOST_GC_LINKER_FLAG,
                    '-DCONFIG_IDF_TARGET_ARCH_RISCV=1', *flags,
                    '-I', str(COMPONENT / 'tests/host/include'),
                    '-I', str(COMPONENT / 'include'),
                    str(COMPONENT / 'tests/host/test_segment_load.c'),
                    '-o', str(output),
                ], check=True)
                subprocess.run([str(output)], check=True)

    def test_section_entry_conditions(self):
        for shared_loading in (False, True):
            with self.subTest(shared_loading=shared_loading):
                output = Path(self.build.name) / f'test_section_load_{shared_loading}'
                flags = ['-DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=1'] if shared_loading else []
                subprocess.run([
                    os.environ.get('CC', 'cc'),
                    '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
                    '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
                    '-Wno-format', '-Wno-unused-variable', '-Wno-unused-parameter',
                    '-Wno-unused-but-set-variable', '-Wno-sign-compare',
                    '-ffunction-sections', '-fdata-sections',
                    HOST_GC_LINKER_FLAG,
                    '-DCONFIG_IDF_TARGET_ARCH_XTENSA=1',
                    '-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1', *flags,
                    '-I', str(COMPONENT / 'tests/host/include'),
                    '-I', str(COMPONENT / 'include'),
                    str(COMPONENT / 'tests/host/test_section_load.c'),
                    str(COMPONENT / 'src/arch/esp_elf_xtensa.c'),
                    '-o', str(output),
                ], check=True)
                subprocess.run([str(output)], check=True)

    def test_psram_exported_function_uses_executable_alias(self):
        output = Path(self.build.name) / 'test_psram_export_alias'
        subprocess.run([
            os.environ.get('CC', 'cc'),
            '-std=c11', '-D_GNU_SOURCE', '-Wall', '-Wextra', '-Werror',
            '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
            '-Wno-format', '-Wno-unused-variable', '-Wno-unused-parameter',
            '-Wno-unused-but-set-variable', '-Wno-sign-compare',
            '-ffunction-sections', '-fdata-sections',
            HOST_GC_LINKER_FLAG,
            '-DCONFIG_IDF_TARGET_ARCH_XTENSA=1',
            '-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1',
            '-DCONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT=1',
            '-DCONFIG_ELF_LOADER_CACHE_OFFSET=1',
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_section_load.c'),
            str(COMPONENT / 'src/arch/esp_elf_xtensa.c'),
            '-o', str(output),
        ], check=True)
        subprocess.run([str(output)], check=True)

    def test_xtensa_relocation_mapping_and_bounds(self):
        output = Path(self.build.name) / 'test_xtensa_relocate'
        subprocess.run([
            os.environ.get('CC', 'cc'),
            '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
            '-DCONFIG_IDF_TARGET_ARCH_XTENSA=1',
            '-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1',
            '-I', str(COMPONENT / 'tests/host/include'),
            '-I', str(COMPONENT / 'include'),
            str(COMPONENT / 'tests/host/test_xtensa_relocate.c'),
            str(COMPONENT / 'src/arch/esp_elf_xtensa.c'),
            '-o', str(output),
        ], check=True)
        subprocess.run([str(output)], check=True)


if __name__ == '__main__':
    unittest.main()
