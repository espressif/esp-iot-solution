# Changelog

## [1.0.1] - 2026-10-16

### Added
- Target: esp32p4

## [1.0.0] - 2026-07-21

### Added
- Controller-side client for IDF `esp_local_ctrl` property get/set
- Endpoints: `esp_local_ctrl/version`, `esp_local_ctrl/session`, `esp_local_ctrl/control`
- Runtime version JSON parsing (`local_ctrl.sec_ver` / `sec_patch_ver`)
- Host unit tests for version parsing and protobuf encode/decode
- Example under `examples/esp_local_controller` (HTTP, BLE, Console × Sec0/1/2)

### Fixed
- Treat empty GetPropertyValues (`n_props == 0`) as success instead of `ESP_ERR_NO_MEM`
- Share `constants.pb-c` / `Status` with `protocomm_ext` (avoid duplicate `status__descriptor`)
- Invalidate cached version on fetch / `start_session` failure
- Align `start_session` with fetch → establish order; document `set_security` prerequisite
- Parse version fields only inside the `local_ctrl` JSON object (ignore injected outer keys)
- Cap property count / `n_props` at `ESP_LOCAL_CONTROLLER_MAX_PROPERTIES`
