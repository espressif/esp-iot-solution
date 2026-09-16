| Supported Targets | ESP32 | ESP32-C3 | ESP32-C2 | ESP32-S3 | ESP32-H2 |
| ----------------- | ----- | -------- | -------- | -------- | -------- |

# BLE Custom Service Example

This example registers a custom GATT service (`0xFFA0`) with two characteristics, then starts advertising.

- `0xFFA1` (Read / Write / Notify): a write from the peer is printed on the serial console.
- `0xFFA2` (Indicate).

`ESP_BLE_CONN_EVENT_CCCD_UPDATE` only updates `notify_enable` / `indicate_enable`. A monitor task then calls `esp_ble_conn_get_chr_handle()` and sends `enable notify` / `enable indicate` through `esp_ble_conn_notify_by_attr_handle()` / `esp_ble_conn_indicate_by_attr_handle()`.

Any BLE scanner app (for example nRF Connect) can be used to test.

## How to Use Example

```bash
idf.py set-target <chip_name>
idf.py menuconfig
idf.py -p PORT flash monitor
```

In `Example Configuration`, the default advertisement name is `BLE_CUSTOM`.

## Example Output

```
I (376) app_main: ESP_BLE_CONN_EVENT_STARTED
I (54526) app_main: ESP_BLE_CONN_EVENT_CONNECTED
I (58366) app_main: Write received, len=5
I (61006) app_main: CCCD update: notify_enable=1 conn=1
I (61016) app_main: Notify sent on conn=1 attr=16
I (62006) app_main: CCCD update: indicate_enable=1 conn=1
I (62016) app_main: Indicate sent on conn=1 attr=20
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub.
