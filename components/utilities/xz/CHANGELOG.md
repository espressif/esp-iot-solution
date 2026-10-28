# ChangeLog

## v1.1.2 - 2026-10-27

### bugfix:

- On the ESP32-P4 bootloader, place the multi-call LZMA2 dictionary on the non-cacheable alias and access it with 32-bit read-modify-write. Byte access on that alias is incorrect, and a cached dictionary is dropped when ROM flash helpers disable L2. `XZ_SINGLE` keeps byte access: its dictionary is the caller's output buffer, which is not 4-byte aligned or sized.
- Keep a failed dictionary allocation as NULL through the non-cacheable alias map so create/reset still return `XZ_MEM_ERROR` instead of using `0x40000000`.
- Null-check before `xz_dec_lzma2_end`.

## v1.1.1 - 2026-9-18

### bugfix:

- Change `tristate` to `bool` in the vendored `xz-embedded` Kconfig, since ESP-IDF has no loadable module support

## v1.1.0 - 2025-3-25

### Features:

- Make the xz_embedded library's multi-call APIs public

## v1.0.1 - 2025-2-20

### bugfix:

- Fix incorrect prototype of the `error` callback of `xz_decompress`

## v1.0.0 - 2023-2-10

First release version.

- Support xz_decompress
