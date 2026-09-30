# Changelog

## v3.0.0 - 2026-02-25

### Breaking Changes

* **`baud_rate` field rename** — `esp_xmodem_transport_config_t.baund_rate` typo.  The field has been renamed to `baud_rate`.  Update
  all call-sites:
  ```c
  // Before
  .baund_rate = 921600,
  // After
  .baud_rate  = 921600,
  ```

### Improve

* **`private_include/` directory** — the private header `esp_xmodem_priv.h`
  has been moved from `src/priv/` to `private_include/`, matching the
  directory-naming convention used by all other esp-iot-solution components.
* **`cmake_utilities` integration** — `idf_component.yml` now declares
  `cmake_utilities: "*"` as a dependency, and `CMakeLists.txt` calls
  `include(package_manager)` + `cu_pkg_define_version()` so the component
  version is available as a compile-time symbol and in the build system,
  consistent with other esp-iot-solution components.

---

## v2.0.1 - 2025-10-02

### Fix

* `esp_xmodem_common.c`: **Stop-task race condition** — `esp_xmodem_stop`
  previously passed the raw `TaskHandle_t` value to the stop helper task.
  If the sender/receiver task self-deleted (clearing `handle->process_handle`
  to NULL) before the 500 ms delay expired, the helper would call
  `vTaskDelete()` on a stale, freed handle — undefined behaviour in
  FreeRTOS that could corrupt the task list.  Fixed by passing the parent
  `esp_xmodem_t *handle` pointer instead and re-reading `process_handle`
  after the delay, so `vTaskDelete` is only called when the task is still
  alive.
* `esp_xmodem_transport.c`: **Transport struct memory leak** — the
  `esp_xmodem_transport_t` struct allocated by `calloc` in
  `esp_xmodem_transport_init` was never freed in
  `esp_xmodem_transport_close`, leaking heap on every open+close cycle.
  Fixed with `free(p); handle->transport = NULL;`.
* `esp_xmodem_receiver.c`: Wrong error log text corrected — the message
  incorrectly said `ESP_XMODEM_SERVER`; now correctly says
  `ESP_XMODEM_RECEIVER`.

---

## v2.0.0 - 2025-02-20

### Breaking Changes

* **CMakeLists.txt**: Component dependency on the UART driver is now
  automatically resolved at configure time.  On IDF ≥ 5.3 the component
  links against `esp_driver_uart`; on earlier versions it links against
  `driver`.  Projects that previously added `driver` to their own
  `REQUIRES` for this component can remove that workaround.

### Fix

* `esp_xmodem_transport.c`: Corrected UART initialisation order to follow
  the ESP-IDF-recommended sequence: `uart_param_config` → `uart_set_pin`
  → `uart_driver_install`.  The previous order (`uart_set_pin` before
  `uart_param_config`) could silently mis-configure GPIO settings on some
  silicon.
* `esp_xmodem_transport.c`: `xQueueReset()` is now called **before**
  `uart_driver_delete()`.  The driver deletes the event queue internally;
  resetting a freed queue was undefined behaviour.
* `esp_xmodem_transport.c`: Replaced deprecated `bzero()` with
  `memset(..., 0, ...)` for POSIX portability.
* `src/priv/esp_xmodem_priv.h`: Replaced removed `xTaskHandle` typedef
  with the current `TaskHandle_t` (FreeRTOS ≥ v8 / IDF ≥ 4.0).
* `esp_xmodem_common.c`: Same `xTaskHandle` → `TaskHandle_t` fix.
* Format specifiers updated to use `<inttypes.h>` `PRI*` macros to silence
  `-Werror=format=` errors on 32-bit targets where `uint32_t` is `long`.
* Added missing `sdkconfig.defaults.esp32s3` for both sender and receiver
  examples.

### Improve

* Added `idf_component.yml` manifest for the ESP Component Registry.
* Added `Kconfig` so task stack sizes and priorities are configurable via
  `idf.py menuconfig` instead of being hardcoded.
* Added `CHANGELOG.md` and `LICENSE` files required by the ESP Component
  Registry review checklist.
* Added ESP32-S3 UART pin configuration to the sender example.
* Example `CMakeLists.txt` files no longer hard-code `"esp32"` as the
  default target — IDF's own tooling handles target selection.

---

## v1.0.0 - 2020-xx-xx  (original release by Espressif)

* Initial implementation — XModem sender and receiver over UART.
* Supports standard 128-byte XMODEM and 1 K-byte XMODEM-1K.
* Optional YMODEM-style file-name/size header packet (`CONFIG_SUPPORT_FILE`).
* Tested on ESP8266 and ESP32.
