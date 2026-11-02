# ChangeLog

## Unreleased

* Add streaming ELF relocation via `elf_source_t` and `esp_elf_relocate_source()`,
  so ELF images larger than available DRAM can be checked and relocated without
  loading the whole file into memory (e.g. apps streamed from an SD card).
  `esp_elf_relocate()` is unchanged and now shares the same relocation core.
* Copy Xtensa internal executable IRAM with aligned 32-bit stores (including
  partial .text and .got.loc words), avoiding S3 LoadStoreError on boards
  without PSRAM (#686).
* Expose the executable runtime base and log it for modules loaded via dlopen,
  allowing offline symbolization without access to private module structs (#561).
* Detach ESP32-S2 executable MMU mappings before releasing PSRAM (#686).
* Validate C++ callback-array types and entry sizes, and verify that each
  constructor/destructor slot has a distinct relocation (#796).
* Correct local and unnamed section relocations and handle unresolved weak
  symbols without letting host exports preempt module-local definitions (#789).
* Optionally generate separate unstripped ELF / shared-object debug companions,
  log executable load bases and decode crash PCs via host addr2line (#561).
* Add named C/C++ host exports and explicitly typed C++ overload exports so
  ELF mangled names can be registered without C-wrapper functions (#560).
* Export the public libc `vprintf` entry by default so ELF applications using
  ESP-IDF logging can resolve their standard formatting dependency (#497).
* Resolve module-defined symbols even with `ELF_DYNAMIC_LOAD_SHARED_OBJECT=n` (#789).
* Preserve and map Xtensa `.got.loc` PLT literal pools in executable memory at
  PC-relative offsets; extend relevant PSRAM cache and S2 MMU mappings (#686).
* Execute C++ `.preinit_array` / `.init_array` or legacy `.ctors` on the first
  `esp_elf_request()`, and finalization arrays at `esp_elf_deinit()` (#796).
* Validate callback arrays, reject duplicate/inconsistent sections, and add
  host regression tests plus a freestanding C++ build fixture.

## v2.0.0 - 2026-10-14

* Validate ELF headers, file ranges, section names, symbol tables, and relocation
  write targets before loading both RISC-V and Xtensa images.
* Validate forward-linked string table bounds and exported function addresses;
  enforce full-width Xtensa relocation targets and aligned embedded ELF buffers.
* Generate aligned ELF embedding assembly on released ESP-IDF versions that do
  not support the ALIGN option in target_add_binary_data().
* Pre-index the loaded relocation regions to avoid quadratic validation time
  for ELF files containing many sections, segments, and relocation entries.
* Reuse the indexed executable PT_LOAD ranges for exported-function checks
  instead of scanning every Program Header for each dynamic symbol.
* Avoid repeated scans of long symbol names by validating string-table
  terminators before constant-time name-offset checks.
* Accept entry-less shared libraries only when dynamic loading is enabled,
  without preventing ET_EXEC images from using a valid zero virtual entry point
  or allowing entry-less libraries to be invoked as executable applications.
* Require uniquely named, correctly typed dynamic symbol tables before parsing
  shared-object exports.
* Remap exported shared-library function addresses to executable aliases when
  code is loaded through a mirrored data/instruction bus (e.g. ESP32-S3 PSRAM).
* Free dynamically allocated symbol tables after an export-name allocation
  failure, including failures before the first name is populated.
* Bound locally mapped relocation symbols to loaded ELF memory.
* Reject RISC-V relocation targets whose word-aligned virtual offsets map
  to misaligned allocated addresses, before performing any 32-bit write.
* Reject ELF files whose sizes exceed loader allocation or read limits.
* Require executable entry points to refer to file-backed code rather than
  uninitialized PT_LOAD memory.
* Fix the first PT_LOAD base address and RISC-V relocation offset calculation;
  propagate unsupported architecture relocation errors.
* Subtract ELF virtual load bases before forming in-memory pointers to loaded
  segments or exported functions, including ELF images with high virtual bases.
* **API change:** `esp_elf_relocate()` now requires the ELF buffer length as its
  third argument. Applications must update existing calls.

## v1.3.5 - 2026-10-10

* Added support for ESP32-C5
* Reject SRAM executable ELF allocations when `CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT` is enabled (C5/C6/C61/P4/S31, etc.); document PMP / PSRAM requirements

## v1.3.4 - 2026-09-29

* Fixed ESP32-S3 PSRAM ELF relocation deadlock under multicore load by using ranged thread-safe cache synchronization
* Fixed alignment of ELF data sections in allocated memory

## v1.3.3 - 2026-07-28

* Fixed intermittent crash during long-term ELF execution from PSRAM on ESP32-S31 (use full cache flush instead of ranged API)

## v1.3.2 - 2026-07-09

* Added support for ESP32-S31
* Fixed ELF execution from PSRAM on ESP32-S31 (enable IBUS1 instruction bus)

## v1.3.1 - 2026-02-10

* Added support for overriding symbol resolution with `elf_set_symbol_resolver()`

## v1.3.0 - 2026-01-08

* Added support for generating a shared library
* Added support for dynamically loading shared objects

## v1.2.0 - 2026-01-05

* Support compiling ELF application into main binary firmware

## v1.1.1 - 2025-06-26

* Added support for ESP32-C61

## v1.1.0 - 2025-05-06

* Added fast build for ELF application
* Added a script to generate the symbol table for the ELF APP:
  * Supports generating symbols table based on ELF file
  * Supports generating symbols table based on static libraries

## v1.0.0 - 2024-12-09

* Added support for the following RISC-V chips: ESP32-P4 and ESP32-C6
* Added support for linking other components to ELF file
* Fixed the issue of getting wrong symbol type

## v0.1.0 - 2023-08-14

* Add basic ELF loader component
