/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <vector>
#include <algorithm>
#include "argtable3/argtable3.h"
#include "esp_check.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "kinematic.h"
#include "app_manager.h"
#include "app_console.h"

#if CONFIG_CONSOLE_CONTROL
static const char* TAG = "app_console";

// Global motor control instance
static damiao::Motor_Control* g_motor_control = nullptr;

// Global app manager instance
static Manager* g_app_manager = nullptr;
static struct {
    struct arg_str *enable;
    struct arg_end *end;
} panthera_enable_args;

static struct {
    struct arg_str *enable;
    struct arg_end *end;
} panthera_zero_g_args;

static struct {
    struct arg_dbl *x;
    struct arg_dbl *y;
    struct arg_dbl *z;
    struct arg_end *end;
} panthera_move_args;

static struct {
    struct arg_dbl *u;
    struct arg_dbl *v;
    struct arg_dbl *z;
    struct arg_lit *dry_run;
    struct arg_end *end;
} panthera_goto_pixel_args;

static struct {
    struct arg_dbl *m1;
    struct arg_dbl *m2;
    struct arg_dbl *m3;
    struct arg_dbl *m4;
    struct arg_dbl *m5;
    struct arg_dbl *m6;
    struct arg_dbl *m7;
    struct arg_dbl *m8;
    struct arg_dbl *m9;
    struct arg_end *end;
} panthera_set_vision_matrix_args;

static struct {
    struct arg_int *master;
    struct arg_str *action;
    struct arg_dbl *pos;
    struct arg_dbl *vel;
    struct arg_end *end;
} panthera_motor_args;

static struct {
    struct arg_dbl *close;
    struct arg_dbl *torque;
    struct arg_dbl *z;
    struct arg_end *end;
} panthera_gripper_args;

int panthera_enable_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_enable_args.end, argv[0]);
        return 1;
    }

    if (g_app_manager == nullptr) {
        ESP_LOGE(TAG, "App manager not available");
        return 1;
    }

    // Go through the manager rather than the motors directly, so the UI switch,
    // the health monitor and the Zero-G / Grasp gates all follow.
    if (panthera_enable_args.enable->count > 0) {
        if (strcmp(panthera_enable_args.enable->sval[0], "on") == 0) {
            if (g_app_manager->set_motors_enabled(true) != ESP_OK) {
                ESP_LOGE(TAG, "Panthera enable failed");
                return 1;
            }
            ESP_LOGI(TAG, "Panthera enabled");
        } else if (strcmp(panthera_enable_args.enable->sval[0], "off") == 0) {
            g_app_manager->set_motors_enabled(false);
            ESP_LOGI(TAG, "Panthera disabled");
        } else {
            ESP_LOGI(TAG, "Invalid panthera state");
            return 1;
        }
    } else {
        ESP_LOGI(TAG, "Panthera state not specified");
        return 1;
    }
    return 0;
}

/** Parse an on/off argument. */
static esp_err_t parse_on_off(const char *text, bool *on)
{
    if (strcmp(text, "on") == 0) {
        *on = true;
        return ESP_OK;
    }
    if (strcmp(text, "off") == 0) {
        *on = false;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

int panthera_zero_g_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_zero_g_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_zero_g_args.end, argv[0]);
        return 1;
    }
    if (g_app_manager == nullptr) {
        ESP_LOGE(TAG, "App manager not available");
        return 1;
    }
    if (panthera_zero_g_args.enable->count == 0) {
        ESP_LOGI(TAG, "Zero gravity state not specified");
        return 1;
    }

    bool on = false;
    if (parse_on_off(panthera_zero_g_args.enable->sval[0], &on) != ESP_OK) {
        ESP_LOGI(TAG, "Invalid zero gravity state");
        return 1;
    }
    if (on) {
        ESP_LOGW(TAG, "Support the arm: zero gravity holds gravity feedforward only");
    }
    if (g_app_manager->set_zero_g_enabled(on) != ESP_OK) {
        return 1;
    }
    ESP_LOGI(TAG, "Panthera zero gravity %s", on ? "enabled" : "disabled");
    return 0;
}

int panthera_grasp_cmd(int argc, char **argv)
{
    if (g_app_manager == nullptr) {
        ESP_LOGE(TAG, "App manager not available");
        return 1;
    }
    // Grasps whatever the camera currently sees, as the UI Grasp button does.
    return (g_app_manager->trigger_grasp() == ESP_OK) ? 0 : 1;
}

int panthera_gripper_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_gripper_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_gripper_args.end, argv[0]);
        return 1;
    }
    if (g_app_manager == nullptr) {
        ESP_LOGE(TAG, "App manager not available");
        return 1;
    }
    if (panthera_gripper_args.close->count > 0) {
        g_app_manager->set_gripper_close_pos(static_cast<float>(panthera_gripper_args.close->dval[0]));
    }
    if (panthera_gripper_args.torque->count > 0) {
        g_app_manager->set_gripper_grip_torque(static_cast<float>(panthera_gripper_args.torque->dval[0]));
    }
    if (panthera_gripper_args.z->count > 0) {
        g_app_manager->set_grasp_z(static_cast<float>(panthera_gripper_args.z->dval[0]));
    }
    printf("Gripper close position: %.3f rad (open is 5.0, larger closes harder)\n", g_app_manager->get_gripper_close_pos());
    printf("Gripper grip torque   : %.3f Nm (0 = off; grasp holds as soon as |torque| exceeds this)\n", g_app_manager->get_gripper_grip_torque());
    printf("Grasp height (Z)      : %.3f m\n", g_app_manager->get_grasp_z());
    return 0;
}

int panthera_goto_zero_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Panthera going to zero position");

    if (g_app_manager == nullptr) {
        ESP_LOGE(TAG, "App manager not available");
        return 1;
    }
    // Through the manager, so the console cannot drive joints to zero while the
    // zero-g loop is commanding the same motors.
    return (g_app_manager->go_zero() == ESP_OK) ? 0 : 1;
}

int panthera_set_zero_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Panthera setting zero position");

    const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = g_motor_control->get_motor_map();
    for (auto &pair : motor_map) {
        damiao::Motor *motor = pair.second;
        if (motor != nullptr) {
            g_motor_control->save_zero_position(*motor);
        }
    }
    return 0;
}

/** Height the grasp path uses; the fitted Z row of the vision matrix is ignored. */
// grasp Z is NVS-tunable; read it live so panthera_goto_pixel uses the latest value

/**
 * @brief Solve IK for a Cartesian target and drive the arm to it.
 *
 * Shared by panthera_goto_position and panthera_goto_pixel. With dry_run the
 * solution is printed and no motor is commanded.
 *
 * @param x X position in meters
 * @param y Y position in meters
 * @param z Z position in meters
 * @param dry_run Print the solution only
 * @return 0 on success, 1 if IK failed
 */
static int move_end_effector_to(float x, float y, float z, bool dry_run)
{
    Kinematic kinematic;
    Joint seed = Manager::GRASP_IK_SEED;
    TransformMatrix target = kinematic.forward(seed);
    target.print();

    target(0, 3) = x;
    target(1, 3) = y;
    target(2, 3) = z;

    IKResult ik = kinematic.solve_ik(target, seed);
    kinematic.forward(ik.joints).print();
    if (!ik) {
        ESP_LOGI(TAG, "IK failed (not reachable / no converge): %s (pos_err=%.4f m, rot_err=%.4f rad, iterations=%d)",
                 ik_status_to_string(ik.status), ik.pos_error, ik.rot_error, ik.iterations);
        return 1;
    }
    ESP_LOGI(TAG, "IK solved (pos_err=%.4f m, rot_err=%.4f rad, iterations=%d)", ik.pos_error, ik.rot_error,
             ik.iterations);
    printf("IK joints (kinematic frame):\n");
    for (int i = 0; i < 6; i++) {
        printf("  J%d: %8.4f rad (%7.2f deg)\n", i + 1, ik.joints[i], RAD2DEG(ik.joints[i]));
    }
    printf("Motor command frame (J3 inverted on bus):\n");
    for (int i = 0; i < 6; i++) {
        const uint32_t master_id = 0x11 + (uint32_t)i;
        const float cmd = (master_id == 0x13) ? (-ik.joints[i]) : ik.joints[i];
        printf("  0x%02X: %8.4f rad (%7.2f deg)\n", (unsigned int)master_id, cmd, RAD2DEG(cmd));
    }

    if (dry_run) {
        ESP_LOGI(TAG, "Dry run, motors not commanded");
        return 0;
    }

    // Get motor map and extract master IDs for joints 0-5 (only control first 6 joints)
    const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = g_motor_control->get_motor_map();

    // Extract master IDs and sort them to ensure consistent ordering
    std::vector<uint32_t> motor_ids;
    motor_ids.reserve(motor_map.size());
    for (const auto &pair : motor_map) {
        motor_ids.push_back(pair.first);
    }
    std::sort(motor_ids.begin(), motor_ids.end());

    // Control joints 0-5 (only first 6 motors)
    int joint_count = (motor_ids.size() > 6) ? 6 : motor_ids.size();
    for (int i = 0; i < joint_count; i++) {
        uint32_t master_id = motor_ids[i];
        auto it = motor_map.find(master_id);
        if (it != motor_map.end() && it->second != nullptr) {
            damiao::Motor* motor = it->second;
            // Joint 2 (master_id 0x13) needs to be inverted
            if (master_id == 0x13) {
                g_motor_control->pos_vel_control(*motor, ik.joints[i] * -1.0f, 4.0f);
            } else {
                g_motor_control->pos_vel_control(*motor, ik.joints[i], 2.0f);
            }
        } else {
            ESP_LOGW(TAG, "Motor with master ID 0x%02X (joint %d) not found", master_id, i);
        }
    }

    return 0;
}

int panthera_goto_position_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Panthera going to position");

    int nerrors = arg_parse(argc, argv, (void **) &panthera_move_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_move_args.end, argv[0]);
        return 1;
    }

    if (panthera_move_args.x->count == 0 || panthera_move_args.y->count == 0 ||
            panthera_move_args.z->count == 0) {
        ESP_LOGI(TAG, "Panthera position not specified");
        return 1;
    }

    float x = static_cast<float>(panthera_move_args.x->dval[0]);
    float y = static_cast<float>(panthera_move_args.y->dval[0]);
    float z = static_cast<float>(panthera_move_args.z->dval[0]);

    ESP_LOGI(TAG, "Panthera going to position (%f, %f, %f)", x, y, z);
    return move_end_effector_to(x, y, z, false);
}

/**
 * @brief Read the stored vision matrix from NVS into row-major m[3][3].
 *
 * The same store panthera_set_vision_matrix writes, holding each element as an
 * int32 scaled by 1000000. Reading it here rather than from the Manager means a
 * freshly written matrix can be tested without a reboot.
 *
 * @param m Destination matrix
 * @return ESP_OK, or the NVS error that stopped it
 */
static esp_err_t load_vision_matrix_from_nvs(double m[3][3])
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("vision_matrix", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < 9; i++) {
        char key[8];
        snprintf(key, sizeof(key), "m%d", i);
        int32_t value = 0;
        ret = nvs_get_i32(nvs_handle, key, &value);
        if (ret != ESP_OK) {
            nvs_close(nvs_handle);
            return ret;
        }
        m[i / 3][i % 3] = static_cast<double>(value) / 1000000.0;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

int panthera_goto_pixel_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_goto_pixel_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_goto_pixel_args.end, argv[0]);
        return 1;
    }

    if (panthera_goto_pixel_args.u->count == 0 || panthera_goto_pixel_args.v->count == 0) {
        ESP_LOGE(TAG, "Pixel coordinate not specified, use -u <u> -v <v>");
        return 1;
    }

    double m[3][3] = {{0}};
    esp_err_t ret = load_vision_matrix_from_nvs(m);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Vision matrix unavailable (%s); run panthera_set_vision_matrix first",
                 esp_err_to_name(ret));
        return 1;
    }

    // An uncalibrated matrix is all zeros, which maps every pixel onto the
    // origin -- inside the arm itself. Refuse instead of driving there.
    bool all_zero = true;
    for (int i = 0; i < 3 && all_zero; i++) {
        for (int j = 0; j < 3; j++) {
            if (m[i][j] != 0.0) {
                all_zero = false;
                break;
            }
        }
    }
    if (all_zero) {
        ESP_LOGE(TAG, "Vision matrix is all zeros (not calibrated); run panthera_set_vision_matrix first");
        return 1;
    }

    const double u = panthera_goto_pixel_args.u->dval[0];
    const double v = panthera_goto_pixel_args.v->dval[0];

    // Same mapping the grasp path applies to a detection box centre.
    double a[3] = { u, v, 1.0 };
    double b[3] = {0};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            b[i] += m[i][j] * a[j];
        }
    }

    const float z = (panthera_goto_pixel_args.z->count > 0)
                    ? static_cast<float>(panthera_goto_pixel_args.z->dval[0])
                    : g_app_manager->get_grasp_z();
    const bool dry_run = panthera_goto_pixel_args.dry_run->count > 0;

    ESP_LOGI(TAG, "Pixel (%.1f, %.1f) -> world (%.4f, %.4f, %.4f fitted), using z %.4f m", u, v, b[0], b[1], b[2],
             z);

    return move_end_effector_to(static_cast<float>(b[0]), static_cast<float>(b[1]), z, dry_run);
}

int panthera_set_vision_matrix_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_set_vision_matrix_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_set_vision_matrix_args.end, argv[0]);
        return 1;
    }

    // Collect matrix values from named arguments
    float matrix_values[9] = {0.0f};
    struct arg_dbl *matrix_args[] = {
        panthera_set_vision_matrix_args.m1,
        panthera_set_vision_matrix_args.m2,
        panthera_set_vision_matrix_args.m3,
        panthera_set_vision_matrix_args.m4,
        panthera_set_vision_matrix_args.m5,
        panthera_set_vision_matrix_args.m6,
        panthera_set_vision_matrix_args.m7,
        panthera_set_vision_matrix_args.m8,
        panthera_set_vision_matrix_args.m9
    };

    // Check if at least one matrix element is provided
    bool has_values = false;
    for (int i = 0; i < 9; i++) {
        if (matrix_args[i]->count > 0) {
            matrix_values[i] = static_cast<float>(matrix_args[i]->dval[0]);
            has_values = true;
        }
    }

    if (!has_values) {
        ESP_LOGI(TAG, "At least one matrix element must be provided");
        return 1;
    }

    // Print matrix
    for (int i = 0; i < 9; i += 3) {
        printf("[%.6f %.6f %.6f]\n", matrix_values[i], matrix_values[i + 1], matrix_values[i + 2]);
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("vision_matrix", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS partition: %s", esp_err_to_name(ret));
        return 1;
    }
    for (int i = 0; i < 9; i++) {
        if (matrix_args[i]->count == 0) {
            continue;  // keep the existing NVS value for unspecified elements
        }
        char key[8];
        snprintf(key, sizeof(key), "m%d", i);
        int32_t value = (int32_t)(matrix_values[i] * 1000000.0f);
        ret = nvs_set_i32(nvs_handle, key, value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error storing matrix element %d: %s", i, esp_err_to_name(ret));
            nvs_close(nvs_handle);
            return 1;
        }
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return 1;
    }
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Vision matrix stored successfully");
    if (g_app_manager != nullptr) {
        g_app_manager->reload_vision_matrix();  // grasp uses it immediately, no reboot
    }

    return 0;
}

int panthera_get_vision_matrix_cmd(int argc, char **argv)
{
    float matrix_values[9] = {0.0f};
    bool all_found = true;

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("vision_matrix", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS partition: %s", esp_err_to_name(ret));
        return 1;
    }

    for (int i = 0; i < 9; i++) {
        char key[8];
        snprintf(key, sizeof(key), "m%d", i);
        int32_t value = 0;
        ret = nvs_get_i32(nvs_handle, key, &value);
        if (ret == ESP_OK) {
            // Convert int back to float by dividing by 1000000
            matrix_values[i] = static_cast<float>(value) / 1000000.0f;
        } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Matrix element %d not found in NVS", i);
            all_found = false;
        } else {
            ESP_LOGE(TAG, "Error reading matrix element %d: %s", i, esp_err_to_name(ret));
            nvs_close(nvs_handle);
            return 1;
        }
    }

    nvs_close(nvs_handle);

    if (!all_found) {
        ESP_LOGW(TAG, "Some matrix elements were not found, showing available values:");
    }

    // Print matrix in 3x3 format
    ESP_LOGI(TAG, "Vision matrix:");
    for (int i = 0; i < 9; i += 3) {
        printf("[%.6f %.6f %.6f]\n", matrix_values[i], matrix_values[i + 1], matrix_values[i + 2]);
    }

    return 0;
}

int panthera_read_position_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Reading panthera motor positions");

    const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = g_motor_control->get_motor_map();

    // Extract master IDs and sort them to ensure consistent ordering
    std::vector<uint32_t> motor_ids;
    motor_ids.reserve(motor_map.size());
    for (const auto &pair : motor_map) {
        motor_ids.push_back(pair.first);
    }
    std::sort(motor_ids.begin(), motor_ids.end());

    // Refresh motor status and read positions
    ESP_LOGI(TAG, "Motor positions:");
    printf("Master ID | Position (rad) | Position (deg) | Fault      | Online\n");
    printf("----------|----------------|----------------|------------|-------\n");

    for (uint32_t master_id : motor_ids) {
        auto it = motor_map.find(master_id);
        if (it != motor_map.end() && it->second != nullptr) {
            damiao::Motor* motor = it->second;
            // Request fresh feedback; the reply is parsed asynchronously
            g_motor_control->refresh_motor_status(*motor);
            vTaskDelay(pdMS_TO_TICKS(10));
            damiao::motor_fb_param_t fb = g_motor_control->get_motor_feedback(*motor);
            float position_deg = fb.position * 180.0f / 3.14159265359f;
            printf("0x%02X      | %12.6f | %12.6f | %-10s | %s\n", master_id, fb.position, position_deg,
                   damiao::fault_to_string(g_motor_control->get_motor_fault(*motor)),
                   g_motor_control->is_motor_online(*motor) ? "yes" : "no");
        } else {
            ESP_LOGW(TAG, "Motor with master ID 0x%02X not found", master_id);
        }
    }

    return 0;
}

int panthera_fk_cmd(int argc, char **argv)
{
    // Fixed arm joints J1..J6 (gripper is not part of FK).
    static const uint32_t k_master_ids[6] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

    Joint joints; // defaults to all zeros
    printf("Joint | Master | Source              | Angle (rad) | Angle (deg)\n");
    printf("------+--------+---------------------+-------------+------------\n");

    for (int i = 0; i < 6; i++) {
        const uint32_t master_id = k_master_ids[i];
        damiao::Motor *motor = g_motor_control->get_motor_by_master_id(master_id);
        const char *source = "default 0 (missing)";
        float q = 0.0f;

        if (motor != nullptr) {
            g_motor_control->refresh_motor_status(*motor);
            vTaskDelay(pdMS_TO_TICKS(10));
            damiao::motor_fb_param_t fb = g_motor_control->get_motor_feedback(*motor);
            // Disabled / never enabled motors use kinematic angle 0.
            if (fb.state == 1) {
                // Match panthera_goto_position sign convention (J3 inverted on the bus).
                q = (master_id == 0x13) ? (-fb.position) : fb.position;
                source = "enabled feedback";
            } else {
                source = "default 0 (disabled)";
            }
        }

        joints[i] = q;
        printf("  J%d  | 0x%02X   | %-19s | %11.4f | %10.2f\n", i + 1, (unsigned int)master_id, source, q,
               RAD2DEG(q));
    }

    Kinematic kinematic;
    TransformMatrix pose = kinematic.forward(joints);
    Position p = pose.get_position();
    AxisAngle aa = pose.to_axis_angle();

    printf("\nFK end-effector:\n");
    printf("  xyz   : (%.4f, %.4f, %.4f) m\n", p.x, p.y, p.z);
    printf("  axis-angle: n=(%.3f, %.3f, %.3f), theta=%.3f rad (%.1f deg)\n", aa.x, aa.y, aa.z, aa.theta,
           RAD2DEG(aa.theta));
    pose.print();
    return 0;
}

int panthera_clear_error_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Clearing errors of all motors");

    const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = g_motor_control->get_motor_map();
    for (auto &pair : motor_map) {
        damiao::Motor *motor = pair.second;
        if (motor != nullptr) {
            g_motor_control->clear_error(*motor);
        }
    }
    ESP_LOGI(TAG, "Errors cleared; re-enable motors (panthera_enable on) to resume");
    return 0;
}

static void print_motor_status(damiao::Motor *motor)
{
    // Request fresh feedback; the reply is parsed asynchronously
    g_motor_control->refresh_motor_status(*motor);
    vTaskDelay(pdMS_TO_TICKS(20));

    damiao::motor_fb_param_t fb = g_motor_control->get_motor_feedback(*motor);
    int64_t age_ms = g_motor_control->get_ms_since_last_feedback(*motor);
    printf("Motor 0x%02X (slave 0x%02X):\n", (unsigned int)motor->get_master_id(), (unsigned int)motor->get_slave_id());
    printf("  position : %10.4f rad (%8.2f deg)\n", fb.position, fb.position * 180.0f / (float)M_PI);
    printf("  velocity : %10.4f rad/s\n", fb.velocity);
    printf("  torque   : %10.4f Nm\n", fb.torque);
    printf("  temp     : MOS %.0f C, rotor %.0f C\n", fb.temper_mos, fb.temper_rotor);
    printf("  state    : 0x%X (0=disabled, 1=enabled, 8..E=error), fault: %s\n", fb.state,
           damiao::fault_to_string(g_motor_control->get_motor_fault(*motor)));
    if (age_ms < 0) {
        printf("  feedback : NEVER RECEIVED (check CAN wiring/bitrate/IDs)\n");
    } else {
        printf("  feedback : last seen %lld ms ago\n", age_ms);
    }
}

int panthera_motor_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_motor_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_motor_args.end, argv[0]);
        return 1;
    }

    uint32_t master_id = (uint32_t)panthera_motor_args.master->ival[0];
    damiao::Motor *motor = g_motor_control->get_motor_by_master_id(master_id);

    if (motor == nullptr) {
        printf("Motor with master ID 0x%02X is not registered. Registered IDs:", (unsigned int)master_id);
        for (const auto &pair : g_motor_control->get_motor_map()) {
            printf(" 0x%02X", (unsigned int)pair.first);
        }
        printf("\n");
        return 1;
    }

    const char *action = panthera_motor_args.action->sval[0];

    if (strcmp(action, "enable") == 0) {
        if (!g_motor_control->switch_control_mode(*motor, damiao::POS_VEL_MODE)) {
            printf("Failed to select POS_VEL mode\n");
            return 1;
        }
        if (g_motor_control->enable_motor(*motor) != ESP_OK) {
            printf("Enable frame transmit failed\n");
            return 1;
        }
        printf("Enable frame sent (id 0x%03X), reading back:\n", (unsigned int)motor->get_slave_id());
        vTaskDelay(pdMS_TO_TICKS(100));
        print_motor_status(motor);
        return 0;
    } else if (strcmp(action, "disable") == 0) {
        if (g_motor_control->disable_motor(*motor) != ESP_OK) {
            printf("Disable frame transmit failed\n");
            return 1;
        }
        printf("Disable frame sent, reading back:\n");
        vTaskDelay(pdMS_TO_TICKS(100));
        print_motor_status(motor);
        return 0;
    } else if (strcmp(action, "clear") == 0) {
        if (g_motor_control->clear_error(*motor) != ESP_OK) {
            return 1;
        }
        printf("Error cleared, re-enable the motor to resume\n");
        return 0;
    } else if (strcmp(action, "zero") == 0) {
        if (g_motor_control->save_zero_position(*motor) != ESP_OK) {
            return 1;
        }
        printf("Current position saved as zero\n");
        return 0;
    } else if (strcmp(action, "status") == 0) {
        print_motor_status(motor);
        return 0;
    } else if (strcmp(action, "pos") == 0) {
        if (panthera_motor_args.pos->count == 0) {
            printf("Action 'pos' needs --pos <rad>\n");
            return 1;
        }
        float pos = (float)panthera_motor_args.pos->dval[0];
        float vel = (panthera_motor_args.vel->count > 0) ? (float)panthera_motor_args.vel->dval[0] : 2.0f;
        if (g_motor_control->pos_vel_control(*motor, pos, vel) != ESP_OK) {
            printf("Position command failed (is the motor enabled and in POS_VEL mode?)\n");
            return 1;
        }
        printf("Command sent: pos=%.4f rad, vel=%.2f rad/s\n", pos, vel);
        vTaskDelay(pdMS_TO_TICKS(500));
        print_motor_status(motor);
        return 0;
    }

    printf("Unknown action '%s'. Use: enable | disable | clear | zero | status | pos\n", action);
    return 1;
}

esp_err_t app_console_init(damiao::Motor_Control* motor_control, Manager* app_manager)
{
    ESP_RETURN_ON_FALSE(motor_control != nullptr, ESP_ERR_INVALID_ARG, TAG, "Motor control is nullptr");

    // Save motor_control and app_manager as global variables
    g_motor_control = motor_control;
    g_app_manager = app_manager;

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif
    // esp_console probes the terminal for escape-sequence support at boot and
    // waits 500 ms for a reply. Over USB-Serial-JTAG the host is rarely attached
    // yet, so the probe fails and the REPL runs in dumb mode: no arrow-key
    // history, no tab completion, no line editing. Every terminal we use
    // (idf.py monitor, picocom, minicom) speaks VT100, so force it on.
    linenoiseSetDumbMode(0);

    panthera_enable_args.enable = arg_str0(NULL, NULL, "<on|off>", "Enable or disable panthera");
    panthera_enable_args.end = arg_end(1);

    const esp_console_cmd_t panthera_enable = {
        .command = "panthera_enable",
        .help = "Enable panthera",
        .hint = NULL,
        .func = panthera_enable_cmd,
        .argtable = &panthera_enable_args,
    };

    panthera_zero_g_args.enable = arg_str0(NULL, NULL, "<on|off>", "Zero gravity mode");
    panthera_zero_g_args.end = arg_end(1);

    const esp_console_cmd_t panthera_zero_g = {
        .command = "panthera_zero_g",
        .help = "Enter or leave zero gravity mode, as the UI Zero Gravity switch does",
        .hint = NULL,
        .func = panthera_zero_g_cmd,
        .argtable = &panthera_zero_g_args,
    };

    const esp_console_cmd_t panthera_grasp = {
        .command = "panthera_grasp",
        .help = "Grasp the target the camera currently sees, as the UI Grasp button does",
        .hint = NULL,
        .func = panthera_grasp_cmd,
        .argtable = NULL,
    };

    const esp_console_cmd_t panthera_goto_zero = {
        .command = "panthera_goto_zero",
        .help = "Go to zero position for all motors",
        .hint = NULL,
        .func = panthera_goto_zero_cmd,
        .argtable = NULL,
    };

    const esp_console_cmd_t panthera_set_zero = {
        .command = "panthera_set_zero",
        .help = "Set zero position for all motors",
        .hint = NULL,
        .func = panthera_set_zero_cmd,
        .argtable = NULL,
    };

    panthera_move_args.x = arg_dbl0("x", "x", "x", "X position in meters");
    panthera_move_args.y = arg_dbl0("y", "y", "y", "Y position in meters");
    panthera_move_args.z = arg_dbl0("z", "z", "z", "Z position in meters");
    panthera_move_args.end = arg_end(3);

    const esp_console_cmd_t panthera_goto_position = {
        .command = "panthera_goto_position",
        .help = "Go to specified x, y, z position",
        .hint = NULL,
        .func = panthera_goto_position_cmd,
        .argtable = &panthera_move_args,
    };

    panthera_goto_pixel_args.u = arg_dbl0("u", "u", "<pixel>", "Image X coordinate in pixels");
    panthera_goto_pixel_args.v = arg_dbl0("v", "v", "<pixel>", "Image Y coordinate in pixels");
    panthera_goto_pixel_args.z = arg_dbl0("z", "z", "<meters>", "World Z in metres (default from panthera_gripper -z)");
    panthera_goto_pixel_args.dry_run = arg_lit0("n", "dry-run", "Print the solution without commanding the motors");
    panthera_goto_pixel_args.end = arg_end(4);

    const esp_console_cmd_t panthera_goto_pixel = {
        .command = "panthera_goto_pixel",
        // Supply the hint explicitly. Leaving it NULL makes esp_console_cmd_register
        // auto-generate one via arg_print_syntax_ds(), which corrupts memory for this
        // argtable and crashes the board a few seconds later, inside unrelated tasks.
        .help = "Map an image pixel to a world position with the stored vision matrix and go there",
        .hint = "-u <pixel> -v <pixel> [-z <meters>] [-n]",
        .func = panthera_goto_pixel_cmd,
        .argtable = &panthera_goto_pixel_args,
    };

    panthera_set_vision_matrix_args.m1 = arg_dbl0("1", "m1", "<value>", "Matrix element m1 (row 0, col 0)");
    panthera_set_vision_matrix_args.m2 = arg_dbl0("2", "m2", "<value>", "Matrix element m2 (row 0, col 1)");
    panthera_set_vision_matrix_args.m3 = arg_dbl0("3", "m3", "<value>", "Matrix element m3 (row 0, col 2)");
    panthera_set_vision_matrix_args.m4 = arg_dbl0("4", "m4", "<value>", "Matrix element m4 (row 1, col 0)");
    panthera_set_vision_matrix_args.m5 = arg_dbl0("5", "m5", "<value>", "Matrix element m5 (row 1, col 1)");
    panthera_set_vision_matrix_args.m6 = arg_dbl0("6", "m6", "<value>", "Matrix element m6 (row 1, col 2)");
    panthera_set_vision_matrix_args.m7 = arg_dbl0("7", "m7", "<value>", "Matrix element m7 (row 2, col 0)");
    panthera_set_vision_matrix_args.m8 = arg_dbl0("8", "m8", "<value>", "Matrix element m8 (row 2, col 1)");
    panthera_set_vision_matrix_args.m9 = arg_dbl0("9", "m9", "<value>", "Matrix element m9 (row 2, col 2)");
    panthera_set_vision_matrix_args.end = arg_end(10);

    const esp_console_cmd_t panthera_set_vision_matrix = {
        .command = "panthera_set_vision_matrix",
        .help = "Set vision matrix",
        .hint = NULL,
        .func = panthera_set_vision_matrix_cmd,
        .argtable = &panthera_set_vision_matrix_args,
    };

    const esp_console_cmd_t panthera_get_vision_matrix = {
        .command = "panthera_get_vision_matrix",
        .help = "Get vision matrix",
        .hint = NULL,
        .func = panthera_get_vision_matrix_cmd,
        .argtable = NULL,
    };

    const esp_console_cmd_t panthera_read_position = {
        .command = "panthera_read_position",
        .help = "Read current position and fault state of all motors",
        .hint = NULL,
        .func = panthera_read_position_cmd,
        .argtable = NULL,
    };

    const esp_console_cmd_t panthera_fk = {
        .command = "panthera_fk",
        .help = "Forward kinematics from current J1-J6; disabled/missing joints default to 0",
        .hint = NULL,
        .func = panthera_fk_cmd,
        .argtable = NULL,
    };

    const esp_console_cmd_t panthera_clear_error = {
        .command = "panthera_clear_error",
        .help = "Clear error state of all motors (re-enable afterwards)",
        .hint = NULL,
        .func = panthera_clear_error_cmd,
        .argtable = NULL,
    };

    panthera_motor_args.master = arg_int1("m", "master", "<master_id>", "Motor master ID, e.g. 0x11");
    panthera_motor_args.action = arg_str1("a", "action", "<action>", "enable | disable | clear | zero | status | pos");
    panthera_motor_args.pos = arg_dbl0("p", "pos", "<rad>", "Target position for action 'pos'");
    panthera_motor_args.vel = arg_dbl0("v", "vel", "<rad/s>", "Velocity for action 'pos' (default 2.0)");
    panthera_motor_args.end = arg_end(4);

    const esp_console_cmd_t panthera_motor = {
        .command = "panthera_motor",
        .help = "Control or inspect one registered motor",
        .hint = NULL,
        .func = panthera_motor_cmd,
        .argtable = &panthera_motor_args,
    };

    panthera_gripper_args.close = arg_dbl0("c", "close", "<rad>", "Gripper target used by grasp (default 1.44)");
    panthera_gripper_args.torque = arg_dbl0("t", "torque", "<Nm>", "Contact torque: hold as soon as |torque| exceeds it (default 0.04, 0 = off)");
    panthera_gripper_args.z = arg_dbl0("z", "z", "<m>", "Grasp height above base plane in metres (default 0.015)");
    panthera_gripper_args.end = arg_end(3);

    const esp_console_cmd_t panthera_gripper = {
        .command = "panthera_gripper",
        .help = "Show or set gripper close position, contact torque, and grasp height (saved to NVS)",
        .hint = "[-c <rad>] [-t <Nm>] [-z <m>]",
        .func = panthera_gripper_cmd,
        .argtable = &panthera_gripper_args,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_enable));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_goto_zero));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_zero_g));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_grasp));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_set_zero));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_goto_position));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_goto_pixel));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_set_vision_matrix));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_get_vision_matrix));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_read_position));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_fk));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_clear_error));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_motor));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_gripper));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    return ESP_OK;
}
#endif
