# ChangeLog

## [0.2.0] - 2026-05-25

START ACK carries recommended sector send window (byte 6) so the central can pipeline sectors; default window is 1 for compatibility with older apps.

* Added `esp_ble_ota_raw_set_sector_send_window_for_ringbuf()` to derive the window from firmware ring buffer capacity (1..64 sectors).
* Added `esp_ble_ota_raw_set_ota_begin_cb()`; OTA receive starts only after the begin hook succeeds (e.g. flash erase / `esp_ota_begin`).

## [0.1.0] - 2026-03-25

BLE OTA (Bluetooth Low Energy Over-the-Air Update) allows firmware images to be transferred to devices over Bluetooth Low Energy.

It enables wireless firmware upgrade for embedded devices, with command control, sector-based data transfer, and ACK feedback during the OTA process.

BLE OTA GATT Definition

- Service UUID (0x8018)
- Characteristic UUID (RECV_FW) (0x8020)
- Characteristic UUID (PROGRESS_BAR) (0x8021)
- Characteristic UUID (COMMAND) (0x8022)
- Characteristic UUID (CUSTOMER) (0x8023)

The OTA characteristics support Write Without Response and Notify for firmware transport, command/ACK exchange, progress reporting, and optional customer-defined data.

Features:

- BLE-OTA: BLE OTA profile based on `esp_ble_conn_mgr`, with command parser, sector CRC check, and firmware receive callback.
