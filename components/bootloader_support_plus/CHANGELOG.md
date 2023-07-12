# ChangeLog

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

