# Changelog

## v0.1.0~1 - 2026-10-22

### CI / Test Changes

- Use USB Serial/JTAG as the ESP32-P4 RAMFS test console to avoid UART write timeouts on the test runner.
- Reformat the dedicated FATFS test partition at startup and fail immediately if its warmup I/O fails, so stale FAT contents cannot cause sync-test failures.
- No changes to the RAMFS implementation or public API.
