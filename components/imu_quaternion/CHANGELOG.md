# ChangeLog

## v0.1.0 - 2026-05-15

### Enhancements:

1. Initial local component skeleton.
2. Migrated quaternion initialization, runtime update, bias learning, and gyro guard logic into the local component.
3. Refactored the component into a clearer ESP-IDF style layout with public
   headers, private headers, and multiple source modules.
4. Split private runtime state into `pose`, `bias`, `gyro_guard`, and `runtime_gain` internal sub-states.
5. Generalized alignment initialization into configurable primary accel target
   axis plus secondary heading target axis constraints.
6. Removed hard-coded world-Z heading initialization from the solver core and
   switched to configurable axis targets plus `heading_ref_body`.
7. Added an internal warmup phase for quaternion convergence after initialization or reinitialization.
8. Expanded component README with feature description, API introduction, and
   placeholder sections for registry badge and example link.
9. Added explicit magnetic-input sample fields to the public sample API and
   clarified that magnetic data is optional.
10. Unified initialization around gravity plus configured heading alignment and
   removed the previous implicit first-sample initialization path.
11. Added explicit sample-driven reinitialization support so applications can
   rebuild world frame deliberately after reset or recenter events.
12. Added initial magnetic-reference capture plus update-time magnetic yaw
   correction and accel/mag rejection tracking.
13. Added public startup gyro-bias setter/getter helpers so applications can
   inject or inspect a persisted initial bias estimate.
14. Normalized public and private header declarations/comments to match the
   repository header-style and ESP-IDF formatting expectations.

### API Changes:

1. Added initial `imu_quat_*` API set.
2. Added layered runtime config types, including:
   - `imu_quat_axis_t`
   - `accel_target_axis`
   - `gyro_bias_enabled`
   - `gyro_guard_enabled`
   - `gyro_guard_limit_dps`
   - `heading_ref_body[3]`
   - `heading_target_axis`
3. Simplified `imu_quat_config_t` to use flat `gyro_bias_enabled`,
   `gyro_guard_enabled`, and `gyro_guard_limit_dps` fields.
4. Moved detailed bias-learning and gyro-guard recovery tuning parameters to internal implementation constants.
5. Removed the old init-strategy split and kept one explicit gravity-heading
   initialization path.
6. Added `mag_valid`/`mag[3]` to `imu_quat_sample_t`.
7. Added `mag_input_enabled` to `imu_quat_config_t`.
8. Added `imu_quat_reinitialize_from_sample()`.
9. Added `imu_quat_set_gyro_bias_dps()` and `imu_quat_get_gyro_bias_dps()`.
10. `imu_quat_update()` now requires the solver to be explicitly initialized
   first and returns `ESP_ERR_INVALID_STATE` when called before reinitialization.

### Testing:

1. Unity test scaffolding remains present in the component, but this repository
   snapshot does not yet include a refreshed standalone test pass for the
   latest magnetic-input and explicit-reinit behavior.

### Bug Fixes:

1. Fixed solver reset semantics so reset clears runtime state without pretending
   that world frame has already been rebuilt.
2. Fixed guarded runtime reinitialization and application-driven recenter flows
   to share the same explicit reinitialization semantics.
3. Fixed magnetic initialization behavior so magnetic samples no longer redefine
   startup heading; they are now recorded as reference data after successful
   initialization.
