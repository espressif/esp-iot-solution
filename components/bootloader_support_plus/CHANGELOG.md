# ChangeLog

## v0.8.0 - 2026-11-11

- The ddelta v3 header CRC covers the whole base firmware file (`base_len` plus `base_crc32`). A mismatch is rejected before the other app slot is erased. Patches packed by cmake_utilities before 1.4.1 still record a 4KB length and only check that prefix. Require `cmake_utilities` >=1.4.1.
- While checking that CRC, the bootloader feeds the watchdog so a large base does not reset the chip.
- A failed destination erase or a failed patch apply now restores otadata to the previously running slot. If that restore write fails, the first sector of the patch partition is erased so the next boot does not erase the destination again. The erase starts at the partition offset. A type-2 image (--add_app_header) stores the custom header 0x120 bytes later, which is not sector-aligned, so erasing from that address is rejected.
- Add `SKIP_VALIDATE_FULL_BASE_IMAGE` (default y). Set it to n, with ddelta enabled, to run `esp_image_verify()` on the base app before applying the patch. The file CRC still runs. With secure boot this is an extra signature check.
- Add `SKIP_VALIDATE_FULL_NEW_IMAGE` (default y). Set it to n to run `esp_image_verify()` on the rebuilt app before switching otadata. A failed check restores the previously running slot. Flash encryption and secure boot make this an extra signature check.

## v0.7.0 - 2026-11-04

- Apply an xz-compressed ddelta patch in the bootloader. The patch stays in the last app partition. The new image is written to the other app slot, after checking the base-firmware prefix CRC stored in the v3 header. Enable `BOOTLOADER_DIFF_DDELTA`. Pure compressed OTA is unchanged.
- Require `cmake_utilities` >=1.4.0. `gen_diff_ota` and `esp_ddelta` in that release pack the v3 ddelta image this bootloader applies.
- Legacy esp-bootloader-plus v2 cannot be enabled together with ddelta. That mode still uses the v2 header and does not apply a differential patch.
- Reject a ddelta header whose `new_file_size` exceeds the destination partition. `bootloader_storage_flash_input` now allows a write that fills the partition exactly and returns an error on overflow instead of discarding the tail and reporting success.
- On a failed package check, restore otadata to the previously running app slot from OTA state instead of always falling back to `ota_0`.
- After xz finishes, require the ddelta zero terminator and `bytes_written == new_file_size`. A truncated patch that still forms a valid xz stream no longer updates otadata.

## v0.6.0 - 2026-10-28

- Add ESP32-P4 support for chip revision >= 3 on IDF v5.5.5 and newer. P4 does not use `linker.lf` (that region overlaps `iram_loader_seg`). IDF 5.5.x copies `bootloader.rev3.ld` into the build directory and injects the same `.iram_loader.text` rules. `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` is a configure error.
- On targets where IRAM and DRAM share an address range, keep the bootloader XZ bump allocator below `bootloader_iram_seg_start` so the dictionary cannot overwrite `.iram_loader.text`.
- When flash encryption is disabled, read the compressed image and RMW destination through the ROM SPI path instead of the shared cache-mapped MMU window, avoiding stale reads during decompress-on-P4.
- Keep decoder state and I/O buffers on the cacheable bootloader heap. On ESP32-P4 the LZMA dictionary itself is remapped to the non-cacheable alias.
- On ESP32-P4, write back the L1 data cache before bootloader flash reads and writes. ROM flash helpers disable L2, and a dirty L1 line written back into a disabled L2 is lost.
- On ESP32-P4 only, align the bump heap to the 64-byte L1 line and take the LZMA dictionary from the top of that heap (with a gap below loader IRAM). Other chips keep an upward bump so a top-down dictionary cannot overwrite the DRAM alias of `iram_loader_seg`.
- Depend on `xz` `>=1.1.2,<2.0.0`. The ESP32-P4 bootloader dictionary fix is in that release.
- Add ESP32-S31 support on IDF v6.1 and newer. S31 uses the IDF 6.x `.iram_loader.text` injection and the unified DIRAM heap ceiling. It does not use the ESP32-P4 non-cacheable dictionary path. On IDF 6.1, S31 is a preview target (`idf.py --preview set-target esp32s31`); CI builds it with an explicit `-t esp32s31` job because `-t all` omits preview targets.

## v0.5.0 - 2026-10-23

### Fix:

- On IDF v6.0 and newer, inject plus/XZ archive rules into `.iram_loader.text` of the generated bootloader linker script (`bootloader.ld` on 6.0/6.0.1, `bootloader.rev3.ld` for ESP32-P4 rev >= 3, `bootloader.sections.ld` from 6.0.2) so the decompressor is placed in `iram_loader_seg` regardless of `-T` order. Other chips on IDF versions before v6.0 still use `linker.lf`.

## v0.4.2 - 2026-08-24

### Fix:

- Replace deprecated `esp_flash_encryption_enabled()` with `esp_efuse_is_flash_encryption_enabled()` on IDF v6.0.1 and newer. IDF v6.0.0 still provides only `esp_flash_encryption_enabled()`.

## v0.4.1 - 2026-06-10

### Fix:

- Support the new CMake build system (cmakev2) by removing the unsupported BUILD_COMPONENTS build property usage.
- Add esp_hal_wdt dependency for IDF v6.0 and newer, where the watchdog HAL was moved out of the hal component.

## v0.4.0 - 2025-09-08

### Improve:

- Add ESP32-C61 and ESP32-S3 support.

## v0.3.9 - 2025-06-17

### Improve:

- Add ESP32-C5 support.

## v0.3.8 - 2025-04-02

### Fix:

- Fix symbols that cannot be displayed properly in readme.

## v0.3.7 - 2025-02-28

### Improve:

- Update the version of dependent cmake_utilities to *

## v0.2.7 - 2025-2-26

### Fix:

* Fix the issue of a `-Wincompatible-pointer-types` compilation error.

## v0.2.6 - 2024-07-01

### Feature:

* Add support to boot into a standard firmware (non-compressed) by checking the image header even if it is at last ota partition (eg. ota_1).

## v0.2.5 - 2024-4-30

### Fix:

* Fix undeclared wdt error when compiling esp32h2.

## v0.2.4 - 2024-4-11

### Fix:

* Fix flash encryption could not be enabled in the bootloader.

## v0.2.3 - 2023-12-14

### Fix:

* Feed watchdog when erase OTA slot.

## v0.2.2 - 2023-12-12

### Fix:

* Change wrap to be triggered only when the CONFIG_BOOTLOADER_COMPRESSED_ENABLED is enabled.

## v0.2.1 - 2023-04-23

### Fix:

* Change CONFIG_BOOTLOADER_COMPRESSED_ENABLED to disable by default.

## v0.2.0 - 2023-04-20

### Feature:

* Add support for compressed OTA v2 which is supported in the deprecated esp-bootloader-plus. However, it is recommended to use bootloader_support_plus as much as possible.

## v0.1.2 - 2023-04-17

### Feature:

* Support ESP32-H2 compressed OTA on esp-idf/v5.1.

## v0.1.2 - 2023-04-17

### Feature:

* Support ESP32-C6 and ESP32 compressed OTA on esp-idf/v5.1.

## v0.1.1 - 2023-04-06

### Feature:

* Support ESP32 compressed OTA on esp-idf/v5.0.

## v0.1.0 - 2023-03-02

### Feature:

* Initial version
  * Support to update the firmware in the bootloader stage by decompressing the compressed OTA firmware
