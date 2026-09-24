# ChangeLog

## v0.1.0 - 2026-7-3

First release version.

- Forward kinematics based on standard DH parameters, configurable via `RobotConfig`
- Inverse kinematics using damped least squares with backtracking line search
- `solve_ik()` takes an explicit seed and returns an `IKResult` (status, joints, achieved position/rotation errors, iterations)
- Per-joint limits enforced during iteration (defaults to [-π, π] until calibrated)
