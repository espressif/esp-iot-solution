/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Static gravity torque G(q) for Ragtime Panthera Zero Gravity (MIT feedforward).
 *
 * Source: float port of Ragtime_Panthera ROS `6Dof_dyna.h` (no Eigen). Same mass /
 * CoM / link-offset tables as that demo and the accompanying URDF inertials.
 *
 * Algorithm (static recursive Newton–Euler):
 *   1) Apply DH-style joint angle offsets to motor bus angles q[].
 *   2) Build link-to-link rotations R_{i-1,i}; propagate base "gravity accel"
 *      (0,0,+9.8) into each link frame (D'Alembert form used by the ROS code).
 *   3) F_i = m_i * a_i  (static simplification: use link-frame accel as CoM accel).
 *   4) Backward recursion: accumulate force f_i and moment n_i from tip to base.
 *   5) tau_i = n_i · z_i  (joint axis = local Z), then per-joint scale.
 *
 * Units: q[rad], tau[N·m], lengths[m], mass[kg]. Frame = motor bus (no J3 flip).
 */
#include "gravity_comp.h"
#include <cmath>

namespace {

constexpr float PI = static_cast<float>(M_PI);

/*
 * Link masses m_i [kg] = panther_description.urdf <link><inertial><mass>.
 * Used as F_i = m_i * a_i.
 *
 *   [0] link1 0.65    [1] link2 0.83    [2] link3 0.87
 *   [3] link4 0.427   [4] link5 0.39    [5] link6 0.41
 *
 * gripper_centor_link / gripper_1_link / gripper_2_link mass are 0 in that URDF,
 * so the physical DMH3510 gripper is NOT in this table yet. Add its weight to
 * [5] (and shift the link6 CoM outward) once it is measured. Until then expect
 * J4/J5 to sag slightly with the gripper mounted.
 */
constexpr float MASS_FULL[6] = {0.65f, 0.83f, 0.87f, 0.427f, 0.39f, 0.41f};

/*
 * Scale applied only to URDF distal bodies: link5 and link6(+gripper) masses.
 *
 * 1.0 = full URDF model (wrist J5/J6 and gripper mounted, the normal case).
 * Lower it only for partial-arm bring-up without the wrist:
 *   0.0 → under-compensate → joint3/joint4 (link4 side) tend to sag
 *   ~0.45 was used while J5/J6/gripper were off the bus
 */
constexpr float k_distal_mass_scale = 1.0f;

/*
 * Multiplier on solved gravity torque: tau[i] *= scale[i].
 * Order: joint1..joint6 (motor bus). Prefer mass tuning first; use for trim
 * (e.g. raise [1] if joint2 sags when the arm folds backward).
 * [1]=1.2 was found on the J1-J4 arm; re-check with the full arm mounted.
 */
constexpr float k_joint_gravity_scale[6] = {1.0f, 1.2f, 1.0f, 1.0f, 1.0f, 1.0f};

/** Effective mass for link1..link6 index (0..5); distal scale on link5/link6. */
static float link_mass(int i)
{
    if (i < 4) {
        return MASS_FULL[i];
    }
    return MASS_FULL[i] * k_distal_mass_scale;
}

/*
 * NEXT_LINK_ORIGIN_OFFSETS[i] = {x, y, z}, in metres.
 *
 * For i = 1..5, NEXT_LINK_ORIGIN_OFFSETS[i] points from the origin of link i
 * to the origin of link i+1, expressed in link i's coordinate frame.
 * It is a relative displacement, not a joint angle, absolute position, or
 * link centre of mass (the centre-of-mass offsets are stored below).
 *
 * Example: NEXT_LINK_ORIGIN_OFFSETS[2] = {0.18, 0, 0} means that the link3
 * origin is 0.18 m from the link2 origin along link2's +X axis.
 *
 * In the backward recursion it is the lever arm in moment = arm × force:
 *   n_i += NEXT_LINK_ORIGIN_OFFSETS[i] × (R_i,i+1 * f_i+1)
 *
 * URDF rpy is not stored here; the R_* rotation chain handles orientation.
 * The base_link→link1 offset (0, 0, 0.1005) is not needed because the
 * gravity recursion calculates moments about the link1 origin.
 *
 * Previously used 6Dof_dyna values ([4] y=0.011, [5] y=-0.040); switched to
 * URDF xyz for bring-up comparison.
 */
constexpr float NEXT_LINK_ORIGIN_OFFSETS[6][3] = {
    {0.0f, 0.0f, 0.0f},        // [0]: unused padding from the ROS 1-based table
    {0.0f, 0.0f, 0.0f},        // [1]: link2 origin relative to link1 origin, in link1 frame
    {0.18f, 0.0f, 0.0f},       // [2]: link3 origin relative to link2 origin, in link2 frame
    {0.18881f, 0.0f, 0.0f},    // [3]: link4 origin relative to link3 origin, in link3 frame
    {0.08f, 0.0f, 0.0f},       // [4]: link5 origin relative to link4 origin, in link4 frame
    {0.0f, -0.036f, 0.0f},     // [5]: link6 origin relative to link5 origin, in link5 frame
};

/*
 * Center of mass [m] in each link frame (= URDF <link><inertial><origin xyz/>).
 * Used as n += p_com × F for that link.
 *
 *   [0] link1  [1] link2  [2] link3  [3] link4  [4] link5
 *   [5] link6 (+ gripper mass lumped; URDF also has gripper_centor_link, etc.)
 */
constexpr float LINK_CENTER_OF_MASS_OFFSETS[6][3] = {
    {0.0f, -0.008491f, 0.034259f},   // link1 inertial origin
    {0.124560f, 0.0f, 0.017699f},    // link2 inertial origin
    {0.15f, 0.007521f, 0.017264f},   // link3 inertial origin
    {0.08f, -0.053910f, 0.004474f},  // link4 inertial origin
    {0.013f, 0.0f, -0.007913f},      // link5 inertial origin
    {0.000018f, -0.000159f, 0.09f},  // link6 (+ gripper) inertial origin
};

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3 &o) const
    {
        return Vec3(x + o.x, y + o.y, z + o.z);
    }

    Vec3 operator*(float s) const
    {
        return Vec3(x * s, y * s, z * s);
    }

    Vec3 cross(const Vec3 &o) const
    {
        return Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
    }

    float dot(const Vec3 &o) const
    {
        return x * o.x + y * o.y + z * o.z;
    }
};

struct Mat3 {
    float m[3][3];

    Vec3 mul(const Vec3 &v) const
    {
        return Vec3(m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                    m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                    m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z);
    }

    Mat3 transpose() const
    {
        Mat3 t;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                t.m[i][j] = m[j][i];
            }
        }
        return t;
    }
};

static Mat3 rot_z(float q)
{
    const float c = cosf(q);
    const float s = sinf(q);
    Mat3 R{};
    R.m[0][0] = c;
    R.m[0][1] = -s;
    R.m[0][2] = 0;
    R.m[1][0] = s;
    R.m[1][1] = c;
    R.m[1][2] = 0;
    R.m[2][0] = 0;
    R.m[2][1] = 0;
    R.m[2][2] = 1;
    return R;
}

} // namespace

void compute_gravity_compensation(const float q[6], float tau[6])
{
    /*
     * q[] is the measured motor-bus angle. q_offset[] is the angle used by this
     * dynamics frame convention after accounting for the fixed orientation of
     * each joint frame at motor zero. These offsets are coordinate transforms,
     * not extra motor commands or gravity-tuning values.
     *
     * Side view of the model when all motor angles q[] are zero (not to scale):
     *
     *        +Z
     *         ^                    J4 ---- 80 mm ---- J5 -- 36 mm -- J6
     *         |                   /
     *         |          188.81 mm
     *         |                 /
     *         |   J3 <--- 180 mm --- J1/J2
     *         +----------------------------------------------------> +X
     *
     * Approximate joint coordinates in this view [mm]:
     *   J1/J2 (0, 100.5), J3 (-180, 100.5), J4 (0, 157.5),
     *   J5 (80, 157.5), J6 (116, 157.5).
     * Thus q[]=0 is a folded zero pose, not a fully straight horizontal arm.
     * The 17.57 deg geometry angle is atan2(57, 180); 162.43 deg is its
     * supplementary angle. The signs below also depend on how each hand-written
     * R matrix factors the URDF fixed rotation, so they are not interchangeable
     * with the standard-DH offsets in kinematic.cpp.
     */
    float q_offset[6];
    q_offset[0] = q[0];                                   // J1: motor zero matches model zero
    q_offset[1] = q[1] + PI;                              // J2: link frame is reversed by 180 deg
    q_offset[2] = q[2] - 162.43f * PI / 180.0f;           // J3: fixed diagonal-link frame rotation
    q_offset[3] = q[3] + 17.57f * PI / 180.0f;            // J4: complementary diagonal-link angle
    q_offset[4] = q[4] + PI / 2.0f;                       // J5: wrist frames differ by 90 deg
    q_offset[5] = q[5];                                   // J6: fixed twist is in R_link5_from_link6

    // R_target_from_source: express a source-frame vector in the target frame.
    Mat3 R_base_from_link1 = rot_z(q_offset[0]);

    Mat3 R_link1_from_link2{};
    {
        const float c = cosf(q_offset[1]);
        const float s = sinf(q_offset[1]);
        R_link1_from_link2.m[0][0] = c;
        R_link1_from_link2.m[0][1] = -s;
        R_link1_from_link2.m[0][2] = 0;
        R_link1_from_link2.m[1][0] = 0;
        R_link1_from_link2.m[1][1] = 0;
        R_link1_from_link2.m[1][2] = 1;
        R_link1_from_link2.m[2][0] = -s;
        R_link1_from_link2.m[2][1] = -c;
        R_link1_from_link2.m[2][2] = 0;
    }

    Mat3 R_link2_from_link3{};
    {
        const float c = cosf(q_offset[2]);
        const float s = sinf(q_offset[2]);
        R_link2_from_link3.m[0][0] = c;
        R_link2_from_link3.m[0][1] = -s;
        R_link2_from_link3.m[0][2] = 0;
        R_link2_from_link3.m[1][0] = -s;
        R_link2_from_link3.m[1][1] = -c;
        R_link2_from_link3.m[1][2] = 0;
        R_link2_from_link3.m[2][0] = 0;
        R_link2_from_link3.m[2][1] = 0;
        R_link2_from_link3.m[2][2] = -1;
    }

    Mat3 R_link3_from_link4{};
    {
        const float c = cosf(q_offset[3]);
        const float s = sinf(q_offset[3]);
        R_link3_from_link4.m[0][0] = c;
        R_link3_from_link4.m[0][1] = -s;
        R_link3_from_link4.m[0][2] = 0;
        R_link3_from_link4.m[1][0] = -s;
        R_link3_from_link4.m[1][1] = -c;
        R_link3_from_link4.m[1][2] = 0;
        R_link3_from_link4.m[2][0] = 0;
        R_link3_from_link4.m[2][1] = 0;
        R_link3_from_link4.m[2][2] = -1;
    }

    Mat3 R_link4_from_link5{};
    {
        const float c = cosf(q_offset[4]);
        const float s = sinf(q_offset[4]);
        R_link4_from_link5.m[0][0] = c;
        R_link4_from_link5.m[0][1] = -s;
        R_link4_from_link5.m[0][2] = 0;
        R_link4_from_link5.m[1][0] = 0;
        R_link4_from_link5.m[1][1] = 0;
        R_link4_from_link5.m[1][2] = 1;
        R_link4_from_link5.m[2][0] = -s;
        R_link4_from_link5.m[2][1] = -c;
        R_link4_from_link5.m[2][2] = 0;
    }

    Mat3 R_link5_from_link6{};
    {
        const float c = cosf(q_offset[5]);
        const float s = sinf(q_offset[5]);
        R_link5_from_link6.m[0][0] = c;
        R_link5_from_link6.m[0][1] = -s;
        R_link5_from_link6.m[0][2] = 0;
        R_link5_from_link6.m[1][0] = 0;
        R_link5_from_link6.m[1][1] = 0;
        R_link5_from_link6.m[1][2] = -1;
        R_link5_from_link6.m[2][0] = s;
        R_link5_from_link6.m[2][1] = c;
        R_link5_from_link6.m[2][2] = 0;
    }

    // The transpose reverses the conversion direction because R^-1 = R^T.
    Mat3 R_link1_from_base = R_base_from_link1.transpose();
    Mat3 R_link2_from_link1 = R_link1_from_link2.transpose();
    Mat3 R_link3_from_link2 = R_link2_from_link3.transpose();
    Mat3 R_link4_from_link3 = R_link3_from_link4.transpose();
    Mat3 R_link5_from_link4 = R_link4_from_link5.transpose();
    Mat3 R_link6_from_link5 = R_link5_from_link6.transpose();

    // Gravity-equivalent acceleration in the base frame (D'Alembert sign: +Z).
    const Vec3 gravity_accel_in_base(0.0f, 0.0f, 9.8f);

    // Express the same gravity vector in each link frame.
    Vec3 gravity_accel_in_link1 = R_link1_from_base.mul(gravity_accel_in_base);
    Vec3 gravity_accel_in_link2 = R_link2_from_link1.mul(gravity_accel_in_link1);
    Vec3 gravity_accel_in_link3 = R_link3_from_link2.mul(gravity_accel_in_link2);
    Vec3 gravity_accel_in_link4 = R_link4_from_link3.mul(gravity_accel_in_link3);
    Vec3 gravity_accel_in_link5 = R_link5_from_link4.mul(gravity_accel_in_link4);
    Vec3 gravity_accel_in_link6 = R_link6_from_link5.mul(gravity_accel_in_link5);

    // Gravity force generated by each link's own mass, expressed in that link frame.
    Vec3 own_gravity_force_in_link1 = gravity_accel_in_link1 * link_mass(0);
    Vec3 own_gravity_force_in_link2 = gravity_accel_in_link2 * link_mass(1);
    Vec3 own_gravity_force_in_link3 = gravity_accel_in_link3 * link_mass(2);
    Vec3 own_gravity_force_in_link4 = gravity_accel_in_link4 * link_mass(3);
    Vec3 own_gravity_force_in_link5 = gravity_accel_in_link5 * link_mass(4);
    Vec3 own_gravity_force_in_link6 = gravity_accel_in_link6 * link_mass(5);

    // A total force in linkN includes linkN and every distal link through link6,
    // expressed in the linkN frame. Example: the link3 total contains links3..6.
    // Accumulate these forces from the tip toward the base.
    Vec3 total_gravity_force_in_link6 = own_gravity_force_in_link6;
    Vec3 total_gravity_force_in_link5 = R_link5_from_link6.mul(total_gravity_force_in_link6) + own_gravity_force_in_link5;
    Vec3 total_gravity_force_in_link4 = R_link4_from_link5.mul(total_gravity_force_in_link5) + own_gravity_force_in_link4;
    Vec3 total_gravity_force_in_link3 = R_link3_from_link4.mul(total_gravity_force_in_link4) + own_gravity_force_in_link3;
    Vec3 total_gravity_force_in_link2 = R_link2_from_link3.mul(total_gravity_force_in_link3) + own_gravity_force_in_link2;

    // Centre-of-mass offsets of URDF link1..link6.
    const Vec3 center_of_mass_offset_in_link1(LINK_CENTER_OF_MASS_OFFSETS[0][0], LINK_CENTER_OF_MASS_OFFSETS[0][1], LINK_CENTER_OF_MASS_OFFSETS[0][2]);
    const Vec3 center_of_mass_offset_in_link2(LINK_CENTER_OF_MASS_OFFSETS[1][0], LINK_CENTER_OF_MASS_OFFSETS[1][1], LINK_CENTER_OF_MASS_OFFSETS[1][2]);
    const Vec3 center_of_mass_offset_in_link3(LINK_CENTER_OF_MASS_OFFSETS[2][0], LINK_CENTER_OF_MASS_OFFSETS[2][1], LINK_CENTER_OF_MASS_OFFSETS[2][2]);
    const Vec3 center_of_mass_offset_in_link4(LINK_CENTER_OF_MASS_OFFSETS[3][0], LINK_CENTER_OF_MASS_OFFSETS[3][1], LINK_CENTER_OF_MASS_OFFSETS[3][2]);
    const Vec3 center_of_mass_offset_in_link5(LINK_CENTER_OF_MASS_OFFSETS[4][0], LINK_CENTER_OF_MASS_OFFSETS[4][1], LINK_CENTER_OF_MASS_OFFSETS[4][2]);
    const Vec3 center_of_mass_offset_in_link6(LINK_CENTER_OF_MASS_OFFSETS[5][0], LINK_CENTER_OF_MASS_OFFSETS[5][1], LINK_CENTER_OF_MASS_OFFSETS[5][2]);

    // Offset of each link origin from the preceding link origin.
    const Vec3 link2_origin_offset_in_link1(NEXT_LINK_ORIGIN_OFFSETS[1][0], NEXT_LINK_ORIGIN_OFFSETS[1][1], NEXT_LINK_ORIGIN_OFFSETS[1][2]);
    const Vec3 link3_origin_offset_in_link2(NEXT_LINK_ORIGIN_OFFSETS[2][0], NEXT_LINK_ORIGIN_OFFSETS[2][1], NEXT_LINK_ORIGIN_OFFSETS[2][2]);
    const Vec3 link4_origin_offset_in_link3(NEXT_LINK_ORIGIN_OFFSETS[3][0], NEXT_LINK_ORIGIN_OFFSETS[3][1], NEXT_LINK_ORIGIN_OFFSETS[3][2]);
    const Vec3 link5_origin_offset_in_link4(NEXT_LINK_ORIGIN_OFFSETS[4][0], NEXT_LINK_ORIGIN_OFFSETS[4][1], NEXT_LINK_ORIGIN_OFFSETS[4][2]);
    const Vec3 link6_origin_offset_in_link5(NEXT_LINK_ORIGIN_OFFSETS[5][0], NEXT_LINK_ORIGIN_OFFSETS[5][1], NEXT_LINK_ORIGIN_OFFSETS[5][2]);

    // A total moment in linkN includes linkN and every distal link through link6.
    // It is calculated about the linkN origin and expressed in the linkN frame.
    // Example: total_gravity_moment_in_link3 contains links3, 4, 5 and 6.
    // Backward recursion: transformed distal moment + own weight moment
    //                     + moment created by the distal force at the next-link offset.
    Vec3 total_gravity_moment_in_link6 = center_of_mass_offset_in_link6.cross(own_gravity_force_in_link6);
    Vec3 total_gravity_moment_in_link5 = R_link5_from_link6.mul(total_gravity_moment_in_link6) + center_of_mass_offset_in_link5.cross(own_gravity_force_in_link5) + link6_origin_offset_in_link5.cross(R_link5_from_link6.mul(total_gravity_force_in_link6));
    Vec3 total_gravity_moment_in_link4 = R_link4_from_link5.mul(total_gravity_moment_in_link5) + center_of_mass_offset_in_link4.cross(own_gravity_force_in_link4) + link5_origin_offset_in_link4.cross(R_link4_from_link5.mul(total_gravity_force_in_link5));
    Vec3 total_gravity_moment_in_link3 = R_link3_from_link4.mul(total_gravity_moment_in_link4) + center_of_mass_offset_in_link3.cross(own_gravity_force_in_link3) + link4_origin_offset_in_link3.cross(R_link3_from_link4.mul(total_gravity_force_in_link4));
    Vec3 total_gravity_moment_in_link2 = R_link2_from_link3.mul(total_gravity_moment_in_link3) + center_of_mass_offset_in_link2.cross(own_gravity_force_in_link2) + link3_origin_offset_in_link2.cross(R_link2_from_link3.mul(total_gravity_force_in_link3));
    Vec3 total_gravity_moment_in_link1 = R_link1_from_link2.mul(total_gravity_moment_in_link2) + center_of_mass_offset_in_link1.cross(own_gravity_force_in_link1) + link2_origin_offset_in_link1.cross(R_link1_from_link2.mul(total_gravity_force_in_link2));

    // Torque about each joint axis (local Z), then optional per-joint trim.
    const Vec3 z_axis(0.0f, 0.0f, 1.0f);
    tau[5] = total_gravity_moment_in_link6.dot(z_axis) * k_joint_gravity_scale[5]; // joint6
    tau[4] = total_gravity_moment_in_link5.dot(z_axis) * k_joint_gravity_scale[4]; // joint5
    tau[3] = total_gravity_moment_in_link4.dot(z_axis) * k_joint_gravity_scale[3]; // joint4
    tau[2] = total_gravity_moment_in_link3.dot(z_axis) * k_joint_gravity_scale[2]; // joint3
    tau[1] = total_gravity_moment_in_link2.dot(z_axis) * k_joint_gravity_scale[1]; // joint2
    tau[0] = total_gravity_moment_in_link1.dot(z_axis) * k_joint_gravity_scale[0]; // joint1
}
