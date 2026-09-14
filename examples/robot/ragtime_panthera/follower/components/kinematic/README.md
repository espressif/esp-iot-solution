# 6DOF Kinematic

The local kinematic design of Ragtime Panthera is based on [Matrix_and_Robotics_on_STM32](https://github.com/SJTU-RoboMaster-Team/Matrix_and_Robotics_on_STM32).

In addition, you can use the Python library `roboticstoolbox` for online simulation and debugging of the robotic arm. The DH parameters of the robotic arm are as follows:

```python
robot = DHRobot(
    [
        RevoluteDH(a=0.0, d=0.1005, alpha=-np.pi / 2, offset=0.0),
        RevoluteDH(a=0.18, d=0.0, alpha=0.0, offset=np.deg2rad(180)),
        RevoluteDH(a=0.188809, d=0.0, alpha=0.0, offset=np.deg2rad(162.429)),
        RevoluteDH(a=0.08, d=0.0, alpha=-np.pi / 2, offset=np.deg2rad(17.5715)),
        RevoluteDH(a=0.0, d=0.0, alpha=np.pi / 2, offset=np.deg2rad(90)),
        RevoluteDH(a=0.0, d=0.184, alpha=np.pi / 2, offset=np.deg2rad(-90)),
    ],
    name="Ragtime_Panthera"
)
```

## Solver

Forward kinematics is the closed-form chain product of standard DH transforms. Inverse kinematics is a **numerical solution** (damped least squares, iterating from a seed posture) — there is no analytic closed form, so convergence depends on the seed and the target being reachable.

## Usage

Forward kinematics returns the end-effector pose; inverse kinematics takes the target pose plus a seed (initial guess, typically the current posture) and returns a result object that must be checked before use:

```cpp
Kinematic kinematic;  // DH parameters of the Ragtime Panthera arm by default

Joint seed = Joint(0.0, DEG2RAD(30.0), DEG2RAD(-36.0), DEG2RAD(65.0), 0.0, 0.0);
TransformMatrix target = kinematic.forward(seed);
target(0, 3) = 0.32f;  // override the position, keep the orientation

IKResult ik = kinematic.solve_ik(target, seed);
if (!ik) {
    // ik.status: NOT_CONVERGED / SINGULAR; ik.pos_error / ik.rot_error: achieved errors
    return;
}
// use ik.joints
```

Tolerances, iteration limit and the damping factor can be tuned per call via `IKOptions`. `RobotConfig` carries the DH table and per-joint limits, so alternative arms (or calibrated limits) can be injected through the `Kinematic(const RobotConfig &)` constructor.

## Gravity compensation

`gravity_comp.h` ports the static recursive Newton–Euler gravity torque from Ragtime Panthera ROS `6Dof_dyna.h` (no Eigen). Used by the follower Zero Gravity mode with MIT `tau` feedforward. Joint inputs/outputs are in the motor bus frame.
