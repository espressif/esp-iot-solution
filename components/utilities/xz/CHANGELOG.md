# ChangeLog

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
