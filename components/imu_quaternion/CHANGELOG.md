# ChangeLog

## v0.1.0 - 2026-05-15

### Enhancements:

1. Initial local component skeleton.
2. Migrated quaternion initialization, runtime update, bias learning, and gyro guard logic into the local component.
3. Refactored the component into a clearer ESP-IDF style layout with public headers, private headers, and multiple source modules.
4. Split private runtime state into `pose`, `bias`, `gyro_guard`, and `runtime_gain` internal sub-states.
5. Generalized alignment initialization into configurable primary accel target axis plus secondary heading target axis constraints.
6. Removed hard-coded world-Z heading initialization from the solver core and switched to configurable axis targets plus `heading_ref_body`.
7. Added an internal warmup phase for quaternion convergence after initialization or reinitialization.
8. Expanded component README with feature description, API introduction, and placeholder sections for registry badge and example link.

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

## Unreleased

### API Changes:

1. Simplified `imu_quat_config_t` to use flat `gyro_bias_enabled`, `gyro_guard_enabled`, and `gyro_guard_limit_dps` fields.
2. Moved detailed bias-learning and gyro-guard recovery tuning parameters to internal implementation constants.
3. Removed the accel-heading initialization fallback path; failure now returns `ESP_ERR_INVALID_STATE` directly.
4. Renamed the startup identity init strategy to `IMU_QUAT_INIT_CURRENT_POSE` to match the AirMouse startup-pose semantics.

### Testing:

1. Added Unity test coverage for default config, custom heading reference, bias-disable behavior, gyro-guard-disable behavior, default warmup runtime state, and quaternion normalization smoke checks.
2. Updated test entry to run automatically without Unity menu interaction.

### Bug Fixes:
