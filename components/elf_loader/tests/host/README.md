# ELF loader host security regression tests

Run from the repository root:

```sh
python3 -m unittest discover -s components/elf_loader/tests/host -p 'test_elf_validate.py' -v
```

The suite builds the ELF structure validator with the host C compiler for both
RISC-V (segment loader) and Xtensa (section loader). It validates the ELF
fixtures shipped in this repository and rejects malformed ELF headers, offsets,
section names, linked string-table boundaries, dynamic symbol exports and
relocation destinations. It also compiles and executes architecture relocation
range checks, RISC-V `RELATIVE` relocation values and real segment/section
loader tests. The tests cover multiple PT_LOAD segments beginning at a nonzero
base and confirm that zero-address ET_EXEC entry points remain valid, while
entry-less ET_DYN libraries are only recognized when dynamic loading is enabled.
The suite also verifies bounded relocation, exported-function lookup and
symbol-name validation with large ELF tables, and checks that the component's
CMake generator preserves aligned embedded ELF symbols even when preceded by
an odd-sized object. It covers cleanup after a partially populated dynamic
symbol table allocation fails.
The PSRAM export case additionally checks that dlsym() function addresses use
the executable alias rather than the data-bus alias.

Run with Python 3, a C compiler, CMake, and binutils (ld and nm). The
GitLab CI check_elf_loader_host job executes the same suite. The ELF loader
example build jobs also run check_embedded_alignment.py to check the actual
_binary_*_start addresses in every linked firmware ELF, across IDF versions
and target configurations.

For an optional sanitizer-backed mutation smoke test, compile the standalone
`fuzz_elf_validate.c` harness together with `src/esp_elf_validate.c` using
`-fsanitize=address,undefined -fno-omit-frame-pointer` and the architecture
macros used by the Python suite. It tests 4096 reproducible byte mutations
per supplied ELF fixture, without executing embedded code.

These tests do not execute embedded application code or substitute for on-chip
execution testing.
