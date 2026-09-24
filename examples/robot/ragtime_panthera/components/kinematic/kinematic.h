/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <cstdint>
#include <cmath>

#define DEG2RAD(x) ((x) * M_PI / 180)
#define RAD2DEG(x) ((x) * 180 / M_PI)

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

    TransformMatrix operator*(const TransformMatrix &other) const;

    void print() const;

    Position get_position() const;
    TransformMatrix inverse() const;

    void get_rotation(float rotation[3][3]) const;
    AxisAngle to_axis_angle() const;
    void to_twist(float twist[6]) const;
    void get_rotation_error(const TransformMatrix &target, float we[3]) const;
};

struct DHParam {
    float theta;    /*!< Fixed part of the joint angle (rad) */
    float d;        /*!< Link offset (m) */
    float a;        /*!< Link length (m) */
    float alpha;    /*!< Link twist (rad) */
    float offset;   /*!< Joint angle offset added to the commanded angle (rad) */
};

struct JointLimit {
    float min;      /*!< Lower joint limit (rad) */
    float max;      /*!< Upper joint limit (rad) */
};

struct RobotConfig {
    DHParam dh[6];
    JointLimit limits[6];

    static RobotConfig ragtime_panthera();
};

enum class IKStatus {
    OK,             /*!< Converged: both position and rotation errors within tolerance */
    NOT_CONVERGED,  /*!< Reached iteration/step-size limit before converging */
    SINGULAR,       /*!< Jacobian is singular, cannot make progress */
};

const char *ik_status_to_string(IKStatus status);

struct IKOptions {
    float pos_tolerance = 5e-3f;    /*!< Position tolerance (m) */
    float rot_tolerance = 1e-2f;    /*!< Rotation tolerance (rad) */
    int max_iterations = 100;
    float damping = 1e-4f;          /*!< Damped-least-squares lambda added to diag(J^T * J) */
};

struct IKResult {
    IKStatus status;
    Joint joints;       /*!< Best solution found; only trust it when status == OK */
    float pos_error;    /*!< Final position error (m) */
    float rot_error;    /*!< Final rotation error (rad) */
    int iterations;

    explicit operator bool() const
    {
        return status == IKStatus::OK;
    }
};

class Kinematic {
public:
    static constexpr int NUM_JOINTS = 6;

    Kinematic() : config_(RobotConfig::ragtime_panthera()) {}
    explicit Kinematic(const RobotConfig &config) : config_(config) {}
    ~Kinematic() = default;

    /** Convert joint angles to an end-effector pose. */
    TransformMatrix forward(const Joint &joint) const;

    /** Solve IK from an initial seed; check the returned status before use. */
    [[nodiscard]] IKResult solve_ik(const TransformMatrix &target, const Joint &seed, const IKOptions &options = {}) const;

    const RobotConfig &config() const
    {
        return config_;
    }

private:
    TransformMatrix dh_transform(int joint_index, float joint_angle) const;
    void calculate_jacobian(const Joint &joint, float jacobian[6][6]) const;
    void error_twist(const TransformMatrix &current, const TransformMatrix &target, float error[6]) const;
    float apply_limits(int joint_index, float angle) const;
    static bool solve_damped_least_squares(const float jacobian[6][6], const float error[6], float damping, float delta[6]);

    RobotConfig config_;
};
