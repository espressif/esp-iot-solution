# Changelog

## [1.0.1] - 2026-10-16

### Added
- Target: esp32p4

## [1.0.0] - 2026-07-14

### Added
- Controller-side network provisioning client for Wi-Fi and Thread
- Endpoints compatible with `espressif/network_provisioning`: config / scan / ctrl
- High-level `network_provisioner_provision_wifi()` / `provision_thread()`
- Runtime capability parsing from `proto-ver` (`prov.cap` / `prov.sec_ver`)
- Host unit tests for capabilities parsing and protobuf encode/decode
- Example under `examples/network_provisioner` (SoftAP, BLE, Thread)

### Fixed
- Parse `cap` / `sec_ver` only inside the `prov` JSON object; require `sec_ver` for structured responses
- Cap Wi-Fi / Thread scan `count` and `result_count` at `NETWORK_PROVISIONER_MAX_SCAN_RESULTS`
- Pin `protocomm_ext` with registry `version` plus in-tree `override_path`
