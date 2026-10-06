| Supported Targets | ESP32-H4 | ESP32-S31 |
| ----------------- | -------- | --------- |

# BLE ESL Tag Example

Minimal Tag demo for the Electronic Shelf Label Profile:

- Advertises with ESL Service UUID `0x1857`
- Sets Display / Image / LED / Sensor Information **before** `esp_ble_eslp_init`
- Accepts association (Address, Keys, Absolute Time); **Update Complete** marks Associated
- Receives OTP image writes and reports `IMAGE_WRITTEN`
- Handles Display / LED / Sensor commands (demo HW logs + sensor report)
- Enters Synchronized via PAST and answers PAwR Ping
- Bonds with LE Secure Connections (`SM_CAP=10`)

## Requirements

- ESP-IDF **v6.1 or newer**
- Target chip: **ESP32-H4** or **ESP32-S31**
- Pair with `ble_esl_ap` on a second board

## How to Use Example

```bash
cd examples/bluetooth/ble_profiles/ble_esl/ble_esl_tag
idf.py set-target esp32h4
idf.py build flash monitor
```

Or use `esp32s31` as the target.

### Configure

```bash
idf.py menuconfig
```

- `Example Configuration` - advertisement name and demo display/image info
- `BLE Profile: Electronic Shelf Label` - enabled by `sdkconfig.defaults`

## Demo flow (with ble_esl_ap)

1. Tag advertises as `ESP_ESL_TAG`
2. AP bonds (LE SC) and writes Address / Keys / Absolute Time (still Configuring)
3. AP reads Info, waits for OTS, transfers a 64-byte demo image (index 0)
4. AP sends Display Image, LED Control, Read Sensor over ECP
5. AP sends Update Complete → Tag **Associated**, then PAwR + PAST
6. Tag becomes **Synchronized**, drops ACL, answers PAwR Ping

`Configuring → Unsynchronized` only appears on early ACL loss (config done but no PAST yet), not on the happy path above.

## Example Output

```
I (xxx) ble_esl_tag: ESL Tag started. Advertising as ESP_ESL_TAG
I (xxx) ble_esl_tag: Connected
I (xxx) ble_esl_tag: State: UNASSOCIATED -> CONFIGURING
I (xxx) ble_esl_tag: Address written: ...
I (xxx) ble_esl_tag: AP Sync Key Material written
I (xxx) ble_esl_tag: Response Key Material written
I (xxx) ble_esl_tag: Absolute Time written: ...
I (xxx) ble_esl_tag: Image written: index=0 size=64 status=ESP_OK
I (xxx) ble_esl_tag: Display image: display=0 image=0
I (xxx) ble_esl_tag: LED control: led=0 RGB=(3,0,0) bright=2 off=0
I (xxx) ble_esl_tag: Sensor read: index=0 report=ESP_OK
I (xxx) ble_esl_tag: Associated
I (xxx) ble_esl_tag: Synchronized
I (xxx) ble_esl_tag: State: CONFIGURING -> SYNCHRONIZED
```

## Troubleshooting

For any technical queries, please open an [issue](https://github.com/espressif/esp-iot-solution/issues) on GitHub.
