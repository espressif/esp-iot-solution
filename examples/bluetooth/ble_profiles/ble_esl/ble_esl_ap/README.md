| Supported Targets | ESP32-H4 | ESP32-S31 |
| ----------------- | -------- | --------- |

# BLE ESL Access Point Example

Minimal AP demo for the Electronic Shelf Label Profile. Main flow:

1. Scans and connects to a Tag advertising name `ESP_ESL_TAG`
2. Bonds with LE Secure Connections (`SM_CAP=10`)
3. After **ENC**: `configure_tag` writes Address, **non-zero demo Keys**, and Absolute Time (**not** Update Complete)
4. Reads Tag info (Display / Image / LED / Sensor)
5. Waits for **OTS discovered**, then transfers a 64-byte demo image via OTP
6. Sends Display Image, LED Control, and Read Sensor over ECP (one pending at a time)
7. Sends **Update Complete**, starts PAwR, PAST (`sync_transfer`), and queues a Ping

## Requirements

- ESP-IDF **v6.1 or newer**
- Target chip: **ESP32-H4** or **ESP32-S31**
- A second board running `ble_esl_tag`

## How to Use Example

```bash
cd examples/bluetooth/ble_profiles/ble_esl/ble_esl_ap
idf.py set-target esp32h4
idf.py build flash monitor
```

Or use `esp32s31` as the target. Flash `ble_esl_tag` on another board first.

### Configure

```bash
idf.py menuconfig
```

- `Example Configuration` - Tag name, ESL ID, Group ID
- `BLE Connection Management` - Extended Adv, Periodic Adv, PAST, PAwR; SM Bond + SC
- `BLE Profile: Electronic Shelf Label` - AP role

## Demo keys

AP Sync / Response Key Material are filled with fixed demo patterns in
`app_main.c` (`0xA0…` / `0xB0…` session keys). They are for lab demos only.

## Example Output

```
I (xxx) ble_esl_ap: ESL AP started. Scanning for Tag name 'ESP_ESL_TAG'
I (xxx) ble_esl_ap: Connected conn=1
I (xxx) ble_esl_ap: Discovery complete
I (xxx) ble_esl_ap: Link encrypted
I (xxx) ble_esl_ap: Tag provisioned (GATT writes): conn=1 esl_id=0x00 group=0
I (xxx) ble_esl_ap: Tag info: displays=1 image=yes sensors=3 leds=1 pnp=no
I (xxx) ble_esl_ap: OTS discovered on Tag
I (xxx) ble_esl_ap: Image transfer started: index=0 len=64
I (xxx) ble_esl_ap: Image transferred: index=0 status=ESP_OK
I (xxx) ble_esl_ap: display_image: ESP_OK
I (xxx) ble_esl_ap: ECP response conn=1 len=...
I (xxx) ble_esl_ap: led_control: ESP_OK
I (xxx) ble_esl_ap: read_sensor: ESP_OK
I (xxx) ble_esl_ap: update_complete: ESP_OK
I (xxx) ble_esl_ap: PAwR started
I (xxx) ble_esl_ap: PAST set_info done
```

Display → LED → Sensor run one at a time (single ECP pending). Update Complete and PAST run after the Sensor response (or timeout). Image transfer waits for `BLE_OTP_EVENT_OTS_DISCOVERED` (or skips with a clear log if discovery fails).

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub.
