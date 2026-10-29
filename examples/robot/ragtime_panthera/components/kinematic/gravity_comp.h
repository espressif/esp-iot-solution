/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * Static gravity compensation G(q) for the Ragtime Panthera 6-DOF arm.
 *
 * Float port of Ragtime_Panthera ROS `6Dof_dyna.h` (recursive Newton–Euler,
 * static case). Used by Zero Gravity mode as MIT torque feedforward.
 *
 * Inputs / outputs are in the **motor bus frame** (same as ROS ZeroGraMode).
 * This module does **not** apply the firmware J3 (`0x13`) sign flip used by
 * Cartesian FK/IK.
 *
 * Model / tuning constants live in gravity_comp.cpp (URDF names from
 * panther_description.urdf: link1..link6, joint1..joint6, gripper_*):
 * - `MASS_FULL` — URDF <mass> of link1..link6
 * - `k_distal_mass_scale` — 1.0 for the full arm; lower only if wrist/gripper missing
 * - `k_joint_gravity_scale` — trim tau for joint1..joint6
 * - `NEXT_LINK_ORIGIN_OFFSETS` — origin of each link relative to the preceding link
 * - `LINK_CENTER_OF_MASS_OFFSETS` — centre of mass relative to each link origin
 */

/** Gravity torques G(q) in N·m for joints 0..5 (bus frame). */
void compute_gravity_compensation(const float q[6], float tau[6]);
