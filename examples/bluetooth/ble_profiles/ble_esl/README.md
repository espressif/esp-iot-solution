# BLE ESL profile examples (mode 2)

Product features from `examples/bluetooth/ble_esl` (mode 2 only) running on iot-solution `esp_ble_eslp_*` + `ble_conn_mgr`.

| Project | Role |
|---------|------|
| [`ble_esl_ap`](ble_esl_ap/README.md) | Access Point + NUS JSONL gateway for the existing Android App |
| [`ble_esl_tag`](ble_esl_tag/README.md) | Tag + SSD1680 e-paper |

The Android App is unchanged. Keep the product example tree; this is a backport, not a replacement.

Bring-up: erase-flash both boards, flash AP then Tag (H4 or C6), provision from the App, upload a 4736-byte 1bpp frame, display/refresh.
