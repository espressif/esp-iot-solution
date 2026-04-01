# BLE Connection OTA Example

This example demonstrates BLE OTA firmware upgrade based on:

- `esp_ble_conn_mgr` for BLE GATT transport
- `ble_ota_raw` profile for OTA protocol parsing
- ESP-IDF OTA APIs for writing firmware to OTA partition

## What This Example Does

1. Initializes BLE connection manager and starts advertising OTA service UUID `0x8018`.
2. Registers OTA profile callback to receive validated sector data.
3. Pushes received firmware data into a ring buffer.
4. OTA task reads ring buffer and writes image to next OTA partition.
5. Reboots into the new partition after `esp_ota_end()` and `esp_ota_set_boot_partition()`.

## Build and Flash

```bash
idf.py set-target <your-target>
idf.py build flash monitor
```

## Configuration

Run `idf.py menuconfig` and check:

- `Example Configuration` -> `BLE OTA Device Name`
- Bluetooth host/backend options required by your target

Default partition table in this example includes OTA app partitions (`ota_0`, `ota_1`).

## Expected Logs

- Boot log with BLE initialization
- `ota_task start`
- `wait for data from ringbuf! fw_len = ...`
- `recv: ..., recv_total: ...` during transfer
- Device restarts when OTA finishes successfully

## Test With Mobile App

You can use Espressif BLE OTA Android app:

- [esp-ble-ota-android releases](https://github.com/EspressifApps/esp-ble-ota-android/releases/tag/rc)

## Notes

- Host must send START command before firmware packets.
- Firmware packets must follow sector order and include sector-end CRC.
- OTA image must match target chip and partition constraints.
