# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
"""Panthera host FK/IK — same DH table as `components/kinematic`."""

from __future__ import annotations

import argparse
import sys

import numpy as np
from roboticstoolbox import DHRobot, RevoluteDH
from spatialmath import SE3

WORLD_DOWN = np.array([0.0, 0.0, -1.0])

# Firmware grasp / goto_position seed (kinematic frame, degrees).
GRASP_SEED_DEG = np.array([0.0, 30.0, -36.0, 65.0, 0.0, 0.0])


def make_robot() -> DHRobot:
    return DHRobot(
        [
            RevoluteDH(a=0.0, d=0.1005, alpha=-np.pi / 2, offset=0.0),
            RevoluteDH(a=0.18, d=0.0, alpha=0.0, offset=np.deg2rad(180)),
            RevoluteDH(a=0.188809, d=0.0, alpha=0.0, offset=np.deg2rad(162.429)),
            RevoluteDH(a=0.08, d=0.0, alpha=-np.pi / 2, offset=np.deg2rad(17.5715)),
            RevoluteDH(a=0.0, d=0.0, alpha=np.pi / 2, offset=np.deg2rad(90)),
            RevoluteDH(a=0.0, d=0.184, alpha=np.pi / 2, offset=np.deg2rad(-90)),
        ],
        name='Panther6DOF',
    )


def joints_deg_to_rad(joints_deg: list[float] | np.ndarray) -> np.ndarray:
    return np.deg2rad(np.asarray(joints_deg, dtype=float))


def print_joints(label: str, q_rad: np.ndarray) -> None:
    q_deg = np.rad2deg(q_rad)
    print(f'{label}:')
    for i, (rad, deg) in enumerate(zip(q_rad, q_deg), start=1):
        print(f'  J{i}: {rad:8.4f} rad ({deg:7.2f} deg)')


def print_pose(label: str, T: SE3) -> None:
    p = T.t
    R = np.asarray(T.R)
    rpy_deg = T.rpy(unit='deg', order='zyx')
    axes = {
        '+X': R[:, 0],
        '+Y': R[:, 1],
        '+Z': R[:, 2],
        '-X': -R[:, 0],
        '-Y': -R[:, 1],
        '-Z': -R[:, 2],
    }
    best_name, best_axis = max(axes.items(), key=lambda kv: float(np.dot(kv[1], WORLD_DOWN)))
    best_align_deg = float(np.degrees(np.arccos(np.clip(np.dot(best_axis, WORLD_DOWN), -1.0, 1.0))))

    print(f'\n=== {label} ===')
    print(f'position xyz (m):  [{p[0]: .6f}, {p[1]: .6f}, {p[2]: .6f}]')
    print(f'RPY zyx (deg):     [{rpy_deg[0]: .3f}, {rpy_deg[1]: .3f}, {rpy_deg[2]: .3f}]')
    print('tool axes in base (columns of R):')
    print(f'  x_tool: [{R[0, 0]: .4f}, {R[1, 0]: .4f}, {R[2, 0]: .4f}]')
    print(f'  y_tool: [{R[0, 1]: .4f}, {R[1, 1]: .4f}, {R[2, 1]: .4f}]')
    print(f'  z_tool: [{R[0, 2]: .4f}, {R[1, 2]: .4f}, {R[2, 2]: .4f}]')
    print(
        f'best tabletop approach vs world -Z: tool {best_name} '
        f'(angle to down = {best_align_deg:.1f} deg)'
    )
    print('T =')
    print(np.array_str(np.asarray(T.A), precision=4, suppress_small=True))


def cmd_fk(args: argparse.Namespace) -> int:
    robot = make_robot()
    q = joints_deg_to_rad(args.joints if args.joints is not None else GRASP_SEED_DEG)
    label = (
        f'FK joints {np.asarray(args.joints, dtype=float).tolist()} deg'
        if args.joints is not None
        else f'FK grasp seed {GRASP_SEED_DEG.tolist()} deg'
    )
    T = robot.fkine(q)
    print_joints('input joints (kinematic frame)', q)
    print_pose(label, T)

    if args.plot:
        robot.plot(q, backend='pyplot', block=True)
    return 0


def build_target(robot: DHRobot, args: argparse.Namespace, seed_rad: np.ndarray) -> SE3:
    """Match firmware: FK(seed) then replace xyz; optional RPY override."""
    target = robot.fkine(seed_rad)
    target.t = [args.x, args.y, args.z]
    if args.rpy is not None:
        rpy = np.deg2rad(np.asarray(args.rpy, dtype=float))
        target = SE3.Trans(args.x, args.y, args.z) * SE3.RPY(rpy, order='zyx')
    return target


def cmd_ik(args: argparse.Namespace) -> int:
    robot = make_robot()
    seed = joints_deg_to_rad(args.seed if args.seed is not None else GRASP_SEED_DEG)
    target = build_target(robot, args, seed)

    print_joints('IK seed (kinematic frame)', seed)
    print_pose('IK target', target)

    # Levenberg–Marquardt numerical IK (host reference; firmware uses DLS).
    sol = robot.ikine_LM(target, q0=seed, joint_limits=True)
    if not sol.success:
        print(f'\nIK failed: {sol.reason}', file=sys.stderr)
        if sol.q is not None:
            print_joints('last joints', sol.q)
            print_pose('FK(last)', robot.fkine(sol.q))
        return 1

    q = np.asarray(sol.q, dtype=float).reshape(6)
    reached = robot.fkine(q)
    pos_err = float(np.linalg.norm(reached.t - target.t))
    # Rotation error via angle of R_err = R_reached^T * R_target
    R_err = np.asarray(reached.R).T @ np.asarray(target.R)
    rot_err = float(np.arccos(np.clip((np.trace(R_err) - 1.0) * 0.5, -1.0, 1.0)))

    print(f'\nIK ok (iterations={sol.iterations}, pos_err={pos_err:.5f} m, rot_err={rot_err:.5f} rad)')
    print_joints('IK joints (kinematic frame)', q)
    print('Motor command frame (J3 inverted on bus, same as firmware):')
    for i, rad in enumerate(q):
        cmd = -rad if i == 2 else rad
        print(f'  0x{0x11 + i:02X}: {cmd:8.4f} rad ({np.rad2deg(cmd):7.2f} deg)')
    print_pose('FK(IK joints)', reached)

    if args.plot:
        robot.plot(q, backend='pyplot', block=True)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description='Panthera host FK/IK (roboticstoolbox, kinematic frame)',
    )
    sub = parser.add_subparsers(dest='command', required=True)

    fk = sub.add_parser('fk', help='forward kinematics from joint angles (deg)')
    fk.add_argument(
        '-q',
        '--joints',
        nargs=6,
        type=float,
        metavar=('J1', 'J2', 'J3', 'J4', 'J5', 'J6'),
        help=f'joint angles in degrees (default: grasp seed {GRASP_SEED_DEG.tolist()})',
    )
    fk.add_argument('--plot', action='store_true', help='plot the posture')
    fk.set_defaults(func=cmd_fk)

    ik = sub.add_parser(
        'ik',
        help='inverse kinematics to xyz; keeps seed orientation unless --rpy is set',
    )
    ik.add_argument('-x', type=float, required=True, help='target x (m)')
    ik.add_argument('-y', type=float, required=True, help='target y (m)')
    ik.add_argument('-z', type=float, required=True, help='target z (m)')
    ik.add_argument(
        '--seed',
        nargs=6,
        type=float,
        metavar=('J1', 'J2', 'J3', 'J4', 'J5', 'J6'),
        help=f'IK seed in degrees (default: {GRASP_SEED_DEG.tolist()})',
    )
    ik.add_argument(
        '--rpy',
        nargs=3,
        type=float,
        metavar=('R', 'P', 'Y'),
        help='optional target RPY zyx in degrees (default: seed orientation)',
    )
    ik.add_argument('--plot', action='store_true', help='plot the solution')
    ik.set_defaults(func=cmd_ik)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == '__main__':
    raise SystemExit(main())
