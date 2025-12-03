/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstring>
#include <cstdio>
#include <limits>
#include <cstdlib>
#include <ctime>
#include "kinematic.h"

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

    // Extract rotation matrix R (3x3) and translation vector t (3x1)
    float r00 = data[0][0], r01 = data[0][1], r02 = data[0][2];
    float r10 = data[1][0], r11 = data[1][1], r12 = data[1][2];
    float r20 = data[2][0], r21 = data[2][1], r22 = data[2][2];
    float tx = data[0][3], ty = data[1][3], tz = data[2][3];

    // For homogeneous transformation matrix:
    // T = [R  t]
    //     [0  1]
    // T^-1 = [R^T  -R^T*t]
    //        [0    1     ]

    // Set rotation part (R^T)
    inv(0, 0) = r00; inv(0, 1) = r10; inv(0, 2) = r20;
    inv(1, 0) = r01; inv(1, 1) = r11; inv(1, 2) = r21;
    inv(2, 0) = r02; inv(2, 1) = r12; inv(2, 2) = r22;

    // Set translation part (-R^T * t)
    inv(0, 3) = -(r00 * tx + r10 * ty + r20 * tz);
    inv(1, 3) = -(r01 * tx + r11 * ty + r21 * tz);
    inv(2, 3) = -(r02 * tx + r12 * ty + r22 * tz);

    // Set bottom row
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
    // Calculate the axis-angle representation of the rotation matrix
    float R[3][3];
    get_rotation(R);

    float trace = R[0][0] + R[1][1] + R[2][2];

    // theta = arccos(0.5 * (trace - 1))
    // [ux, uy, uz] = 1 / (2 * sin(theta)) * [R21 - R12, R02 - R20, R10 - R01]
    float cos_theta = 0.5f * (trace - 1.0f);
    // Limit cos_theta to [-1, 1] range like reference code
    if (cos_theta > 1.0f) {
        cos_theta = 1.0f;
    } else if (cos_theta < -1.0f) {
        cos_theta = -1.0f;
    }
    float theta = acosf(cos_theta);

    if (fabsf(theta) < 1e-6f || fabsf(theta - M_PI) < 1e-6f) {
        // Special cases: theta = 0 or theta = π
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
    } else {
        // General case - exactly matching reference implementation
        float sin_theta = sinf(theta);
        if (fabsf(sin_theta) < 1e-8f) {
            // sin_theta is too small, use special case
            return AxisAngle(1.0f, 0.0f, 0.0f, theta);
        }
        float x = (R[2][1] - R[1][2]) / (2.0f * sin_theta);
        float y = (R[0][2] - R[2][0]) / (2.0f * sin_theta);
        float z = (R[1][0] - R[0][1]) / (2.0f * sin_theta);

        // Check for NaN or infinity
        if (std::isnan(x) || std::isinf(x)) {
            x = 0.0f;
        }
        if (std::isnan(y) || std::isinf(y)) {
            y = 0.0f;
        }
        if (std::isnan(z) || std::isinf(z)) {
            z = 0.0f;
        }

        return AxisAngle(x, y, z, theta);
    }
}

void TransformMatrix::to_twist(float twist[6]) const
{
    // Twist = [px, py, pz, phix, phiy, phiz], contains the position and rotation

    // Get position and axis-angle representation
    Position p = get_position();
    AxisAngle angvec = to_axis_angle();

    // Calculate phi = axis * theta
    float phi_x = angvec.x * angvec.theta;
    float phi_y = angvec.y * angvec.theta;
    float phi_z = angvec.z * angvec.theta;

    // Fill twist vector: [px, py, pz, phix, phiy, phiz]
    twist[0] = p.x;
    twist[1] = p.y;
    twist[2] = p.z;
    twist[3] = phi_x;
    twist[4] = phi_y;
    twist[5] = phi_z;
}

void TransformMatrix::get_rotation_error(const TransformMatrix &target, float we[3]) const
{
    // Calculate relative transformation: Td * inv(T)
    TransformMatrix current_inv = this->inverse();
    TransformMatrix relative_transform;

    // Matrix multiplication: target * current_inv
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            relative_transform(i, j) = 0.0f;
            for (int k = 0; k < 4; k++) {
                relative_transform(i, j) += target(i, k) * current_inv(k, j);
            }
        }
    }

    // Get twist representation
    float twist[6];
    relative_transform.to_twist(twist);

    // Extract rotation error: we = [phix, phiy, phiz]
    we[0] = twist[3];
    we[1] = twist[4];
    we[2] = twist[5];

    // Check for NaN or infinity
    for (int i = 0; i < 3; i++) {
        if (std::isnan(we[i]) || std::isinf(we[i])) {
            we[i] = 0.0f;  // Set to zero if invalid
        }
    }
}

Kinematic::Kinematic()
{
    float temp_dh_matrix[6][5] = {
        /* theta, d, a, alpha, offset */
        {0.0f,  0.1005f, 0,         -M_PI / 2.0f,       0.0f},
        {0.0f,  0.0f,    0.18f,     0.0f,               M_PI},
        {0.0f,  0.0f,    0.188809f, 0.0f,               162.429f * M_PI / 180.0f},
        {0.0f,  0.0f,    0.08f,     -M_PI / 2.0f,       17.5715f * M_PI / 180.0f},
        {0.0f,  0.0f,    0.0f,      M_PI / 2.0f,        M_PI / 2.0f},
        {0.0f,  0.184f,  0.0f,      M_PI / 2.0f,        -M_PI / 2.0f},
    };
    memcpy(dh_matrix, temp_dh_matrix, sizeof(temp_dh_matrix));
}

TransformMatrix Kinematic::calculate_dh_transform(int joint_index, float joint_angle)
{
    // T =
    // | cθ  -sθcα   sθsα   acθ |
    // | sθ   cθcα  -cθsα   asθ |
    // |  0     sα     cα     d |
    // |  0      0      0     1 |

    TransformMatrix transform;

    float theta = dh_matrix[joint_index][0] + joint_angle + dh_matrix[joint_index][4];
    float d = dh_matrix[joint_index][1];
    float a = dh_matrix[joint_index][2];
    float alpha = dh_matrix[joint_index][3];

    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);
    float cos_alpha = cosf(alpha);
    float sin_alpha = sinf(alpha);

    transform(0, 0) = cos_theta;
    transform(0, 1) = -sin_theta * cos_alpha;
    transform(0, 2) = sin_theta * sin_alpha;
    transform(0, 3) = a * cos_theta;

    transform(1, 0) = sin_theta;
    transform(1, 1) = cos_theta * cos_alpha;
    transform(1, 2) = -cos_theta * sin_alpha;
    transform(1, 3) = a * sin_theta;

    transform(2, 0) = 0.0f;
    transform(2, 1) = sin_alpha;
    transform(2, 2) = cos_alpha;
    transform(2, 3) = d;

    transform(3, 0) = 0.0f;
    transform(3, 1) = 0.0f;
    transform(3, 2) = 0.0f;
    transform(3, 3) = 1.0f;

    return transform;
}

void Kinematic::matrix_multiply(const TransformMatrix &a, const TransformMatrix &b, TransformMatrix &result)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result(i, j) = 0.0f;
            for (int k = 0; k < 4; k++) {
                result(i, j) += a(i, k) * b(k, j);
            }
        }
    }
}

void Kinematic::solve_forward_kinematics(const Joint &joint, TransformMatrix &transform_matrix)
{
    transform_matrix = TransformMatrix();

    for (int i = 0; i < NUM_JOINTS; i++) {
        TransformMatrix dh_transform = calculate_dh_transform(i, joint[i]);
        TransformMatrix temp_result;
        matrix_multiply(transform_matrix, dh_transform, temp_result);
        transform_matrix = temp_result;
    }
}

bool Kinematic::solve_inverse_kinematics(const TransformMatrix &transform_matrix, Joint &joint, float tolerance, int max_iterations)
{
    TransformMatrix current_transform;
    Position position_error;
    float rotation_error[3];
    float error_vector[6];
    float new_error_vector[6];
    float joint_delta[6];
    float step = 1.0f;

    for (int iter = 0; iter < max_iterations; iter++) {
        solve_forward_kinematics(joint, current_transform);

        // Calculate current position and target position error
        Position current_pos = current_transform.get_position();
        Position target_pos = transform_matrix.get_position();
        position_error = target_pos - current_pos;

        // Calculate rotation error (using twist representation)
        current_transform.get_rotation_error(transform_matrix, rotation_error);

        // Build error vector [position error; rotation error]
        error_vector[0] = position_error.x;
        error_vector[1] = position_error.y;
        error_vector[2] = position_error.z;
        error_vector[3] = rotation_error[0];
        error_vector[4] = rotation_error[1];
        error_vector[5] = rotation_error[2];

        // Calculate the norm of the error
        float error_norm = 0.0f;
        for (int i = 0; i < 6; i++) {
            error_norm += error_vector[i] * error_vector[i];
        }
        error_norm = sqrtf(error_norm);

        // Check if the error is within the tolerance
        if (error_norm < tolerance) {
            return true;
        }

        // Calculate Jacobian matrix
        float jacobian[6][6];
        calculate_jacobian(joint, jacobian);

        // Solve the linear system: J * delta_q = error
        // delta_q = (J^T * J)^-1 * J^T * error
        bool success = solve_linear_system(jacobian, error_vector, joint_delta, step);

        if (!success) {
            // Jacobian matrix is singular, add regularization term
            add_regularization(jacobian, 0.1f);
            success = solve_linear_system(jacobian, error_vector, joint_delta, step);
            if (!success) {
                return false; // Cannot solve the linear system
            }
        }

        // Update the joint angles
        Joint new_joint = joint;
        for (int i = 0; i < 6; i++) {
            new_joint[i] += joint_delta[i];
            // Normalize the angle to [-π, π]
            new_joint[i] = normalize_angle(new_joint[i]);
        }

        // Check if the new solution is better
        solve_forward_kinematics(new_joint, current_transform);
        Position new_pos = current_transform.get_position();
        Position new_position_error = target_pos - new_pos;
        current_transform.get_rotation_error(transform_matrix, rotation_error);

        new_error_vector[0] = new_position_error.x;
        new_error_vector[1] = new_position_error.y;
        new_error_vector[2] = new_position_error.z;
        new_error_vector[3] = rotation_error[0];
        new_error_vector[4] = rotation_error[1];
        new_error_vector[5] = rotation_error[2];

        float new_error_norm = 0.0f;
        for (int i = 0; i < 6; i++) {
            new_error_norm += new_error_vector[i] * new_error_vector[i];
        }
        new_error_norm = sqrtf(new_error_norm);

        if (new_error_norm < error_norm) {
            // The new solution is better, accept the update
            joint = new_joint;
            step = 1.0f; // Reset the step size
        } else {
            // The new solution is not better, decrease the step size
            step /= 2.0f;
            if (step < 1e-3f) {
                return false; // The step size is too small, cannot converge
            }
        }
    }

    return false; // Exceeded the maximum number of iterations
}

void Kinematic::calculate_jacobian(const Joint &joint, float jacobian[6][6])
{
    // J = [Zi * (p_e - p_i)]
    //   = [Zi]

    TransformMatrix transforms[7]; // T_0^0, T_1^0, ..., T_6^0
    transforms[0] = TransformMatrix(); // Identity matrix

    // Calculate the transformation matrices for each joint
    for (int i = 0; i < NUM_JOINTS; i++) {
        TransformMatrix dh_transform = calculate_dh_transform(i, joint[i]);
        matrix_multiply(transforms[i], dh_transform, transforms[i + 1]);
    }

    // End effector position
    Position p_e = transforms[NUM_JOINTS].get_position();

    // Calculate the Jacobian columns
    for (int i = 0; i < NUM_JOINTS; i++) {
        float z_i[3] = {transforms[i](0, 2), transforms[i](1, 2), transforms[i](2, 2)};
        Position p_i = Position(transforms[i](0, 3), transforms[i](1, 3), transforms[i](2, 3));

        // Position Jacobian: J_pi = z_i × (p_e - p_i)
        // Cross product calculated via skew-symmetric matrix: z_i × diff = [z_i]× * diff
        //
        // Skew-symmetric matrix [z_i]× = [0   -z_iz   z_iy]
        //                                  [z_iz   0   -z_ix]
        //                                  [-z_iy z_ix   0 ]
        //
        // Matrix multiplication: [z_i]× * diff = [0*diff.x + (-z_iz)*diff.y + z_iy*diff.z]
        //                                      [z_iz*diff.x + 0*diff.y + (-z_ix)*diff.z]
        //                                      [(-z_iy)*diff.x + z_ix*diff.y + 0*diff.z]
        //
        // Simplified result: z_i × diff = [z_iy*diff.z - z_iz*diff.y]  ← x component
        //                                 [z_iz*diff.x - z_ix*diff.z]  ← y component
        //                                 [z_ix*diff.y - z_iy*diff.x]  ← z component
        Position diff = p_e - p_i;
        float cross_product[3];
        cross_product[0] = z_i[1] * diff.z - z_i[2] * diff.y;  // z_iy*diff.z - z_iz*diff.y
        cross_product[1] = z_i[2] * diff.x - z_i[0] * diff.z;  // z_iz*diff.x - z_ix*diff.z
        cross_product[2] = z_i[0] * diff.y - z_i[1] * diff.x;  // z_ix*diff.y - z_iy*diff.x

        // Orientation Jacobian: J_oi = z_i
        jacobian[0][i] = cross_product[0]; // J_px
        jacobian[1][i] = cross_product[1]; // J_py
        jacobian[2][i] = cross_product[2]; // J_pz
        jacobian[3][i] = z_i[0];          // J_ox
        jacobian[4][i] = z_i[1];          // J_oy
        jacobian[5][i] = z_i[2];          // J_oz
    }
}

bool Kinematic::solve_linear_system(float jacobian[6][6], const float error[6], float solution[6], float step)
{
    // Solve the linear system: J^T * J * delta_q = J^T * error
    // delta_q = (J^T * J)^-1 * J^T * error

    float JT[6][6];
    matrix_transpose_6x6(jacobian, JT);

    // Calculate J^T * J
    float JTJ[6][6];
    matrix_multiply_6x6(JT, jacobian, JTJ);

    // Calculate J^T * error
    float JTe[6];
    matrix_vector_multiply_6x6(JT, error, JTe);

    // Check if the matrix is singular
    float det = calculate_determinant_6x6(JTJ);
    if (fabsf(det) < 1e-12f) {
        return false; // Matrix is singular
    }

    float JTJ_inv[6][6];
    matrix_inverse_6x6(JTJ, JTJ_inv);

    matrix_vector_multiply_6x6(JTJ_inv, JTe, solution);

    for (int i = 0; i < 6; i++) {
        solution[i] *= step; // Apply the step size
    }

    return true;
}

void Kinematic::add_regularization(float jacobian[6][6], float lambda)
{
    // Add regularization term: J^T * J + lambda * I
    // We directly modify jacobian, assuming it has been transposed and multiplied
    for (int i = 0; i < 6; i++) {
        jacobian[i][i] += lambda;
    }
}

float Kinematic::normalize_angle(float angle)
{
    // Normalize the angle to [-π, π]
    while (angle > M_PI) {
        angle -= 2.0f * M_PI;
    }
    while (angle < -M_PI) {
        angle += 2.0f * M_PI;
    }
    return angle;
}

void Kinematic::matrix_inverse_6x6(const float input[6][6], float output[6][6])
{
    float temp[6][12];

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            temp[i][j] = input[i][j];
            temp[i][j + 6] = (i == j) ? 1.0f : 0.0f;
        }
    }

    for (int i = 0; i < 6; i++) {
        int max_row = i;
        for (int k = i + 1; k < 6; k++) {
            if (fabsf(temp[k][i]) > fabsf(temp[max_row][i])) {
                max_row = k;
            }
        }

        if (max_row != i) {
            for (int j = 0; j < 12; j++) {
                float swap = temp[i][j];
                temp[i][j] = temp[max_row][j];
                temp[max_row][j] = swap;
            }
        }

        if (fabsf(temp[i][i]) < 1e-12f) {
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 6; j++) {
                    output[i][j] = 0.0f;
                }
            }
            return;
        }

        float pivot = temp[i][i];
        for (int j = 0; j < 12; j++) {
            temp[i][j] /= pivot;
        }

        for (int k = 0; k < 6; k++) {
            if (k != i) {
                float factor = temp[k][i];
                for (int j = 0; j < 12; j++) {
                    temp[k][j] -= factor * temp[i][j];
                }
            }
        }
    }

    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            output[i][j] = temp[i][j + 6];
        }
    }
}

void Kinematic::matrix_multiply_6x6(const float a[6][6], const float b[6][6], float result[6][6])
{
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            result[i][j] = 0.0f;
            for (int k = 0; k < 6; k++) {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

void Kinematic::matrix_transpose_6x6(const float input[6][6], float output[6][6])
{
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) {
            output[i][j] = input[j][i];
        }
    }
}

void Kinematic::matrix_vector_multiply_6x6(const float matrix[6][6], const float vector[6], float result[6])
{
    for (int i = 0; i < 6; i++) {
        result[i] = 0.0f;
        for (int j = 0; j < 6; j++) {
            result[i] += matrix[i][j] * vector[j];
        }
    }
}

float Kinematic::calculate_determinant_6x6(const float matrix[6][6])
{
    float det = 0.0f;
    float sign = 1.0f;

    for (int j = 0; j < 6; j++) {
        float minor[5][5];
        for (int row = 1; row < 6; row++) {
            int col_idx = 0;
            for (int col = 0; col < 6; col++) {
                if (col != j) {
                    minor[row - 1][col_idx] = matrix[row][col];
                    col_idx++;
                }
            }
        }

        float minor_det = calculate_determinant_5x5(minor);
        det += sign * matrix[0][j] * minor_det;
        sign = -sign;
    }

    return det;
}

float Kinematic::calculate_determinant_5x5(const float matrix[5][5])
{
    float det = 0.0f;
    float sign = 1.0f;

    for (int j = 0; j < 5; j++) {
        float minor[4][4];
        for (int row = 1; row < 5; row++) {
            int col_idx = 0;
            for (int col = 0; col < 5; col++) {
                if (col != j) {
                    minor[row - 1][col_idx] = matrix[row][col];
                    col_idx++;
                }
            }
        }

        float minor_det = calculate_determinant_4x4(minor);
        det += sign * matrix[0][j] * minor_det;
        sign = -sign;
    }

    return det;
}

float Kinematic::calculate_determinant_4x4(const float matrix[4][4])
{
    float det = 0.0f;
    float sign = 1.0f;

    for (int j = 0; j < 4; j++) {
        float minor[3][3];
        for (int row = 1; row < 4; row++) {
            int col_idx = 0;
            for (int col = 0; col < 4; col++) {
                if (col != j) {
                    minor[row - 1][col_idx] = matrix[row][col];
                    col_idx++;
                }
            }
        }

        float minor_det = calculate_determinant_3x3(minor);
        det += sign * matrix[0][j] * minor_det;
        sign = -sign;
    }

    return det;
}

float Kinematic::calculate_determinant_3x3(const float matrix[3][3])
{
    // [a11, a12, a13]
    // [a21, a22, a23]
    // [a31, a32, a33]
    // D = a11 * (a22 * a33 - a23 * a32) - a12 * (a21 * a33 - a23 * a31) + a13 * (a21 * a32 - a22 * a31)
    return matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
           matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
           matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
}
