# ESP-BLE-UART Vibe Indicator Example

[中文](README_cn.md)

| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-S3 | ESP32-S31 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | --------- |

`ble_uart_vibe_indicator` is an ESP-IDF device-side example for **vibe coding**: the board exposes one or more red / yellow / green signal-light groups over **ESP-BLE-UART**. The host sends **JSONL** commands (Signal Light Control Protocol v1) such as `query` and `control`; firmware parses them and drives GPIO for off, on, slow blink, and fast blink. End-to-end use is not firmware-only—you also run the [**ESP-BLE-UART Bridge**]($IDF_PATH/tools/ble/ble_uart_bridge) **`daemon`** on the PC (`connection-check`, then `daemon <DEVICE_ID>`). The daemon holds the BLE connection, subscribes to notifications, forwards JSON commands over GATT, and returns responses to scripts or automation. In a vibe coding workflow, this maps coding or task progress and state to visible lamp patterns: the device runs the lamps; the bridge daemon provides the host-side BLE link and integration.

## Features

- Reuses the shared [ble_uart](${IDF_PATH}/examples/bluetooth/common/ble_uart) component (NimBLE default)
- `op: "command"` with `query` / `control` sub-commands
- Batch control via `data.payload[]`
- Kconfig: group count (default 3), per-group GPIO pins, slow/fast blink timing, device name prefix

## Hardware wiring

Configure GPIO pins in menuconfig:

```
idf.py menuconfig → ESP-BLE-UART Vibe Indicator Example
```

Set `Channel N red/yellow/green GPIO` for each group. Default `-1` means unconnected (protocol still works; lamps simply do not toggle).

**You must assign GPIOs for your board before expecting visible output.** This example does not ship board-specific pin defaults.

## Build and flash

Primary bring-up target is **ESP32-H4**; ESP32-H21 is equally supported.

```bash
cd esp-iot-solution/examples/bluetooth/ble_uart_vibe_indicator
idf.py --preview set-target esp32h4    # or esp32h21
idf.py --preview build flash monitor
```

ESP32-H4 and ESP32-H21 are preview targets in this IDF version; keep the `--preview` flag on `idf.py` subcommands.

The device advertises as `Vibe-Indicator-XXXX` (last two BT MAC bytes).

BLE runs with `encrypted = false` for easier debugging.

## Protocol

See [json_format.md](json_format.md) for wire format, error codes, and examples.

Typical host sequence:

1. Connect over BLE (NUS / ESP-BLE-UART GATT)
2. Query `indicator_count`
3. Send `control` with one or more entries in `payload`

Every JSONL line must end with `\n`.

## Host testing with ESP-BLE-UART Bridge

Install bridge dependencies once:

```bash
cd $IDF_PATH
. ./export.sh
python -m pip install -r tools/ble/ble_uart_bridge/requirements.txt
```

Replace `<DEVICE_ID>` with the BLE address from `list-devices` or the monitor log.

```bash
cd $IDF_PATH/tools/ble/ble_uart_bridge

python main.py connection-check <DEVICE_ID>
python main.py daemon <DEVICE_ID>

# Query group count
python main.py daemon-send --op command \
  --json '{"cmd":"query","type":"indicator_count"}' --timeout 5

# Control one lamp
python main.py daemon-send --op command \
  --json '{"cmd":"control","payload":[{"indicator_id":0,"light_id":0,"light_action":2}]}' \
  --timeout 5

# Batch control
python main.py daemon-send --op command \
  --json '{"cmd":"control","payload":[{"indicator_id":0,"light_id":0,"light_action":1},{"indicator_id":0,"light_id":1,"light_action":0}]}' \
  --timeout 5
```

### Expected results


| Step                    | Expectation                                                 |
| ----------------------- | ----------------------------------------------------------- |
| `indicator_count` query | `ok: true`, `data.count` matches Kconfig                    |
| `control` (single item) | `ok: true`, `data.payload` echoes request                   |
| `control` (batch)       | All items applied; echoed `payload` matches                 |
| Invalid `light_id`      | `ok: false`, `error: invalid_parameter`, `data.message` set |
| Empty `id`              | `ok: false`, `error: id_not_specified`                      |


## Project layout


| File                     | Role                                               |
| ------------------------ | -------------------------------------------------- |
| `main/app_main.c`        | NVS, indicator + JSONL init, BLE UART install/open |
| `main/ble_jsonl.c`       | JSONL envelope, `query` / `control` dispatch       |
| `main/indicator.c`       | Per-lamp GPIO driver, slow/fast blink timer        |
| `main/Kconfig.projbuild` | Groups, GPIO map, blink timing, name prefix        |
| `json_format.md`         | Protocol reference                                 |


## Related examples

- [ble_uart_service]($IDF_PATH/examples/bluetooth/ble_uart_service) — minimal echo server and Console smoke test
- [ble_uart]($IDF_PATH/examples/bluetooth/common/ble_uart) — shared transport component
- [ESP-BLE-UART Bridge]($IDF_PATH/tools/ble/ble_uart_bridge) — provides a host-side command-line tool for communicating with the device via the BLE-UART channel