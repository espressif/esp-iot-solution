# BLE Connection OTA Profile

`ble_ota_raw` implements a BLE OTA profile based on `esp_ble_conn_mgr`.
It parses OTA commands and firmware packets, verifies CRC by sector, and reports ACK via the OTA service characteristics.

## Features

- Integrates with `esp_ble_conn_mgr` backend (NimBLE path).
- Reuses BLE OTA Service UUID `0x8018` and characteristics `0x8020`~`0x8023`.
- Supports START/STOP command flow with CRC16 verification.
- Buffers firmware by 4 KB sector and calls user callback only after sector CRC passes.
- Exposes total firmware length received from START command.

## Dependencies

- `ble_conn_mgr`
- `ble_services` (for OTA Service and DIS Service)

These dependencies are added through the component `CMakeLists.txt`.

## Public API

Header: `include/esp_ble_ota_raw.h`

- `esp_ble_ota_raw_init()`: register OTA and DIS services.
- `esp_ble_ota_raw_deinit()`: remove OTA service and release runtime state.
- `esp_ble_ota_raw_recv_fw_data_callback()`: register sector-level firmware callback.
- `esp_ble_ota_raw_get_fw_length()`: get firmware total length from START command.

## Typical Usage

1. Initialize BLE connection manager (`esp_ble_conn_init`).
2. Initialize profile (`esp_ble_ota_raw_init`).
3. Register firmware callback (`esp_ble_ota_raw_recv_fw_data_callback`).
4. Start advertising (`esp_ble_conn_start`).
5. In callback, write received firmware data into OTA partition.

Minimal flow:

```c
ESP_ERROR_CHECK(esp_ble_conn_init(&config));
ESP_ERROR_CHECK(esp_ble_ota_raw_init());
ESP_ERROR_CHECK(esp_ble_ota_raw_recv_fw_data_callback(my_recv_fw_cb));
ESP_ERROR_CHECK(esp_ble_conn_start());
```

## OTA Data Path

- Host sends command packet to `COMMAND_CHAR (0x8022)`.
- Profile validates command CRC16 and replies ACK by Notify.
- Host sends firmware packet stream to `RECV_FW_CHAR (0x8020)`.
- Profile validates sector order, packet sequence, and sector CRC16.
- On successful sector completion, registered callback is invoked.

## Example

See `examples/bluetooth/ble_profiles/ble_ota`.
