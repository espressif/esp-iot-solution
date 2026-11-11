# ChangeLog

## v0.2.0 - 2026-11-11

### Enhancements:

- Add `skip_profile_init` to `bq27220_config_t` for fast creation without battery profile verification or update.
- Add `bq27220_update_profile()` to explicitly verify the battery profile and update it only when the stored profile differs.
- Make `bq27220_seal()` return immediately when the gauge is already sealed and confirm SEALED by polling instead of waiting a fixed time. `CONFIG_BQ27220_SEAL_SETTLE_MS` is now the maximum wait, which removes about 2 s from profile checks and updates.
- Replace the fixed 1 s wait after entering FULL ACCESS and the fixed 2 s wait after `EXIT_CFG_UPDATE_REINIT` with OperationStatus() polling. A profile update now takes about 1.3 s instead of 6.3 s; the unseal lockout after reinitialization is only paid by a later `bq27220_unseal()` that needs it.

### Bug Fixes:

- Leave CONFIG UPDATE mode and restore the sealed state when a battery profile update fails.
- Retry the device probe in `bq27220_create()` for up to 500 ms so creation does not fail while the gauge is still within its 250 ms power-up communication delay.
- Limit OperationStatus() polling to two reads per second as required by the datasheet, and skip redundant status reads in profile checks.
- Retry `bq27220_unseal()` for up to 8 s because the gauge rejects unseal keys for up to about 5 s after a profile update. Previously, `bq27220_create()` could fail if the ESP restarted shortly after a profile update.

## v0.1.3 - 2026-10-10

### Bug Fixes:

- Include `esp_assert.h` explicitly for `ESP_STATIC_ASSERT` to avoid relying on transitive includes.

## v0.1.2 - 2026-09-07

### Bug Fixes:

- Validate the complete battery profile before skipping an update and correct the DOD50 and one-byte parameter writes.
- Use checked Data Memory transfers, targeted writes, and full readback verification with error propagation.
- Confirm CONFIG UPDATE transitions and device reinitialization, then restore the sealed state.
- Add ESP-IDF 6.x test-app compatibility and a manual repeated-restart regression test.

## v0.1.1 - 2025-9-22

### Bug Fix:

- Change dependency i2c_bus to public to fix compilation errors.

## v0.1.0 - 2025-08-12

### Enhancements:

* Initial version: Provide the basic functionality of the BQ27220 fuel gauge.
