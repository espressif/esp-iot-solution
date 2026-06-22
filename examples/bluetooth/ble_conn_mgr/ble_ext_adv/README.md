| Supported Targets | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- | -------- | -------- | -------- |

# BLE Extended Advertising 1 KiB Example (Advertiser)

(See the README.md file in the upper level `examples` directory for more information about examples.)

This example sends a **non-connectable, non-scannable** extended advertising PDU of **1000 octets** on **primary 1M / secondary 2M** PHY, with **Advertising SID = 2**. It pairs with `ble_ext_scan`, which reassembles the same payload and checks FNV-1a.

This is **not** periodic advertising; for periodic adv/sync demos use `ble_periodic_adv` and `ble_periodic_sync`.

## How to use

```bash
idf.py set-target <chip_name>
idf.py build flash monitor
```

Compare the log line **FNV-1a** with the scanner example on the other board.
