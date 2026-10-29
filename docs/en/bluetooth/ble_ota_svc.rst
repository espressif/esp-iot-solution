BLE OTA Service
==============================
:link_to_translation:`zh_CN:[中文]`

BLE OTA Service defines GATT service ``0x8018`` and the following characteristics:

- ``0x8020`` (RECV_FW): firmware data and firmware ACK notifications
- ``0x8021`` (PROGRESS_BAR): progress value (read and notify)
- ``0x8022`` (COMMAND): OTA command and command ACK notifications
- ``0x8023`` (CUSTOMER): vendor-defined payload channel

This service is transport-level and can be reused by different OTA profile logic.
When used with ``ble_ota_raw``, command and firmware payload parsing is handled by that profile.

Examples
--------------

:example:`bluetooth/ble_profiles/ble_ota`.

API Reference
-----------------

.. include-build-file:: inc/esp_ble_ota_svc.inc
