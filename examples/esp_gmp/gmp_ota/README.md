# GMP OTA BLE Example

**English** | [中文](README_CN.md)

End-to-end OTA demo over GMP-on-Flux:

- **Server (device)**: writes received firmware **directly to an OTA partition** and reboots (no SPIFFS inside the OTA handler)
- **Client (host)**: **manages the upgrade image in SPIFFS**, then connects and uploads it to the server

## Layout

```text
examples/esp_gmp/gmp_ota/
├── common_components/gmp_ota_common/
├── ble_gmp_ota_server/     # peripheral, dual OTA partitions
└── ble_gmp_ota_client/     # central, SPIFFS holds ota_image.bin
```

## Hardware

- **Server**: 4 MB Flash (dual OTA slots, `0x1A0000` bytes each)
- **Client**: 4 MB Flash (factory + SPIFFS `storage` ~2 MB for firmware)

The image to upload must not exceed the server OTA slot size. The client SPIFFS can hold a larger file, but the server rejects oversized images at `START`.

## Build the server

```bash
cd examples/esp_gmp/gmp_ota/ble_gmp_ota_server
idf.py set-target esp32
idf.py build flash monitor
```

## Build the client

```bash
cp ble_gmp_ota_server/build/ble_gmp_ota_server.bin \
   ble_gmp_ota_client/spiffs/ota_image.bin

cd ble_gmp_ota_client
idf.py set-target esp32
idf.py build flash monitor
```

The client reads `/spiffs/ota_image.bin`, scans for `ESP-GMP-OTA`, then uploads automatically.

If `ble_gmp_ota_client/spiffs/ota_image.bin` is missing, the client still builds and flashes; at runtime it logs that the image is missing and skips the OTA upload.

## Security boundary

This example only demonstrates the GMP-over-Flux OTA data path. It does not verify firmware provenance, does not enable BLE access control, and does not replace signed app images or secure boot. Use it only in controlled lab setups. Production products must add link authentication, application authorization, and firmware trust checks.

The client example does not auto-format a failed SPIFFS mount (`format_if_mount_failed=false`); mount failure aborts with an error. To refresh `ota_image.bin`, regenerate and flash SPIFFS as described in `ble_gmp_ota_client/spiffs/README.md`.

## Component API

Device-side OTA handler: `esp_gmp_ota_init()` (see `components/esp_gmp/profiles/ota/`).

Protocol: [`SPEC.md`](../../../components/esp_gmp/SPEC.md).

## Suggested tests

Basic protocol and OTA payload parsing are covered by `components/esp_gmp/test`. For the BLE OTA examples, cover in HIL or manual integration tests: `OS_CAP_QUERY`, small-image upload, disconnect abort, FINISH timeout boundaries, and the runtime skip path when `ota_image.bin` is missing.
