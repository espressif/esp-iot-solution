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

Issue regressions additionally verify module-defined weak-symbol relocation
with `CONFIG_ELF_DYNAMIC_LOAD_SHARED_OBJECT` both on and off (#789); Xtensa
`.got.loc` allocation and PLT literal relocation without remapping intervening
`.rodata` (#686), including an optional test which cross-builds a real
four-import Xtensa shared library, validates it and runs the actual ELF loader
and relocation loop without executing target code; and C++ callback ordering,
legacy `.ctors`/`.dtors`, one-time initialization, and validation of malformed
callback arrays (#796). These checks run with the Xtensa and RISC-V host
configurations. The host Python runner selects the appropriate GNU or macOS
linker option for dead-section removal. ESP32-S2 MMU span arithmetic is also
tested for unaligned allocation bases and multi-page executable pools.

For ESP-IDF cross-compilation, build the freestanding ELF fixture in
`components/elf_loader/tests/idf_cpp_lifecycle` with `idf.py set-target
<target> build`; then inspect the resulting `elf_cpp_lifecycle.app.elf` for
allocated `.init_array` / `.fini_array` sections and any required relocations.
The module provides local `__dso_handle`/`__cxa_atexit` hooks, so its global
object can link without adding C++ runtime symbols to the host. Check the
compiled module's callback sections and relocation entries using:

```sh
python3 components/elf_loader/tests/idf_cpp_lifecycle/check_elf_arrays.py \
    /path/to/build/elf_cpp_lifecycle.app.elf
```

Hardware verification of actual constructor/destructor execution is still required.

Additional regression coverage for ELF issues:

- #497: compile and resolve the actual default `vprintf`/`printf`/`vfprintf`
  table entries, including a negative lookup for private IDF functions.
- #560: compile and execute the new named/overloaded C++ export macros,
  verify the older C macro remains compatible, and (when cross compilers
  are installed) compare registered names with real Xtensa/RISC-V mangled
  names in their object files.
- #561: test runtime-to-ELF PC conversion for Xtensa and RISC-V, reject
  addresses outside executable sections, and verify addr2line invocation.
  The tool and `ELF_LOADER_KEEP_DEBUG_ELF=ON` CMake path are additionally
  integration-tested with actual IDF-generated `.app.elf.debug` and
  `.so.debug` binaries. `readelf -SW` should show `.debug_line` and `.symtab`
  only in the debug companion; `addr2line` must resolve to the test source.
  Verify `ELF_LOADER_KEEP_DEBUG_ELF=OFF` in a clean build directory to ensure
  no additional debug file is shipped by default.

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

CI runs the 34-case Host suite in `check_elf_loader_host` and a separate
`check_elf_loader_ubsan` job that mutates both checked-in Xtensa and RISC-V
ELF images 4096 times with UBSan instrumentation and abort-on-error enabled.

`components/elf_loader/tests/idf_runtime` is a separate flashable Unity test
that embeds the actual `idf_cpp_lifecycle` ELF. The ESP32-S3 and ESP32-P4
`components_test_elf_loader_runtime` jobs run it on physical boards using
`pytest-embedded`, checking constructor ordering, one-time initialization, and
C++ destructor execution at unload. No hardware-dependent result is inferred
from either the Host suite or a successful firmware build.

The multi-import Xtensa relocation remains covered by the real-toolchain Host
test. The simulated ESP32-S2 MMU-register test compiles the real S2 allocator
and verifies that fragmented I-bus pages return -EIO with unchanged mapping
state, that the final legal two-page window maps and unmaps safely, that an
unaligned allocation requires the extra page, and that invalid page spans do
not disable caches. ESP32-S2 PSRAM/MMU hardware validation still requires an
S2 board and is not claimed by these Host tests.
