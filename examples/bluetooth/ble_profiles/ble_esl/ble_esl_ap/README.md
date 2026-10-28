| Supported Targets | ESP32-H4 | ESP32-C6 | ESP32-S31 |
| ----------------- | -------- | -------- | --------- |

# BLE ESL Access Point (profile example)

Default build is **mode 2**: AP + existing Android App over NUS JSONL. The App is **not** changed — command names, events, NUS UUIDs, `max_chunk:192`, `max_line:1024`, `protocol: esl-jsonl-v1`, and `device_name: ESP-ESL-AP` stay byte-compatible with the product firmware.

This example sits on iot-solution `esp_ble_eslp_*` + `ble_conn_mgr` (not ESP-IDF `ble_esl_*`). Pair with [`ble_esl_tag`](../ble_esl_tag/README.md).

The original product tree `examples/bluetooth/ble_esl/` is unchanged and remains in the repo.

## Requirements

- ESP-IDF **v6.1 or newer**
- PAwR-capable chip: **ESP32-H4**, **ESP32-C6**, or **ESP32-S31** (H4/S31 are preview: `idf.py --preview`)
- One Tag board running `ble_esl_tag` (SSD1680 e-paper on H4/C6)
- Existing Android ESL App (no APK change)

## How to use

```bash
cd examples/bluetooth/ble_profiles/ble_esl/ble_esl_ap
idf.py set-target esp32c6    # or: idf.py --preview set-target esp32h4
idf.py erase-flash
idf.py build flash monitor
```

First bring-up: erase flash on AP and Tag so old NVS/bonds do not leak.

### Configure

`sdkconfig.defaults` already selects:

| Option | Value |
|--------|--------|
| AP operation mode | AP + phone app (`ESL_AP_MODE_WITH_APP`) |
| PAwR subevents | 4 |
| Ext-adv instances | 2 (PAwR instance 0, NUS instance 1) |
| NimBLE dynamic GATT | on (`CONFIG_BT_NIMBLE_DYNAMIC_SERVICE`; required so NUS can be registered after the host starts) |
| Max BLE connections | 3 (phone + Tag) |

## Expected App flow

1. Phone connects to **ESP-ESL-AP** (NUS)
2. `provision_tag` → Tag associates and PAST-syncs
3. `upload_image_*` (4736-byte 1bpp SSD1680 frame) → `transfer_image_to_tag`
4. `display_image` / `refresh_display`

JSONL image upload over NUS is a demo transport (`max_chunk:192`).

## Testing

There is no host-side unit-test target for this example yet. Validate on hardware with a PAwR-capable AP, a `ble_esl_tag` board, and the Android ESL App (provision → image upload/transfer → display). Cover recovery after AP reboot and repeated `transfer_image_to_tag` while the Tag is synchronized.
