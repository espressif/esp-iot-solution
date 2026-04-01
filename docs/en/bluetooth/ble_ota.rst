BLE OTA Profile
===============
:link_to_translation:`zh_CN:[中文]`

The BLE OTA Profile provides sector-based firmware transfer over BLE on top of ``esp_ble_conn_mgr``.
It uses the OTA service UUID ``0x8018`` and parses START/STOP commands plus firmware packets from the host.

The profile validates:

- command packet CRC16
- sector index and packet sequence continuity
- sector CRC16 before delivering data to application callback

Examples
--------------

:example:`bluetooth/ble_profiles/ble_ota`.

API Reference
-----------------

.. include-build-file:: inc/esp_ble_ota_raw.inc