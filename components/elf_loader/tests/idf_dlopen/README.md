# ESP32-S3 dynamic-loader execution regression (#776)

This is an on-device regression for the original
[Issue #776](https://github.com/espressif/esp-iot-solution/issues/776) and
[upstream PR #788](https://github.com/espressif/esp-iot-solution/pull/788).
Unlike a direct `esp_elf_relocate()` test, it writes the repository's real
`build_shared_library_example` `lib.so` into SPIFFS and executes the complete
`dlopen("lib.so")` → `dlsym("fibonacci")` → `fibonacci(10)` → `dlclose()` path.

Each Unity test runs three load/call/unload cycles, requires the result to be
55, verifies that `dlsym()` returns an executable pointer, and checks that a
missing export returns an error rather than a spurious callable address.

CI discovers two S3 firmware configurations (the base sdkconfig.defaults
and the additional sdkconfig.ci.psram):

- `defaults`: internal executable RAM on a generic ESP32-S3 board (API smoke test)
- `psram`: Octal PSRAM on an `esp-box-3` board. `CONFIG_ELF_LOADER_LOAD_PSRAM=y`
  exercises the original defect where a D-bus pointer was not callable on the
  instruction bus. The ESP-BOX-3 USB console does not reliably accept Unity
  menu input, so this variant auto-runs all Unity cases once and prints a
  repeatable ELF_DLOPEN_PSRAM=PASS result only when Unity reports zero
  failures. A device without PSRAM cannot establish that the bug is
  resolved, even if the other configuration passes.

The shared-object fixture is rebuilt for the target as part of the firmware
build. The custom partition table reserves a SPIFFS partition, and the test
uses the real ESP-IDF VFS file API rather than mocked reads. Device results
must come from the matching S3 board and build configuration.
