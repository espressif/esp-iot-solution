## v0.1.1 - 2026.10.28

Bug Fixes:
- `esp_ble_hrp_get_location()` now passes a caller-owned buffer to `esp_ble_conn_read()` instead of borrowing the connection-manager GATT cache pointer.

## v0.1.0

This is the first release version for Heart Rate Profile component in Espressif Component Registry, more detailed descriptions about the project, please refer to [User_Guide](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/bluetooth/ble_profiles.html).

Features:
- HRP: Support Heart Rate Profile.
