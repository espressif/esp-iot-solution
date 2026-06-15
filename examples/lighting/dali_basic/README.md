# DALI LED Lighting Fixture Control Example Application

This example demonstrates how to use the `dali` component to control LED lighting fixtures for brightness adjustment, color control, status query, and the control and use of devices with 103 and 303 sensors.

## Main Features

- Initialize the DALI driver (RX/TX GPIO; RX/TX INVERT)
- Commission: assign short addresses to Part 102 control gear and Part 103 input devices
- Auto-detect: scan and identify DT6 (dimming) and DT8 (color) lamps by device type
- Direct Arc Power Control (DAPC): `OFF -> 25% -> 50% -> 100%` on all lamps
- Indirect command demo: `RECALL_MAX_LEVEL`
- Query command demo: `QUERY_STATUS`, `QUERY_ACTUAL_LEVEL`, `QUERY_DEVICE_TYPE`, `QUERY_COLOR_CAPABILITIES`
- Part 103 sensor demo: occupancy polling triggers simultaneous blink
  - DT6 lamps: blink together (bright/dark)
  - DT8 lamp: alternate Red/Blue

## Hardware Connections

1. ESP32-C6 (or any ESP32 series chip with the RMT peripheral)
                          
```                            Part 103 sensor                                
ESP32 RX ──┐                         │
           ├── TTL-to-DALI ──┬── DALI bus ── DALI driver ── LED fixture
ESP32 TX ──┘                 │
                    DALI power supply (16 V)
```

2. Hardware Notes:

   - DALI power module: the DALI bus uses 16 V (high) and 0 V (low). You must use a dedicated DALI bus power module.
   - DALI driver: you need a DALI driver to enable communication between the LED fixture and the DALI bus.
   - DALI transceiver (TTL-to-DALI): you need a TTL-to-DALI module to connect the ESP32 to the DALI bus while providing electrical isolation.
   - Part 103 sensor (optional): an occupancy sensor (DLS-203-P) is used in Step 6.Simply connect the DLS-203-P to the DALI bus and draw power directly from the DALI bus; no separate power supply is required.

## Configuration Steps

1. Go to the example directory:

```bash
cd ./esp-iot-solution/examples/lighting/dali_basic
```

2. Modify GPIO pins according to your hardware wiring:

```c
#define DALI_RX_GPIO    5
#define DALI_TX_GPIO    6
```

3. Configure signal polarity (enable `invert_tx/rx` if your TTL-to-DALI module has internal inverters):

```c
dali_master_config_t cfg = {
    .rx_gpio   = DALI_RX_GPIO,
    .tx_gpio   = DALI_TX_GPIO,
    .invert_tx = true,
    .invert_rx = true,
};
```

4. Enable required DALI parts via menuconfig (Please select the required protocol according to your Dali device. This example uses Part 102/103/209/303):

```bash
idf.py menuconfig
# → Component config → DALI Component Configuration
#   Enable: Part 102, Part 103, Part 209, Part 303/304
```

5. Run `idf.py set-target <your_target>` (e.g., `esp32` / `esp32c6`)
6. Build, flash and monitor:

```bash
idf.py -p <PORT> build flash monitor
```

## Notes

- The DALI bus protocol has strict timing requirements. It is recommended to use a dedicated DALI conversion chip/transceiver and a proper DALI driver.
- If the TTL to DALI module has an internal inverting function, the pin polarity needs to be configured.
- Configuration commands such as `STORE_ACTUAL_LEVEL` are sent twice and must be issued within 100 ms; otherwise, they may have no effect.

## Example Output

After the program starts, it runs in sequence:

1. Commission Part 102 and Part 103 devices
2. Scan addresses and identify DT6 / DT8 by device type
3. DAPC dimming sequence on all lamps
4. Indirect command on first discovered lamp
5. Query all discovered lamps and sensor
6. Wait for occupancy and trigger simultaneous blink

Sample log:

```log
I (30628) dali_example: --- Step 2: Scan & identify addresses ---
I (30730) dali_example:     addr  0 -> DT8 RGB lamp  (color_caps=0xE0 XY:N Tc:Y PrimaryN:Y RGBWAF:Y)
I (30862) dali_example:     addr  1 -> lamp (type=6, status=0x00)
I (30994) dali_example:     addr  2 -> lamp (type=6, status=0x02)
I (37483) dali_example:   Identified: 4 DT6 lamp(s), DT8_ADDR=0
...
I (76346) dali_example:   Occupied! Triggering simultaneous DT6 blink and DT8 Red/Blue flash.
I (76580) dali_example:   [1/6] DT6 ON + DT8 Red
I (77255) dali_example:   [1/6] DT6 OFF + DT8 Blue
...
```
