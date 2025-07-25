# ChangeLog

## v0.1.0 - 2023-01-04

### Feature:

* BLE-OTA:
  * The initial version.

## v0.1.1 - 2023-01-17

### Enhancements:
* BLE-OTA:
  * Deleted subscribe command support, no longer needed for protocomm.

  * Added finish command support to indicated end of OTA.

## v0.1.2 - 2023-01-18

* Fixed warnings

## v0.1.3 - 2023-02-09

* BLE-OTA:
  * Added support for IDF release v4.4
  * Fixed github ble_ota link in idf_component.yml

## v0.1.4 - 2023-02-13

* BLE-OTA:
  * Added support for bluedroid BLE 5.0 features.

## v0.1.5 - 2023-02-15

* BLE-OTA:
  * Added support for pre-encrypted OTA.

## v0.1.6 - 2023-02-17

* BLE-OTA:
  * Added support for nimble extended advertising.

## v0.1.7 - 2023-03-09

* BLE-OTA:
  * Use cu_pkg_define_version to define the version of this component.

## v0.1.8 - 2023-03-31

* BLE-OTA:
  * Fixed errors for release v4.4

## v0.1.9 - 2023-10-16

* BLE-OTA:
  * Add support for ESP32H2

## v0.1.10 - 2023-11-23

* Fix possible cmake_utilities dependency issue

## v0.1.11 - 2024-01-02

* BLE-OTA:
  * Fixed ble ota packet index issue.

## v0.1.12 - 2024-02-19

* BLE-OTA:
  * Add support for ESP32C2 and ESP32C6.

## v0.1.13 - 2024-10-21

* BLE-OTA:
  * Add support for Delta OTA.

## v0.1.14 - 2024-11-21
* BLE-OTA:
  * Add support for enabling BLE 5.0 for NimBLE

## v0.1.15 - 2025-07-29
* BLE-OTA:
  * Fixed BLE OTA data corruption issue caused by incomplete NimBLE's mbufs reading [#502](https://github.com/espressif/esp-iot-solution/pull/502)