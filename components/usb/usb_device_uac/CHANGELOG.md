# ChangeLog

## v1.3.1 (2026-5-25)

* Fix TinyUSB RHPort high-speed configuration for ESP32-S31: use RHPORT0 instead of RHPORT1 (RHPORT1 is only for ESP32-P4).

## v1.3.0 (2026-5-15)

* Add configurable bit resolution: 16-bit, 24-in-24 (packed), 24-in-32, and 32-bit.
* Support ESP32-S31 and ESP32-H4

## v1.2.3 (2026-3-5)

* Fix Windows multi-channel UAC enumeration (Code 10) by changing EP size padding in `tusb_config_uac.h` from fixed `+4` to frame-aligned `+FRAME_SZ`.


## v1.2.2 (2025-9-23)

* Remove usb component dependency for idf 6.0 and higher

## v1.2.1 (2025-8-4)

* Fix build issue and audio path for MIC-only configuration.

## v1.2.0 (2025-3-31)

* Supports integration into other TinyUSB projects.

## v1.1.0 (2025-1-15)

* Use espressif/tinyusb: 0.17.2

## v1.0.0 (2024-11-27)

* Release the official version.

## v0.2.0 (2024-07-31)

* Support USB High speed
* Support ESP32-P4

## v0.1.1 (2024-06-13)

* Fix task core affinity issue

## v0.1.0 Init version

* Support set num of channel about MIC and SPK
* Support fifo_count feedback
