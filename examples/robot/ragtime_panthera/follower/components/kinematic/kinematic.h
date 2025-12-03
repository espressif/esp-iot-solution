/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <cstdint>
#include <cmath>

#define DEG2RAD(x) (x * M_PI / 180)
#define RAD2DEG(x) (x * 180 / M_PI)

struct Position {
    float x, y, z;

    Position() : x(0.0f), y(0.0f), z(0.0f) {}
    Position(float x_val, float y_val, float z_val) : x(x_val), y(y_val), z(z_val) {}

    Position operator-(const Position &other) const
    {
        return Position(x - other.x, y - other.y, z - other.z);
    }

    Position operator+(const Position &other) const
    {
        return Position(x + other.x, y + other.y, z + other.z);
    }

};

struct AxisAngle {
    float x, y, z, theta;

    AxisAngle() : x(0.0f), y(0.0f), z(0.0f), theta(0.0f) {}
    AxisAngle(float x_val, float y_val, float z_val, float theta_val)
        : x(x_val), y(y_val), z(z_val), theta(theta_val) {}
};

struct Joint {
    float angles[6];

    Joint()
    {
        for (int i = 0; i < 6; i++) {
            angles[i] = 0.0f;
        }
    }

    Joint(float j0, float j1, float j2, float j3, float j4, float j5)
    {
        angles[0] = j0;
        angles[1] = j1;
        angles[2] = j2;
        angles[3] = j3;
        angles[4] = j4;
        angles[5] = j5;
    }

    float &operator[](int index)
    {
        return angles[index];
    }
    const float &operator[](int index) const
    {
        return angles[index];
    }
};

struct TransformMatrix {
    float data[4][4];

    TransformMatrix()
    {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                data[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
    }

    float &operator()(int row, int col)
    {
        return data[row][col];
    }
    const float &operator()(int row, int col) const
    {
        return data[row][col];
    }

    void print() const;

    Position get_position() const;
    TransformMatrix inverse() const;

    void get_rotation(float rotation[3][3]) const;
    AxisAngle to_axis_angle() const;
    void to_twist(float twist[6]) const;
    void get_rotation_error(const TransformMatrix &target, float we[3]) const;
};

class Kinematic {
public:
    static constexpr int NUM_JOINTS = 6;

    Kinematic();
    ~Kinematic() = default;
    void solve_forward_kinematics(const Joint &joint, TransformMatrix &transform_matrix);
    bool solve_inverse_kinematics(const TransformMatrix &transform_matrix, Joint &joint, float tolerance = 1e-4f, int max_iterations = 100);
    float dh_matrix[6][5];
    TransformMatrix calculate_dh_transform(int joint_index, float joint_angle);
    void matrix_multiply(const TransformMatrix &a, const TransformMatrix &b, TransformMatrix &result);

private:
    void calculate_jacobian(const Joint &joint, float jacobian[6][6]);
    bool solve_linear_system(float jacobian[6][6], const float error[6], float solution[6], float step);
    void add_regularization(float jacobian[6][6], float lambda);
    float normalize_angle(float angle);
    void matrix_inverse_6x6(const float input[6][6], float output[6][6]);
    void matrix_multiply_6x6(const float a[6][6], const float b[6][6], float result[6][6]);
    void matrix_transpose_6x6(const float input[6][6], float output[6][6]);
    void matrix_vector_multiply_6x6(const float matrix[6][6], const float vector[6], float result[6]);
    float calculate_determinant_6x6(const float matrix[6][6]);
    float calculate_determinant_5x5(const float matrix[5][5]);
    float calculate_determinant_4x4(const float matrix[4][4]);
    float calculate_determinant_3x3(const float matrix[3][3]);

};
