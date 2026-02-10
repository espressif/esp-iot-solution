# ChangeLog

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
