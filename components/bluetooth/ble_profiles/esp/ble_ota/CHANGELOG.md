## v0.1.18 - 2026.09.09

Bugfix:
- BLE-OTA: Limit Bluedroid RECV_FW characteristic to `ESP_GATT_MAX_ATTR_LEN`

## v0.1.17 - 2026.08.25

Bugfix:
- BLE-OTA: Use espressif/cjson on ESP-IDF v6.0 and later; keep the built-in json component on older IDF versions.

## v0.1.16 - 2026.04.15

Bugfix:
- BLE-OTA: Fixed CONFIG_PRE_ENC_OTA on NimBLE to decrypt after sector/packet checks, preventing AES-GCM breakage when packets were lost or sectors retried.

## v0.1.15 - 2025.07.29

Bugfix:
- BLE-OTA: Fixed BLE OTA data corruption issue caused by incomplete NimBLE's mbufs reading [#502](https://github.com/espressif/esp-iot-solution/pull/502)

## v0.1.14 - 2024.11.21

Features:
- BLE-OTA: Add support for enabling BLE 5.0 for NimBLE

## v0.1.13 - 2024.10.21

Features:
- BLE-OTA: Add support for Delta OTA.

## v0.1.12 - 2024.02.19

Features:
- BLE-OTA: Add support for ESP32C2 and ESP32C6.

## v0.1.11 - 2024.01.02

Bugfix:
- BLE-OTA: Fixed ble ota packet index issue.

## v0.1.10 - 2023.11.23

Bugfix:
- Fix possible cmake_utilities dependency issue

## v0.1.9 - 2023.10.16

Features:
- BLE-OTA: Add support for ESP32H2

## v0.1.8 - 2023.03.31

Bugfix:
- BLE-OTA: Fixed errors for release v4.4

## v0.1.7 - 2023.03.09

Features:
- BLE-OTA: Use cu_pkg_define_version to define the version of this component.

## v0.1.6 - 2023.02.17

Features:
- BLE-OTA: Added support for nimble extended advertising.

## v0.1.5 - 2023.02.15

Features:
- BLE-OTA: Added support for pre-encrypted OTA.

## v0.1.4 - 2023.02.13

Features:
- BLE-OTA: Added support for bluedroid BLE 5.0 features.

## v0.1.3 - 2023.02.09

Features:
- BLE-OTA: Added support for IDF release v4.4

Bugfix:
- BLE-OTA: Fixed github ble_ota link in idf_component.yml

## v0.1.2 - 2023.01.18

Bugfix:
- Fixed warnings

## v0.1.1 - 2023.01.17

Features:
- BLE-OTA: Deleted subscribe command support, no longer needed for protocomm.
- BLE-OTA: Added finish command support to indicated end of OTA.

## v0.1.0 - 2023.01.04

Features:
- BLE-OTA: The initial version.
