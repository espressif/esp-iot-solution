# ChangeLog

## v2.0.1 - 2026-10-10

### Bugfix:

* Log the RGB panel pointer before freeing it during deletion, fixing GCC 15 use-after-free build errors when debug logging is enabled.

## v2.0.0~1 - 2026-05-28

### Changes:

* Add esp32s31 target support

## v2.0.0 - 2025-10-29

### Changes:

* Compatible with ESP-IDF v6.0

## v1.0.0 - 2025-7-17

### Enhancements:

* Implement the driver for the NV3052 LCD controller
* Support RGB interface

