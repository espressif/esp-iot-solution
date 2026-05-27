# DALI Bus Controller Driver (`esp_dali`)

[![Component Registry](https://components.espressif.com/components/espressif/esp_dali/badge.svg)](https://components.espressif.com/components/espressif/esp_dali)

A DALI (Digital Addressable Lighting Interface, IEC 62386) master controller driver for ESP-IDF. The driver uses the on-chip **RMT (Remote Control Transceiver)** peripheral to generate and decode the Manchester-encoded DALI physical layer, requiring no external timing logic.

---

## Features

- **Full IEC 62386 physical layer** — Manchester encoding at Te = 416.67 µs, ±10 % tolerance window.
- **Forward frame TX** — Sends 16-bit DALI forward frames (address byte + command/data byte).
- **Backward frame RX** — Receives and decodes 8-bit DALI backward frames from slave devices.
- **All addressing modes** — Short address (0–63), group address (0–15), broadcast, and special-command addressing.
- **IEC 62386-102 Control Gear** — Direct arc-power control (DAPC), ON/OFF, scene control, configuration, and queries for lamps.
- **IEC 62386-103 Control Devices** — Input device (sensors) support with event-based reporting and commissioning.
- **Configuration commands** — RESET, STORE_DTR_AS_*, ADD/REMOVE group, scene storage.
- **Query commands** — QUERY_STATUS, QUERY_ACTUAL_LEVEL, QUERY_VERSION, and all IEC 62386-102/103 queries.
- **Commissioning** — Automatic short address assignment for both control gear (102) and control devices (103).
- **Special (commissioning) commands** — INITIALIZE, RANDOMIZE, COMPARE, PROGRAM_SHORT_ADDRESS, etc.
- **Send-twice** — Automatic re-transmission for commands that require two consecutive forward frames within 100 ms.
- **Auto inter-frame gap** — The driver automatically inserts the IEC 62386 minimum inter-frame gap (> 22 Te) after every transaction; no manual delay needed.
- **IEC 62386-209 color control** — dali_master_set_color()` supports three color modes: color temperature (CCT/Mirek), RGB (3-channel), and full RGBWAF (6-channel). All required DTR pre-loads, `ENABLE_DEVICE_TYPE 8` framing, and `ACTIVATE` are handled internally.
- **Handle-based API** — `dali_new_master_rmt()` returns an independent instance handle; multiple instances can run concurrently on different GPIO pairs.
- **ESP-IDF v5.0+** compatible — Uses the new `esp_driver_rmt` component API.

---

## Supported Targets

| ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-P4 | ESP32-H2 |
| :---: | :------: | :------: | :------: | :------: | :------: | :------: |
|  Yes  |   Yes   |   Yes   |   Yes   |   Yes   |   Yes   |   Yes   |

---

## Requirements

- **ESP-IDF** v5.5.0 or later
- **Two GPIO pins** — one for TX (DALI bus drive), one for RX (DALI bus sense)
- **External DALI interface circuit** — The GPIO signals are 3.3 V logic; a DALI-compliant bus transceiver (e.g., based on a current-limited open-drain driver and optocoupler) is required to interface with the 9.5 V–22.5 V DALI bus.

## Configuration

Enable the required DALI protocol parts via menuconfig:

```bash
idf.py menuconfig
# → Component config → DALI Component Configuration
```

| Option | Description | Default |
|--------|-------------|---------|
| `DALI_PART102_ENABLED` | Control gear (lamps, drivers) | y |
| `DALI_PART209_ENABLED` | DT8 color control (requires Part 102) | y |
| `DALI_PART103_ENABLED` | Input devices (sensors) | y |
| `DALI_PART303_304_ENABLED` | Occupancy/light sensor helpers (requires Part 103) | y |

> **Note:** Part 101 (physical layer) is always enabled. Disable unused parts to reduce code size.

## API Overview

### Include headers

```c
#include "dali.h"          /* Driver API and constants  */
#include "dali_command.h"  /* DALI command code macros  */
```

### Initialization

```c
dali_master_handle_t dali;
dali_master_config_t cfg = {
    .rx_gpio = GPIO_NUM_4,
    .tx_gpio = GPIO_NUM_5,
    .invert_tx = false,   /* enable if TX hardware path is inverting */
    .invert_rx = false,   /* enable if RX hardware path is inverting */
};
dali_master_rmt_config_t rmt_cfg = {
    .mem_block_symbols = 64,
};
ESP_ERROR_CHECK(dali_new_master_rmt(&cfg, &rmt_cfg, &dali));
```

### Sending a command (no reply expected)

```c
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_BROADCAST, .addr = 0,
    .is_cmd = true, .command = DALI_CMD_OFF,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &tx, NULL);
/* Inter-frame gap is inserted automatically — no explicit delay needed */
```

### Direct arc-power control (DAPC)

```c
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_SHORT, .addr = 0,
    .is_cmd = false, .command = 200,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &tx, NULL);
```

### Querying a device (reply expected)

```c
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_SHORT, .addr = 0,
    .is_cmd = true, .command = DALI_CMD_QUERY_ACTUAL_LEVEL,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
int reply;
esp_err_t err = dali_master_do_transaction(dali, &tx, &reply);
if (err == ESP_OK && DALI_RESULT_IS_VALID(reply)) {
    ESP_LOGI(TAG, "Actual level: 0x%02X", (uint8_t)reply);
}
```

### Address assignment (Commissioning)

**For Control Gear (IEC 62386-102 lamps):**

```c
uint8_t count = 0;
esp_err_t err = dali_commission(dali, DALI_COMMISSION_ALL, 0, 64, &count,
                                DALI_TX_TIMEOUT_MS);
if (err == ESP_OK) {
    ESP_LOGI(TAG, "Commissioning complete: %d device(s) addressed", count);
}
```

**For Control Devices (IEC 62386-103 sensors):**

```c
uint8_t count = 0;
esp_err_t err = dali_103_commission(dali, DALI_COMMISSION_ALL, 0, 64, &count,
                                    DALI_TX_TIMEOUT_MS);
if (err == ESP_OK) {
    ESP_LOGI(TAG, "Input devices commissioned: %d", count);
}
```

> **Quiescent Mode Note:** Both commissioning functions automatically put Part 103 input devices into **quiescent mode** after completion. This prevents sensors from pushing event frames onto the bus immediately, which could be mis-decoded by some Part 102 gear as DAPC brightness commands. Call `dali_103_do_device_command(dali, DALI_ADDR_BROADCAST, 0, DALI_103_STOP_QUIESCENT_MODE, true, timeout, NULL)` when you are ready to receive sensor events.

### Part 103 Input Device API

Part 103 input devices use a different 24-bit frame format from Part 102 gear. Include:

```c
#include "control_device.h"
```

**Key Functions:**

| Function | Purpose |
|----------|---------|
| `dali_103_commission()` | Assign short addresses to input devices |
| `dali_103_do_device_command()` | Send device-level commands (e.g., START/STOP_QUIESCENT_MODE) |
| `dali_103_do_instance_command()` | Send instance-level commands (e.g., Part 303/304 sensor commands) |
| `dali_103_query_device_status()` | Query device status byte |
| `dali_103_query_number_of_instances()` | Query how many sensor instances the device has |
| `dali_103_query_instance_type()` | Query the type of a specific instance (e.g., occupancy=3, light=4) |
| `dali_103_reset_device()` | Reset device to factory defaults |

### Part 303 & 304 Sensor API

For input devices that implement Part 303 (occupancy) or Part 304 (light) sensor instances:

```c
#include "device_sensors.h"
```

**Part 303 — Occupancy Sensor:**

| Function | Purpose |
|----------|---------|
| `dali_303_query_occupancy()` | Query current occupancy state (occupied/unoccupied) |
| `dali_303_query_hold_timer()` | Query hold timer value |
| `dali_303_set_hold_timer()` | Set hold timer (unit: 10s) |
| `dali_303_query_deadtime_timer()` | Query deadtime timer value |

**Part 304 — Light Sensor:**

| Function | Purpose |
|----------|---------|
| `dali_304_query_hysteresis()` | Query hysteresis setting |
| `dali_304_query_report_timer()` | Query report timer value |
| `dali_304_query_deadtime_timer()` | Query deadtime timer value |
| `DALI_304_RAW_TO_LUX_FP(raw)` | Convert raw value to Lux (float) |

```c
/* Query occupancy from instance 0 of sensor at short address 5 */
bool occupied;
esp_err_t err = dali_303_query_occupancy(dali, DALI_ADDR_SHORT, 5, 0, &occupied, timeout);

/* Convert raw illuminance value to Lux */
uint8_t raw_value = 201;  /* Example raw reading */
float lux = DALI_304_RAW_TO_LUX_FP(raw_value);  /* ≈ 100000 Lux */
```

### Send-twice commands

```c
/* RESET must be sent twice within 100 ms. Pass send_twice = true. */
dali_master_transaction_config_t tx = {
    .addr_type = DALI_ADDR_SHORT, .addr = 0,
    .is_cmd = true, .command = DALI_CMD_RESET,
    .send_twice = true, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &tx, NULL);
```

### IEC 62386-209 Color & ColorTemp Control

`dali_master_set_color()` encapsulates the complete DTR pre-load + color-set + ACTIVATE sequence defined by Part 209. Three color modes are supported:

| Mode | Enum | Description |
|------|------|-------------|
| Color temperature | `DALI_COLOR_CCT` | Mirek (153 ≈ 6500 K, 370 ≈ 2700 K) |
| RGB | `DALI_COLOR_RGB` | R/G/B channels, each 0–254 |
| RGBWAF | `DALI_COLOR_RGBWAF` | R/G/B/W/A/F channels, 0xFF = no change |

```c
/* Color temperature */
dali_color_val_t val = { .cct = { .mirek = 370 } };dali_master_set_color(dali, DALI_ADDR_SHORT, 4, DALI_COLOR_CCT, val, DALI_TX_TIMEOUT_MS);

/* RGB */
dali_color_val_t val = { .rgb = { .r = 254, .g = 0, .b = 0 } };dali_master_set_color(dali, DALI_ADDR_SHORT, 4, DALI_COLOR_RGB, val, DALI_TX_TIMEOUT_MS);
```

**Querying color capabilities**: use `dali_enable_device_type()` to unlock Part 209 commands for DT8 before issuing the query:

```c
dali_enable_device_type(dali, 8, DALI_TX_TIMEOUT_MS);

int caps = DALI_RESULT_NO_REPLY;
dali_master_transaction_config_t caps_cfg = {
    .addr_type = DALI_ADDR_SHORT, .addr = 4,
    .is_cmd = true, .command = DALI_209_QUERY_COLOR_CAPABILITIES,
    .send_twice = false, .tx_timeout_ms = DALI_TX_TIMEOUT_MS,
};
dali_master_do_transaction(dali, &caps_cfg, &caps);
/* caps: bit4=XY  bit5=Tc  bit6=Primary-N  bit7=RGBWAF */
```

> dali_master_set_color()` calls `dali_enable_device_type()` internally — no manual call needed. When issuing DT8 query commands directly, call it yourself first.

### Release resources

```c
dali_del_master(dali);
```

### Frame timing

The driver automatically inserts the minimum inter-frame gap required by IEC 62386 (> 22 Te ≈ 9.2 ms) after every `dali_master_do_transaction()` call. No explicit delay helper is needed between consecutive transactions.

---

## DALI Frame Timing Reference

```
Te = 416.67 µs (half-period)

Forward frame (FF):  start(1Te×2) + 16 bits(1Te×2 each) + stop(2Te×2) = 38 Te ≈ 15.8 ms
Backward frame (BF): start(1Te×2) + 8 bits (1Te×2 each) + stop(2Te×2) = 22 Te ≈  9.2 ms

No reply:   FF → wait > 22 Te → next FF
With reply: FF → [7 Te … 22 Te] → BF → wait > 22 Te → next FF
Send-twice: FF1 → wait < 100 ms → FF2
```

---

## Project Layout

```
components/dali/
├── CMakeLists.txt
├── idf_component.yml
├── CHANGELOG.md
├── LICENSE
├── README.md
├── README_cn.md
├── src/
│   ├── dali_control_gear.c          # IEC 62386-102 control gear (lamps)
│   ├── dali_control_device.c        # IEC 62386-103 control devices (input sensors)
│   ├── dali_color_control_dt8.c    # IEC 62386-209 color control (DT8)
│   ├── dali_device_sensors.c        # Sensor-related utilities
│   └── dali_system_components.c     # System component support
└── include/
    ├── dali.h                  # Core public API
    ├── dali_command.h          # Command code definitions
    ├── dali_control_gear.h          # Control gear API
    ├── dali_control_device.h        # Control device API
    ├── dali_color_control_dt8.h    # Color control API
    ├── dali_device_sensors.h        # Sensor API
    └── dali_system_components.h     # System component API
```

---

## License

Apache License 2.0. See [LICENSE](LICENSE) for details.
