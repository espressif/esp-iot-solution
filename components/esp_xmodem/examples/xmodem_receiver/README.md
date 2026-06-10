# XModem Receiver Example

This example waits on UART for an XMODEM sender and writes each received block
directly to the next OTA partition using `esp_ota_write`.  When the transfer
finishes the device reboots into the new firmware.

Typical use-case: OTA firmware update for an ESP32-family target pushed from an
external host (PC running `sz`, another ESP32 running the xmodem_sender example,
or any XMODEM-capable tool) over a UART link.

---

## Hardware

| Signal | ESP32 GPIO (default) | ESP32-S3 GPIO (default) | Notes                         |
|--------|----------------------|-------------------------|-------------------------------|
| TX     | 17                   | 17                      | → TX of host / sender device  |
| RX     | 16                   | 18                      | ← RX of host / sender device  |
| GND    | GND                  | GND                     | Common ground required        |

> **Note:** Pin numbers are set in `xmodem_receiver_start()` via
> `esp_xmodem_transport_config_t`.  Change them to match your board.

---

## Partition table

This example requires a **two-OTA-slot** partition table.  Use the supplied
`sdkconfig.defaults.<target>` which sets:

```
CONFIG_PARTITION_TABLE_TWO_OTA=y
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
```

If your flash is different, supply a custom `partitions.csv` and set
`CONFIG_PARTITION_TABLE_CUSTOM=y` in menuconfig.

---

## Build and flash (ESP32-S3)

```bash
cd examples/xmodem_receiver
idf.py set-target esp32s3
idf.py build flash monitor
```

---

## Sending the binary (from a PC)

Install `lrzsz` and use `sz` to push a firmware file over the serial port:

```bash
# Linux/macOS — replace /dev/ttyUSB0 with your port
sz --xmodem --1k firmware.bin < /dev/ttyUSB0 > /dev/ttyUSB0
```

Alternatively, use the companion `xmodem_sender` example running on a second
ESP32 to push the binary over a direct UART-to-UART connection.

---

## Expected output

```
I (3001) xmodem_receiver_example: Starting OTA...
I (3010) xmodem_receiver_example: Writing to partition subtype 17 at offset 0x110000
I (3011) xmodem_receiver_example: esp_ota_begin succeeded
I (3012) xmodem_receiver_example: Please Wait. This may take time
I (18050) xmodem_receiver_example: ESP_XMODEM_EVENT_FINISHED
I (18060) xmodem_receiver_example: OTA successfully, will restart now...
```

---

## Supported targets

| Target    | Tested |
|-----------|--------|
| ESP32     | ✓      |
| ESP32-S3  | ✓      |

