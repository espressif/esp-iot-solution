# DALI LED Lighting Fixture Control Example Application

This example demonstrates how to use the `dali` component to control DALI lighting fixtures for brightness adjustment and to query device status.

## Main Features

- Initialize the DALI driver (RX/TX GPIO; RX/TX INVERT)
- Direct Arc Power Control (DAPC): `OFF -> 25% -> 50% -> 100%`
- Indirect command demo: `RECALL_MAX_LEVEL`
- Query command demo: `QUERY_STATUS`, `QUERY_ACTUAL_LEVEL`, `QUERY_MAX_LEVEL`, `QUERY_MIN_LEVEL`, `QUERY_DEVICE_TYPE`
- Configuration command demo (send-twice): `STORE_ACTUAL_LEVEL`

## Hardware Connections

1. ESP32-C3 (or any ESP32 series chip with the RMT peripheral)

```
ESP32 RX ──┐
           ├── TTL-to-DALI ──┬── DALI bus ── DALI driver ── LED fixture
ESP32 TX ──┘                 │
                    DALI power supply (16 V)
```

2. Hardware Notes:

   - DALI power module: the DALI bus uses 16 V (high) and 0 V (low). You must use a dedicated DALI bus power module.
   - DALI driver: you need a DALI driver to enable communication between the LED fixture and the DALI bus.
   - DALI transceiver (TTL-to-DALI): you need a TTL-to-DALI module to connect the ESP32 to the DALI bus while providing electrical isolation.

## Configuration Steps

1. Go to the example directory:

```bash
cd ./esp-iot-solution/examples/lighting/dali_basic
```

2. Run `idf.py set-target <your_target>` (e.g., `esp32` / `esp32c6`)
4. Build, flash and monitor:

```bash
idf.py -p <PORT> build flash monitor
```

## Notes

- The DALI bus protocol has strict timing requirements. It is recommended to use a dedicated DALI conversion chip/transceiver and a proper DALI driver.
- If your TTL-to-DALI module inverts the signal internally, enable `DALI_TX_INVERT` or `DALI_RX_INVERT` in `menuconfig`.
- Configuration commands such as `STORE_ACTUAL_LEVEL` are sent twice and must be issued within 100 ms; otherwise, they may have no effect.

## Example Output

After the program starts, it runs in a loop:

- Print DAPC level logs
- Send the indirect command and print logs
- Query device results and print them (if no reply is received, it will show `NO REPLY`)
- Send `STORE_ACTUAL_LEVEL` to demonstrate the send-twice configuration command

Sample log:

```log
I (304) dali: TX polarity: inverting (DALI_TX_INVERT=y)
I (305) dali: DALI driver initialized (RX GPIO 5, TX GPIO 6 inv)
I (307) dali_example: === Round 1 ===
I (310) dali_example: [DAPC] OFF
I (1399) dali_example: [DAPC] 25%  (0x3F)
I (2484) dali_example: [DAPC] 50%  (0x7F)
I (3569) dali_example: [DAPC] MAX  (0xFE)
I (4654) dali_example: [CMD]  RECALL_MAX_LEVEL
I (4970) dali_example: [QUERY] STATUS              = 0x24  (36)
I (5222) dali_example: [QUERY] ACTUAL_LEVEL        = 0xFE  (254)
I (5473) dali_example: [QUERY] MAX_LEVEL           = 0xFE  (254)
I (5724) dali_example: [QUERY] MIN_LEVEL           = 0x01  (1)
I (5975) dali_example: [QUERY] DEVICE_TYPE         = 0x06  (6)
I (6195) dali_example: [CMD]  STORE_ACTUAL_LEVEL (send-twice)
```

