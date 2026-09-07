# ChangeLog

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
