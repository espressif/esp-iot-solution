# ChangeLog

## v0.0.3 - 2026-08-18

### Breaking Changes:

* Remove deprecated Flux adapter entry points `esp_gmp_flux_on_data_received()` and `esp_gmp_flux_on_session_complete()` from `esp_gmp_flux.h`. Use `esp_gmp_flux_get_gatt_callbacks()` (preferred) or `esp_gmp_flux_get_callbacks()` instead.

### Enhancements:

* Add File Transfer profile under `profiles/file_transfer/` with sender/receiver demos.
* Support multiple GMP handlers per `group_id`; OTA host self-registers GRP_OTA + GRP_OS.
* Emit `ESP_GMP_LINK_EVENT_MTU_CHANGED` from the Flux/GATT adapter.

## v0.0.2 - 2026-07-28

### Bug Fixes:

* Embed `psa_hash_operation_t` in `esp_gmp_sha256_ctx_t` instead of a fixed opaque buffer, fixing `_Static_assert` failures when `psa_hash_operation_t` grows with SHA-512 (e.g. ESP32-C3 defaults).
* Fix `components/esp_gmp/test` build discovery via `EXTRA_COMPONENT_DIRS` for `esp_gmp` / `esp_flux`, and enable NimBLE in `sdkconfig.defaults`.

### Enhancements:

* Add GitLab CI compile jobs for BLE GMP OTA client/server examples and the component unit test app (BLE-only targets).
* Add `idf_component.yml` for Component Manager metadata and dependencies.

## v0.0.1 - 2026-07-16

### Enhancements:

* Initial release of the ESP-GMP management protocol core (framing, CRC, multi-link TX).
* Add Flux/GATT adapter (`esp_gmp_flux_*`) for BLE stacks using `esp_flux`.
* Add optional OTA submodule with incremental SHA256 transfer integrity verify.
* Add BLE GMP OTA client/server examples and component unit tests.
