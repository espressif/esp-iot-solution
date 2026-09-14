# Panthera Host Simulation

Host-side **FK / IK** for the Ragtime Panthera 6-DOF arm using [roboticstoolbox](https://github.com/petercorke/robotics-toolbox-python). The DH table matches `components/kinematic` and the parent [follower README](../README.md).

> Use a clean shell. Do **not** mix with an ESP-IDF `export.sh` session.

## Setup

Requires [uv](https://github.com/astral-sh/uv).

```bash
uv sync
```

Joint angles are in the **kinematic frame** (same as on-device `Kinematic`). Motor bus joint 3 (`0x13`) is inverted only in firmware TX/RX; `ik` prints both frames.

## Forward kinematics

```bash
# Default: firmware grasp seed
uv run python main.py fk

# Custom joints (degrees)
uv run python main.py fk -q 0 30 -36 65 0 0

uv run python main.py fk -q 0 0 0 0 0 0 --plot
```

Prints xyz, RPY (zyx), tool axes in the base frame, alignment of each tool axis to world **−Z** (tabletop down), and `T`.

## Inverse kinematics

Mirrors firmware Cartesian targets: `T = FK(seed)`, then replace **xyz** only (seed orientation kept). Host solver is Levenberg–Marquardt (`ikine_LM`); on-device uses damped least squares — solutions should be close, not bit-identical.

```bash
# Same style as panthera_goto_position -x -y -z
uv run python main.py ik -x 0.32 -y 0.0 -z 0.045

# Custom seed / explicit orientation (RPY zyx, degrees)
uv run python main.py ik -x 0.25 -y 0.1 -z 0.015 --seed 0 30 -36 65 0 0
uv run python main.py ik -x 0.25 -y 0.0 -z 0.02 --rpy -90 -90 0 --plot
```

On success: kinematic joints, motor-command joints (J3 negated), achieved pose and position/rotation errors.

## DH Parameters

| Joint | a (m) | d (m) | alpha | offset |
|-------|-------|-------|-------|--------|
| J1 | 0 | 0.1005 | −π/2 | 0 |
| J2 | 0.18 | 0 | 0 | π |
| J3 | 0.188809 | 0 | 0 | 162.429° |
| J4 | 0.08 | 0 | −π/2 | 17.5715° |
| J5 | 0 | 0 | π/2 | π/2 |
| J6 | 0 | 0.184 | π/2 | −π/2 |

| Posture | Joints (deg) | xyz (m) |
|---------|--------------|---------|
| Zero | `(0, 0, 0, 0, 0, 0)` | ≈ `(0.264, 0, 0.1575)` |
| Grasp seed | `(0, 30, -36, 65, 0, 0)` | ≈ `(0.153, 0, 0.040)` |

## Choosing a tabletop grasp orientation

On-device `panthera_goto_position` / grasp lock the seed rotation `R` for every Cartesian point. The seed is both the IK guess and the orientation policy.

Practical workflow:

1. Identify which physical gripper axis is the approach direction (into the table/object).
2. Use `fk -q ...` until that tool axis reports a small angle to world −Z.
3. Prefer a reachable posture with suitable table height (`z`).
4. Promote that joint vector to the firmware seed; validate Cartesian points with `ik -x -y -z`.

Default seed `(0°, 30°, -36°, 65°, 0°, 0°)`: tool **+Y** is nearest to world −Z (~31° off vertical). Whether that matches the hardware depends on gripper mounting vs the DH tool frame.

## On-device counterparts

| Topic | Location |
|-------|----------|
| DH + FK/IK | `../components/kinematic/` |
| `panthera_goto_position` / `panthera_fk` | `../main/app_console.cpp` |
| Grasp Cartesian targets | `../main/app_manager.cpp` |
