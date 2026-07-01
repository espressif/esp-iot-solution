# esp_xmodem

An ESP-IDF component that implements the **XMODEM** and **XMODEM-1K** file-transfer protocol over a UART link.

The component supports both **sender** and **receiver** roles, optional YMODEM-style file-name/size header packets, CRC-16 or checksum modes, and is compatible with all mainstream ESP32 family targets.

---

## Origin & History

The original code was developed by **Espressif Systems (Shanghai) PTE LTD** (copyright 2020) and was first published on Espressif's Chinese Gitee repository:

> **https://gitee.com/EspressifSystems/esp-iot-solution**  
> path: `components/esp_xmodem`

A mirror also exists on GitHub:

> **https://github.com/espressif/esp-iot-solution**  
> path: `components/esp_xmodem`

The original component targeted ESP-IDF **4.x** and the ESP8266 RTOS SDK.  
This fork was created to make the component build and run correctly on **ESP-IDF 5.x** (specifically tested on 5.3.4) with the ESP32-S3 as primary target.

---

## Changes from the original (IDF 5.x port)

The table below summarises every change made relative to the Gitee/GitHub original.

| Category | File(s) changed | What changed and why |
|----------|-----------------|----------------------|
| **API type rename** | `src/priv/esp_xmodem_priv.h`, `src/esp_xmodem_common.c` | `xTaskHandle` → `TaskHandle_t`. The old typedef was removed from FreeRTOS headers bundled with IDF ≥ 4.0; the canonical name has always been `TaskHandle_t`. |
| **UART component split** | `CMakeLists.txt` | IDF 5.3 moved `driver/uart.h` into its own component `esp_driver_uart`. `CMakeLists.txt` now detects the IDF version at configure time and links the correct component (`esp_driver_uart` on IDF ≥ 5.3, `driver` on older). |
| **Format-specifier warnings → errors** | `src/esp_xmodem_sender.c`, `src/esp_xmodem_receiver.c`, `examples/xmodem_sender/main/xmodem_sender_example.c` | On Xtensa (ESP32/S3), `uint32_t` is `long unsigned int`, not `unsigned int`. Using `%d`, `%u`, or `%x` on a `uint32_t` triggers `-Werror=format=`. Fixed with `<inttypes.h>` `PRIu32` / `PRIx32` macros and explicit casts where needed. |
| **UART init order** | `src/esp_xmodem_transport.c` | The original called `uart_set_pin` before `uart_param_config`. The correct IDF order is `uart_param_config` → `uart_set_pin` → `uart_driver_install`. The wrong order is silently accepted on older IDF but causes subtle UART mis-configuration on newer versions. |
| **Use-after-free on queue reset** | `src/esp_xmodem_transport.c` | `xQueueReset()` was called *after* `uart_driver_delete()`. Because `uart_driver_delete` frees the internal queue object, this was operating on a dangling pointer. Fixed by calling `xQueueReset` before the driver delete. |
| **`bzero` removed** | `src/esp_xmodem_transport.c` | `bzero()` was removed from POSIX.1-2008 and is not available in newlib-nano (used by ESP-IDF). Replaced with `memset(..., 0, n)`. |
| **Stop-task race condition (bug fix)** | `src/esp_xmodem_common.c` | `esp_xmodem_stop` previously passed the raw `TaskHandle_t` value to the stop helper task. If the sender/receiver task already self-deleted (and zeroed `handle->process_handle`) before the 500 ms delay expired, the helper would call `vTaskDelete()` on a stale, freed handle — undefined behaviour in FreeRTOS. Fixed by passing the `esp_xmodem_t *handle` pointer instead and re-reading `handle->process_handle` after the delay so we call `vTaskDelete` only when the task is still alive. |
| **Transport memory leak (bug fix)** | `src/esp_xmodem_transport.c` | The `esp_xmodem_transport_t` struct (allocated by `calloc` in `esp_xmodem_transport_init`) was never freed in `esp_xmodem_transport_close`, leaking heap on every open+close cycle. Added `free(p); handle->transport = NULL;` at the end of the close function. |
| **Wrong log message (bug fix)** | `src/esp_xmodem_receiver.c` | Error log printed `"ESP_XMODEM_SERVER"` instead of `"ESP_XMODEM_RECEIVER"`. Fixed. |
| **Kconfig task tuning** | `Kconfig` (new file) | Added four `menuconfig` options so users can adjust task stack sizes and FreeRTOS priorities without touching source code. |
| **Component Registry manifest** | `idf_component.yml` (new file) | Added the mandatory registry manifest (version, targets, license, examples). |
| **Example — ESP32-S3 pins** | `examples/xmodem_sender/main/xmodem_sender_example.c` | Added `#elif defined(CONFIG_IDF_TARGET_ESP32S3)` section that sets UART pins to GPIO17 (TX) / GPIO18 (RX). |
| **Example — CMake hardcoded target** | `examples/xmodem_sender/CMakeLists.txt`, `examples/xmodem_receiver/CMakeLists.txt` | Removed the hardcoded `IDF_TARGET "esp32"` fallback that would silently override `idf.py set-target`. |
| **Missing sdkconfig defaults** | `examples/xmodem_sender/sdkconfig.defaults.esp32s3`, `examples/xmodem_receiver/sdkconfig.defaults.esp32s3` | Added missing per-target sdkconfig defaults so the example CMake does not fail on ESP32-S3. |

---

## Features

| Feature | Detail |
|---------|--------|
| Protocol | XMODEM (128 B blocks), XMODEM-1K (1024 B blocks) |
| CRC mode | CRC-16 (sender auto-negotiated) or checksum |
| File header | Optional YMODEM-compatible filename + size packet |
| Roles | Sender **and** Receiver in the same component |
| UART | Any UART port; fully configurable baud/pins |
| ESP-IDF | ≥ 4.4 — tested on 4.4, 5.0, 5.1, 5.3 |
| Targets | ESP32, ESP32-S2, **ESP32-S3**, ESP32-C3, ESP32-C6, ESP32-H2 |

---

## Component dependencies

| IDF version | UART component used |
|-------------|---------------------|
| IDF < 5.3   | `driver`            |
| IDF ≥ 5.3   | `esp_driver_uart`   |

`CMakeLists.txt` resolves this automatically at configure time — no manual changes required.

---

## Directory layout

```
esp_xmodem/
├── CMakeLists.txt          # Component build file (IDF version-aware)
├── Kconfig                 # Menuconfig options (stack sizes, priorities)
├── idf_component.yml       # ESP Component Registry manifest
├── CHANGELOG.md
├── license.txt             # Apache 2.0
├── include/
│   ├── esp_xmodem.h        # Public API
│   └── esp_xmodem_transport.h
├── private_include/
│   └── esp_xmodem_priv.h   # Internal types (not for application use)
├── src/
│   ├── esp_xmodem_common.c
│   ├── esp_xmodem_sender.c
│   ├── esp_xmodem_receiver.c
│   ├── esp_xmodem_transport.c
└── examples/
    ├── xmodem_sender/      # ESP32-S3 + Wi-Fi + HTTP download → UART
    └── xmodem_receiver/    # Bare UART receiver, writes via callback
```

---

## Quick start

### Add the component to your project

**Option A — ESP Component Registry**
```bash
idf.py add-dependency "espressif/esp_xmodem"
```

**Option B — local / submodule**

Add the component directory to `EXTRA_COMPONENT_DIRS` in your project's `CMakeLists.txt`, or place the folder inside your project's `components/` directory.

---

## API overview

### Initialise

```c
// 1. Init transport (UART)
esp_xmodem_transport_config_t transport_cfg = {
    .baud_rate  = 115200,
    .uart_num   = UART_NUM_1,
    .swap_pin   = true,
    .tx_pin     = 17,
    .rx_pin     = 18,
    .rts_pin    = UART_PIN_NO_CHANGE,
    .cts_pin    = UART_PIN_NO_CHANGE,
};
esp_xmodem_transport_handle_t transport = esp_xmodem_transport_init(&transport_cfg);

// 2. Init xmodem handle
esp_xmodem_config_t cfg = {
    .role              = ESP_XMODEM_SENDER,   // or ESP_XMODEM_RECEIVER
    .event_handler     = my_event_cb,
    .support_xmodem_1k = true,
};
esp_xmodem_handle_t xmodem = esp_xmodem_init(&cfg, transport);

// 3. Start – begins the connect handshake in a background task
esp_xmodem_start(xmodem);
```

### Sender – send data inside the CONNECTED event

```c
esp_err_t my_event_cb(esp_xmodem_event_t *evt)
{
    if (evt->event_id == ESP_XMODEM_EVENT_CONNECTED) {
        // Optional: send YMODEM file-name header first
        esp_xmodem_sender_send_file_packet(evt->handle,
                                           "firmware.bin", 12, total_bytes);
        // Stream the payload in chunks
        while (bytes_remaining) {
            esp_xmodem_sender_send(evt->handle, buf, chunk_len);
            bytes_remaining -= chunk_len;
        }
        // Signal end-of-transmission
        esp_xmodem_sender_send_eot(evt->handle);
    }
    return ESP_OK;
}
```

### Receiver – supply a write callback

```c
esp_err_t my_write_cb(uint8_t *data, uint32_t len)
{
    // write to flash / file / buffer
    return ESP_OK;
}

esp_xmodem_config_t cfg = {
    .role    = ESP_XMODEM_RECEIVER,
    .crc_type = ESP_XMODEM_CRC16,
    .recv_cb = my_write_cb,
    .event_handler = my_event_cb,
};
```

### Cleanup

```c
esp_xmodem_transport_close(xmodem);
esp_xmodem_clean(xmodem);
```

---

## Menuconfig options

```
Component config → ESP XModem
  ├── Sender task stack size (bytes)          [default: 4096]
  ├── Receiver task stack size (bytes)        [default: 4096]
  ├── Sender/receiver task FreeRTOS priority  [default: 11]
  └── Stop-helper task FreeRTOS priority      [default: 12]
```

---

## Examples

### `examples/xmodem_sender`

Downloads a firmware binary from an HTTP server over Wi-Fi and streams it to a peer device over UART using XMODEM-1K.

```bash
cd examples/xmodem_sender
idf.py set-target esp32s3
idf.py menuconfig          # set Wi-Fi SSID/password, server IP, filename
idf.py build flash monitor
```

### `examples/xmodem_receiver`

Receives an XMODEM stream on UART and passes each data block to a user-supplied callback (default: prints to log).

```bash
cd examples/xmodem_receiver
idf.py set-target esp32s3
idf.py build flash monitor
```

---

## ChangeLog

See [CHANGELOG.md](CHANGELOG.md).

---

## License

Apache License 2.0 — see [license.txt](license.txt).  
Original implementation © 2020 Espressif Systems (Shanghai) PTE LTD.
