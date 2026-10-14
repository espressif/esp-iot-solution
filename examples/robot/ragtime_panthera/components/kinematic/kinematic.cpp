/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <cstdio>
#include "kinematic.h"

TransformMatrix TransformMatrix::operator*(const TransformMatrix &other) const
{
    TransformMatrix result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result(i, j) = 0.0f;
            for (int k = 0; k < 4; k++) {
                result(i, j) += data[i][k] * other(k, j);
            }
        }
    }
    return result;
}

void TransformMatrix::print() const
{
    printf("Transform Matrix:\n");
    for (int i = 0; i < 4; i++) {
        printf("[");
        for (int j = 0; j < 4; j++) {
            printf("%8.4f", data[i][j]);
            if (j < 3) {
                printf(", ");
            }
        }
        printf("]\n");
    }
}

Position TransformMatrix::get_position() const
{
    return Position(data[0][3], data[1][3], data[2][3]);
}

TransformMatrix TransformMatrix::inverse() const
{
    TransformMatrix inv;

    float r00 = data[0][0], r01 = data[0][1], r02 = data[0][2];
    float r10 = data[1][0], r11 = data[1][1], r12 = data[1][2];
    float r20 = data[2][0], r21 = data[2][1], r22 = data[2][2];
    float tx = data[0][3], ty = data[1][3], tz = data[2][3];

    // For homogeneous transformation matrix:
    // T = [R  t]
    //     [0  1]
    // T^-1 = [R^T  -R^T*t]
    //        [0    1     ]

    inv(0, 0) = r00; inv(0, 1) = r10; inv(0, 2) = r20;
    inv(1, 0) = r01; inv(1, 1) = r11; inv(1, 2) = r21;
    inv(2, 0) = r02; inv(2, 1) = r12; inv(2, 2) = r22;

    inv(0, 3) = -(r00 * tx + r10 * ty + r20 * tz);
    inv(1, 3) = -(r01 * tx + r11 * ty + r21 * tz);
    inv(2, 3) = -(r02 * tx + r12 * ty + r22 * tz);

    inv(3, 0) = 0.0f; inv(3, 1) = 0.0f; inv(3, 2) = 0.0f; inv(3, 3) = 1.0f;

    return inv;
}

void TransformMatrix::get_rotation(float rotation[3][3]) const
{
    rotation[0][0] = data[0][0]; rotation[0][1] = data[0][1]; rotation[0][2] = data[0][2];
    rotation[1][0] = data[1][0]; rotation[1][1] = data[1][1]; rotation[1][2] = data[1][2];
    rotation[2][0] = data[2][0]; rotation[2][1] = data[2][1]; rotation[2][2] = data[2][2];
}

AxisAngle TransformMatrix::to_axis_angle() const
{
    float R[3][3];
    get_rotation(R);

    float trace = R[0][0] + R[1][1] + R[2][2];

    // theta = arccos(0.5 * (trace - 1))
    // [ux, uy, uz] = 1 / (2 * sin(theta)) * [R21 - R12, R02 - R20, R10 - R01]
    float cos_theta = 0.5f * (trace - 1.0f);
    if (cos_theta > 1.0f) {
        cos_theta = 1.0f;
    } else if (cos_theta < -1.0f) {
        cos_theta = -1.0f;
    }
    float theta = acosf(cos_theta);

    if (fabsf(theta) < 1e-6f) {
        return AxisAngle(1.0f, 0.0f, 0.0f, 0.0f);
    }

    if (fabsf(theta - M_PI) < 1e-6f) {
        float x = sqrtf(fmaxf(0.0f, (1.0f + R[0][0] - R[1][1] - R[2][2]) / 4.0f));
        float y = sqrtf(fmaxf(0.0f, (1.0f - R[0][0] + R[1][1] - R[2][2]) / 4.0f));
        float z = sqrtf(fmaxf(0.0f, (1.0f - R[0][0] - R[1][1] + R[2][2]) / 4.0f));

        if (x > y && x > z) {
            y = (R[0][1] + R[1][0]) / (4.0f * x);
            z = (R[0][2] + R[2][0]) / (4.0f * x);
        } else if (y > z) {
            x = (R[0][1] + R[1][0]) / (4.0f * y);
            z = (R[1][2] + R[2][1]) / (4.0f * y);
        } else {
            x = (R[0][2] + R[2][0]) / (4.0f * z);
            y = (R[1][2] + R[2][1]) / (4.0f * z);
        }
        return AxisAngle(x, y, z, theta);
    }

    float sin_theta = sinf(theta);
    float x = (R[2][1] - R[1][2]) / (2.0f * sin_theta);
    float y = (R[0][2] - R[2][0]) / (2.0f * sin_theta);
    float z = (R[1][0] - R[0][1]) / (2.0f * sin_theta);

    if (!std::isfinite(x)) {
        x = 0.0f;
    }
    if (!std::isfinite(y)) {
        y = 0.0f;
    }
    if (!std::isfinite(z)) {
        z = 0.0f;
    }

    return AxisAngle(x, y, z, theta);
}

void TransformMatrix::to_twist(float twist[6]) const
{
    Position p = get_position();
    AxisAngle angvec = to_axis_angle();

    twist[0] = p.x;
    twist[1] = p.y;
    twist[2] = p.z;
    twist[3] = angvec.x * angvec.theta;
    twist[4] = angvec.y * angvec.theta;
    twist[5] = angvec.z * angvec.theta;
}

void TransformMatrix::get_rotation_error(const TransformMatrix &target, float we[3]) const
{
    // Relative transformation: Td * inv(T); its twist rotation part is the error
    TransformMatrix relative_transform = target * this->inverse();

    float twist[6];
    relative_transform.to_twist(twist);

    we[0] = twist[3];
    we[1] = twist[4];
    we[2] = twist[5];

    for (int i = 0; i < 3; i++) {
        if (std::isnan(we[i]) || std::isinf(we[i])) {
            we[i] = 0.0f;
        }
    }
}

RobotConfig RobotConfig::ragtime_panthera()
{
    RobotConfig config = {
        .dh = {
            /* theta, d, a, alpha, offset */
            {0.0f,  0.1005f, 0.0f,      -(float)M_PI / 2.0f, 0.0f},
            {0.0f,  0.0f,    0.18f,     0.0f, (float)M_PI},
            {0.0f,  0.0f,    0.188809f, 0.0f,                162.429f * (float)M_PI / 180.0f},
            {0.0f,  0.0f,    0.08f,     -(float)M_PI / 2.0f, 17.5715f * (float)M_PI / 180.0f},
            {0.0f,  0.0f,    0.0f, (float)M_PI / 2.0f, (float)M_PI / 2.0f},
            {0.0f,  0.184f,  0.0f, (float)M_PI / 2.0f,  -(float)M_PI / 2.0f},
        },
        // TODO: replace with calibrated mechanical limits.
        .limits = {
            {-(float)M_PI, (float)M_PI},
            {-(float)M_PI, (float)M_PI},
            {-(float)M_PI, (float)M_PI},
            {-(float)M_PI, (float)M_PI},
            {-(float)M_PI, (float)M_PI},
            {-(float)M_PI, (float)M_PI},
        },
    };
    return config;
}

const char *ik_status_to_string(IKStatus status)
{
    switch (status) {
    case IKStatus::OK:
        return "OK";
    case IKStatus::NOT_CONVERGED:
        return "NOT_CONVERGED";
    case IKStatus::SINGULAR:
        return "SINGULAR";
    default:
        return "UNKNOWN";
    }
}

TransformMatrix Kinematic::dh_transform(int joint_index, float joint_angle) const
{
    // T =
    // | cθ  -sθcα   sθsα   acθ |
    // | sθ   cθcα  -cθsα   asθ |
    // |  0     sα     cα     d |
    // |  0      0      0     1 |

    TransformMatrix transform;
    const DHParam &dh = config_.dh[joint_index];

    float theta = dh.theta + joint_angle + dh.offset;
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);
    float cos_alpha = cosf(dh.alpha);
    float sin_alpha = sinf(dh.alpha);

    transform(0, 0) = cos_theta;
    transform(0, 1) = -sin_theta * cos_alpha;
    transform(0, 2) = sin_theta * sin_alpha;
    transform(0, 3) = dh.a * cos_theta;

    transform(1, 0) = sin_theta;
    transform(1, 1) = cos_theta * cos_alpha;
    transform(1, 2) = -cos_theta * sin_alpha;
    transform(1, 3) = dh.a * sin_theta;

    transform(2, 0) = 0.0f;
    transform(2, 1) = sin_alpha;
    transform(2, 2) = cos_alpha;
    transform(2, 3) = dh.d;

    return transform;
}

TransformMatrix Kinematic::forward(const Joint &joint) const
{
    TransformMatrix transform;
    for (int i = 0; i < NUM_JOINTS; i++) {
        transform = transform * dh_transform(i, joint[i]);
    }
    return transform;
}

void Kinematic::error_twist(const TransformMatrix &current, const TransformMatrix &target, float error[6]) const
{
    Position position_error = target.get_position() - current.get_position();
    error[0] = position_error.x;
    error[1] = position_error.y;
    error[2] = position_error.z;
    current.get_rotation_error(target, &error[3]);
}

IKResult Kinematic::solve_ik(const TransformMatrix &target, const Joint &seed, const IKOptions &options) const
{
    IKResult result = {};
    result.status = IKStatus::NOT_CONVERGED;
    result.joints = seed;
    for (int i = 0; i < NUM_JOINTS; i++) {
        result.joints[i] = apply_limits(i, result.joints[i]);
    }

    float error[6];
    float delta[6];
    float step = 1.0f;

    TransformMatrix current = forward(result.joints);
    error_twist(current, target, error);
    float pos_error_sq = error[0] * error[0] + error[1] * error[1] + error[2] * error[2];
    float rot_error_sq = error[3] * error[3] + error[4] * error[4] + error[5] * error[5];

    int max_iterations = options.max_iterations > 0 ? options.max_iterations : 0;
    for (int iter = 0; iter < max_iterations; iter++) {
        result.pos_error = sqrtf(pos_error_sq);
        result.rot_error = sqrtf(rot_error_sq);
        result.iterations = iter;

        if (result.pos_error < options.pos_tolerance && result.rot_error < options.rot_tolerance) {
            result.status = IKStatus::OK;
            return result;
        }

        float jacobian[6][6];
        calculate_jacobian(result.joints, jacobian);

        // Damped least squares: (J^T * J + lambda * I) * delta = J^T * error.
        // If the system is unsolvable near a singularity, retry with a larger damping.
        float lambda = options.damping > 0.0f ? options.damping : 1e-9f;
        bool solved = false;
        while (lambda <= 1.0f) {
            if (solve_damped_least_squares(jacobian, error, lambda, delta)) {
                solved = true;
                break;
            }
            lambda *= 10.0f;
        }
        if (!solved) {
            result.status = IKStatus::SINGULAR;
            return result;
        }

        // Backtracking line search: accept the update only if it reduces the error
        Joint new_joint = result.joints;
        for (int i = 0; i < NUM_JOINTS; i++) {
            new_joint[i] = apply_limits(i, new_joint[i] + step * delta[i]);
        }

        float new_error[6];
        TransformMatrix new_transform = forward(new_joint);
        error_twist(new_transform, target, new_error);
        float new_pos_error_sq = new_error[0] * new_error[0] + new_error[1] * new_error[1] + new_error[2] * new_error[2];
        float new_rot_error_sq = new_error[3] * new_error[3] + new_error[4] * new_error[4] + new_error[5] * new_error[5];

        if (new_pos_error_sq + new_rot_error_sq < pos_error_sq + rot_error_sq) {
            result.joints = new_joint;
            memcpy(error, new_error, sizeof(error));
            pos_error_sq = new_pos_error_sq;
            rot_error_sq = new_rot_error_sq;
            step = 1.0f;
        } else {
            step /= 2.0f;
            if (step < 1e-3f) {
                return result;
            }
        }
    }

    result.pos_error = sqrtf(pos_error_sq);
    result.rot_error = sqrtf(rot_error_sq);
    result.iterations = max_iterations;
    return result;
}

void Kinematic::calculate_jacobian(const Joint &joint, float jacobian[6][6]) const
{
    TransformMatrix transforms[NUM_JOINTS + 1]; // T_0^0, T_1^0, ..., T_6^0
    transforms[0] = TransformMatrix();          // Identity matrix

    for (int i = 0; i < NUM_JOINTS; i++) {
        transforms[i + 1] = transforms[i] * dh_transform(i, joint[i]);
    }

    Position p_e = transforms[NUM_JOINTS].get_position();

    for (int i = 0; i < NUM_JOINTS; i++) {
        // Joint axis z_i and origin p_i of frame i, both in the base frame
        float z_i[3] = {transforms[i](0, 2), transforms[i](1, 2), transforms[i](2, 2)};
        Position p_i = transforms[i].get_position();

        // Position part: J_pi = z_i × (p_e - p_i); orientation part: J_oi = z_i
        Position diff = p_e - p_i;
        jacobian[0][i] = z_i[1] * diff.z - z_i[2] * diff.y;
        jacobian[1][i] = z_i[2] * diff.x - z_i[0] * diff.z;
        jacobian[2][i] = z_i[0] * diff.y - z_i[1] * diff.x;
        jacobian[3][i] = z_i[0];
        jacobian[4][i] = z_i[1];
        jacobian[5][i] = z_i[2];
    }
}

bool Kinematic::solve_damped_least_squares(const float jacobian[6][6], const float error[6], float damping, float delta[6])
{
    // Build A = J^T * J + lambda * I and b = J^T * error, then solve A * delta = b
    // with Gaussian elimination (partial pivoting). A near-zero pivot means the
    // system is singular even after damping.
    float a[6][7];

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            float sum = 0.0f;
            for (int k = 0; k < 6; k++) {
                sum += jacobian[k][i] * jacobian[k][j];
            }
            a[i][j] = sum;
        }
        a[i][i] += damping;

        float sum = 0.0f;
        for (int k = 0; k < 6; k++) {
            sum += jacobian[k][i] * error[k];
        }
        a[i][6] = sum;
    }

    for (int i = 0; i < 6; i++) {
        int max_row = i;
        for (int k = i + 1; k < 6; k++) {
            if (fabsf(a[k][i]) > fabsf(a[max_row][i])) {
                max_row = k;
            }
        }
        if (max_row != i) {
            for (int j = i; j < 7; j++) {
                float swap = a[i][j];
                a[i][j] = a[max_row][j];
                a[max_row][j] = swap;
            }
        }

        if (fabsf(a[i][i]) < 1e-9f) {
            return false;
        }

        for (int k = i + 1; k < 6; k++) {
            float factor = a[k][i] / a[i][i];
            for (int j = i; j < 7; j++) {
                a[k][j] -= factor * a[i][j];
            }
        }
    }

    for (int i = 5; i >= 0; i--) {
        float sum = a[i][6];
        for (int j = i + 1; j < 6; j++) {
            sum -= a[i][j] * delta[j];
        }
        delta[i] = sum / a[i][i];
    }

    return true;
}

float Kinematic::apply_limits(int joint_index, float angle) const
{
    const JointLimit &limit = config_.limits[joint_index];
    if (angle < limit.min) {
        return limit.min;
    }
    if (angle > limit.max) {
        return limit.max;
    }
    return angle;
}
