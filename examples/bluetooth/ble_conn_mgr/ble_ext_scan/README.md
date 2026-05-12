| Supported Targets | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- | -------- | -------- | -------- |

# BLE Extended Advertising 1 KiB Example (Scanner)

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example performs **extended passive scanning** on **1M / 2M / Coded** PHYs, reassembles extended advertising data for **Advertising SID = 2**, and verifies the **1000-byte** payload and **FNV-1a** against the same pattern as `ble_ext_adv`.

Flash `ble_ext_adv` on another board and place both devices in range.

## How to use

```bash
idf.py set-target <chip_name>
idf.py build flash monitor
```

The **FNV-1a** line should match the advertiser log.
