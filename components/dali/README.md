# DALI Bus Controller Driver (`esp_dali`)

[![Component Registry](https://components.espressif.com/components/espressif/esp_dali/badge.svg)](https://components.espressif.com/components/espressif/esp_dali)

A DALI (Digital Addressable Lighting Interface, IEC 62386) master controller driver for ESP-IDF. The driver uses the on-chip **RMT (Remote Control Transceiver)** peripheral to generate and decode the Manchester-encoded DALI physical layer, requiring no external timing logic.

---

## Features

- **Full IEC 62386 physical layer** — Manchester encoding at Te = 416.67 µs, ±10 % tolerance window.
- **Forward frame TX** — Sends 16-bit DALI forward frames (address byte + command/data byte).
- **Backward frame RX** — Receives and decodes 8-bit DALI backward frames from slave devices.
- **All addressing modes** — Short address (0–63), group address (0–15), broadcast, and special-command addressing.
- **Direct arc-power control (DAPC)** — Set lamp output level directly (0x01–0xFE).
- **Indirect control commands** — OFF, UP, DOWN, STEP_UP, RECALL_MAX, GO_TO_SCENE, etc.
- **Configuration commands** — RESET, STORE_DTR_AS_*, ADD/REMOVE group, scene storage.
- **Query commands** — QUERY_STATUS, QUERY_ACTUAL_LEVEL, QUERY_VERSION, and all IEC 62386-102 queries.
- **Special (commissioning) commands** — INITIALISE, RANDOMISE, COMPARE, PROGRAM_SHORT_ADDRESS, etc.
- **Send-twice** — Automatic re-transmission for commands that require two consecutive forward frames within 100 ms.
- **ESP-IDF v5.0+** compatible — Uses the new `esp_driver_rmt` component API.

---

## Supported Targets

| ESP32 | ESP32-S2 | ESP32-S3 | ESP32-C3 | ESP32-C6 | ESP32-P4 |
|:-----:|:--------:|:--------:|:--------:|:--------:|:--------:|
| Yes   | Yes      | Yes      | Yes      | Yes      | Yes      |

---

## Requirements

- **ESP-IDF** v5.0.0 or later
- **Two GPIO pins** — one for TX (DALI bus drive), one for RX (DALI bus sense)
- **External DALI interface circuit** — The GPIO signals are 3.3 V logic; a DALI-compliant bus transceiver (e.g., based on a current-limited open-drain driver and optocoupler) is required to interface with the 9.5 V–22.5 V DALI bus.


## API Overview

### Include headers

```c
#include "dali.h"          /* Driver API and constants  */
#include "dali_commands.h" /* DALI command code macros  */
```

### Initialization

```c
dali_init(GPIO_NUM_4,   
          GPIO_NUM_5); 
```

### Sending a command (no reply expected)

```c
dali_transaction(DALI_ADDR_BROADCAST, 0, true, DALI_CMD_OFF,
                 false, DALI_TX_TIMEOUT_MS, NULL);
dali_wait_between_frames();
```

### Direct arc-power control (DAPC)

```c
dali_transaction(DALI_ADDR_SHORT, 0, false, 200,
                 false, DALI_TX_TIMEOUT_MS, NULL);
dali_wait_between_frames();
```

### Querying a device (reply expected)

```c
int reply;
esp_err_t err = dali_transaction(DALI_ADDR_SHORT, 0, true,
                                  DALI_CMD_QUERY_ACTUAL_LEVEL,
                                  false, DALI_TX_TIMEOUT_MS, &reply);
if (err == ESP_OK && DALI_RESULT_IS_VALID(reply)) {
    ESP_LOGI(TAG, "Actual level: 0x%02X", (uint8_t)reply);
}
dali_wait_between_frames();
```

### Send-twice commands

```c
/* RESET must be sent twice within 100 ms. Pass send_twice = true. */
dali_transaction(DALI_ADDR_SHORT, 0, true, DALI_CMD_RESET,
                 true, DALI_TX_TIMEOUT_MS, NULL);
dali_wait_between_frames();
```

### Frame timing

`dali_wait_between_frames()` inserts a 20 ms delay. Call it after **every** `dali_transaction()` to ensure the minimum inter-frame gap required by IEC 62386 (> 22 Te ≈ 9.2 ms).

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
├── LICENSE
├── README.md
├── dali.c                  # Driver implementation
└── include/
    ├── dali.h              # Public API
    └── dali_commands.h     # Command code definitions
```

---

## License

Apache License 2.0. See [LICENSE](LICENSE) for details.
