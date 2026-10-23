## v0.1.1 - 2026.10.28

Bug Fixes:
- `esp_ble_anp_get_new_alert()` and `esp_ble_anp_get_unr_alert()` now pass a caller-owned buffer to `esp_ble_conn_read()` instead of borrowing the connection-manager GATT cache pointer.

## v0.1.0

This is the first release version for Alert Notification Profile component in Espressif Component Registry, more detailed descriptions about the project, please refer to [User_Guide](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/bluetooth/ble_profiles.html).

Features:
- ANP: Support Alert Notification Profile.
