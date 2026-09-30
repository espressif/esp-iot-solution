# XModem Sender Example

This example downloads a binary file from an HTTP server over Wi-Fi and
forwards it to a connected device over UART using the XMODEM-1K protocol.

Typical use-case: OTA firmware update for a secondary MCU (e.g. NXP, STM32)
attached to an ESP32-S3 via UART.

---

## Hardware

| Signal | ESP32-S3 GPIO (default) | Notes                         |
|--------|------------------------|-------------------------------|
| TX     | 17                     | → RX of secondary MCU         |
| RX     | 18                     | ← TX of secondary MCU         |
| GND    | GND                    | Common ground required        |

> **Note:** Pin numbers are set in `app_main()` via
> `esp_xmodem_transport_config_t`.  Change them to match your board.

---

## Configuration (`idf.py menuconfig`)

```
Example Configuration
  ├── Server IP address       (e.g. 192.168.1.100)
  ├── Server port             (default: 8070)
  └── Firmware filename       (e.g. firmware.bin)

Example Connection Configuration
  ├── WiFi SSID
  └── WiFi Password
```

---

## Build and flash (ESP32-S3)

```bash
cd examples/xmodem_sender
idf.py set-target esp32s3
idf.py menuconfig   # set Wi-Fi + HTTP server credentials
idf.py build flash monitor
```

---

## Serve the binary from a laptop

On Linux / macOS:
```bash
cd /path/to/your/firmware
python3 -m http.server 8070
```

On Windows (PowerShell):
```powershell
cd C:\path\to\your\firmware
python -m http.server 8070
```

Make sure the laptop and the ESP32-S3 are on the **same Wi-Fi network**.
Set `Server IP address` in menuconfig to the laptop's Wi-Fi IP address.

---

## Expected output

```
I (1234) xmodem_sender_example: Connected to AP, begin http client task
I (1234) xmodem_sender: Connecting to Xmodem server(1/25)
I (2234) xmodem_sender_example: ESP_XMODEM_EVENT_CONNECTED, heap size 240000
I (5678) xmodem_sender_example: Send image success
```

---

## Supported targets

| Target    | Tested |
|-----------|--------|
| ESP32-S3  | ✓      |
