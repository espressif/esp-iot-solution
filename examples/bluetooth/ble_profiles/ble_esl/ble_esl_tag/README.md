| Supported Targets | ESP32-H4 | ESP32-C6 | ESP32-S31 |
| ----------------- | -------- | -------- | --------- |

# BLE ESL Tag (profile example)

Default build on **ESP32-C6** and **ESP32-H4** is **mode 2**: SSD1680 e-paper, LED backend **none**, association NVS persisted. ESP32-S31 keeps the mock display backend (no e-paper pin preset). Pair with [`ble_esl_ap`](../ble_esl_ap/README.md) and the existing Android App.

Stack: iot-solution `esp_ble_eslp_*` + `ble_conn_mgr`.

## Requirements

- ESP-IDF **v6.1 or newer**
- **ESP32-H4** or **ESP32-C6** for SSD1680 wiring presets (S31 builds, but has no e-paper pin defaults)
- External SSD1680 panel (128×296, 4736-byte 1bpp frame)

## How to use

```bash
cd examples/bluetooth/ble_profiles/ble_esl/ble_esl_tag
idf.py set-target esp32c6    # or: idf.py --preview set-target esp32h4
idf.py erase-flash
idf.py build flash monitor
```

`sdkconfig.defaults` selects the `ESP_ESL_TAG` name, 4 image slots, `MAX_IMAGE_SIZE=4736`, 4MB flash, NimBLE bond persistence, and `partitions.csv` (`images`, data subtype `0x40`, 64 KB at `0x210000`). `sdkconfig.defaults.esp32c6` and `sdkconfig.defaults.esp32h4` select the SSD1680 backend and LED none. ESP32-S31 stays on the mock display; it still uses the same slots, object size, and `images` partition, so image-store init does not abort. A partition-table change needs `idf.py erase-flash` before the next flash. GAP connection reattempt stays at the chip default (enabled on ESP32-C6/H4). Advertising restart after disconnect stays in `ble_conn_mgr`.

## E-paper pins (Kconfig defaults)

| Item | ESP32-H4 | ESP32-C6 |
|------|----------|----------|
| Power GPIO | 37 (active-high) | 8 (active-low) |
| DC / RST / CS / BUSY | 1 / 16 / 0 / 17 | 20 / 19 / 21 / 22 |
| MOSI / SCLK | 38 / 39 | 18 / 15 |
| Frame | 128 × 296 / 8 = **4736** bytes, linear 1bpp | same |

Display Information reports **296×128** black/white to match the product App.
