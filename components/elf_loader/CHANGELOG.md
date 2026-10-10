# ChangeLog

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
