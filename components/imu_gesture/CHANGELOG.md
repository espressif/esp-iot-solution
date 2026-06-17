# ChangeLog

## v0.1.0 - 2026-05-15

### Enhancements:

1. Added the initial `imu_gesture` ESP-IDF component skeleton.
2. Migrated runtime knob and space-switch gesture detection out of the airmouse business layer.
3. Refactored runtime gesture logic into an opaque handle plus public config and event APIs.
4. Added callback-based runtime event dispatch APIs.
5. Kept gesture inference in the application layer so the component stays independent from `predict/model`.
6. Generalized knob and space-switch detectors with configurable axes and direction signs.

### API Changes:

1. Added initial `imu_gesture_*` API set.
2. Added public event, sample, config, callback, and state query types.
3. Exposed `axis_angle_rad`, configurable knob axis/sign, and configurable horizontal/vertical space-switch axes.
4. Removed the unused `space_switch.confirm_ms` config entry.
5. Removed internal detector-window gating from the component so space-switch validity can be decided in the application layer.

### Testing:

1. No standalone component tests are included in this repository snapshot yet.
