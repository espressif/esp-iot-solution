/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <vector>
#include <algorithm>
#include "argtable3/argtable3.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "kinematic.h"
#include "app_console.h"

#if CONFIG_CONSOLE_CONTROL
static const char* TAG = "app_console";

// Global motor control instance
static damiao::Motor_Control* g_motor_control = nullptr;

static struct {
    struct arg_str *enable;
    struct arg_end *end;
} panthera_enable_args;

static struct {
    struct arg_dbl *x;
    struct arg_dbl *y;
    struct arg_dbl *z;
    struct arg_end *end;
} panthera_move_args;

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

int panthera_enable_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &panthera_enable_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_enable_args.end, argv[0]);
        return 1;
    }

    if (panthera_enable_args.enable->count > 0) {
        if (strcmp(panthera_enable_args.enable->sval[0], "on") == 0) {
            ESP_LOGI(TAG, "Panthera enabled");
            g_motor_control->enable_all_motors();
        } else if (strcmp(panthera_enable_args.enable->sval[0], "off") == 0) {
            ESP_LOGI(TAG, "Panthera disabled");
            g_motor_control->disable_all_motors();
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

int panthera_goto_zero_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Panthera going to zero position");

    const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = g_motor_control->get_motor_map();
    for (auto &pair : motor_map) {
        damiao::Motor *motor = pair.second;
        if (motor != nullptr) {
            g_motor_control->pos_vel_control(*motor, 0.0f, 1.0f);
        }
    }
    return 0;
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

int panthera_goto_position_cmd(int argc, char **argv)
{
    ESP_LOGI(TAG, "Panthera going to position");

    int nerrors = arg_parse(argc, argv, (void **) &panthera_move_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, panthera_move_args.end, argv[0]);
        return 1;
    }

    if (panthera_move_args.x->count > 0 && panthera_move_args.y->count > 0 && panthera_move_args.z->count > 0) {
        float x = static_cast<float>(panthera_move_args.x->dval[0]);
        float y = static_cast<float>(panthera_move_args.y->dval[0]);
        float z = static_cast<float>(panthera_move_args.z->dval[0]);

        ESP_LOGI(TAG, "Panthera going to position (%f, %f, %f)", x, y, z);

        Kinematic kinematic;
        Joint j1 = Joint(0.0, DEG2RAD(30.0), DEG2RAD(-36.0), DEG2RAD(65.0), 0.0, 0.0);
        TransformMatrix t1;
        kinematic.solve_forward_kinematics(j1, t1);
        t1.print();

        TransformMatrix t2 = t1;
        t2(0, 3) = x;
        t2(1, 3) = y;
        t2(2, 3) = z;
        Joint j2 = j1;
        kinematic.solve_inverse_kinematics(t2, j2);
        TransformMatrix t3;
        kinematic.solve_forward_kinematics(j2, t3);
        t3.print();

        float error_x = t3(0, 3) - x;
        float error_y = t3(1, 3) - y;
        float error_z = t3(2, 3) - z;
        if (fabs(error_x) > 0.01 || fabs(error_y) > 0.01 || fabs(error_z) > 0.01) {
            ESP_LOGI(TAG, "Panthera position not reached");
            return 1;
        }
        ESP_LOGI(TAG, "Panthera position reached");

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
                    g_motor_control->pos_vel_control(*motor, j2[i] * -1.0f, 4.0f);
                } else {
                    g_motor_control->pos_vel_control(*motor, j2[i], 2.0f);
                }
            } else {
                ESP_LOGW(TAG, "Motor with master ID 0x%02X (joint %d) not found", master_id, i);
            }
        }

        return 0;
    } else {
        ESP_LOGI(TAG, "Panthera position not specified");
        return 1;
    }
    return 0;
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
        char key[8];
        snprintf(key, sizeof(key), "m%d", i);
        // Convert float to int by multiplying by 1000000
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
    printf("Master ID | Position (rad) | Position (deg)\n");
    printf("----------|----------------|---------------\n");

    for (uint32_t master_id : motor_ids) {
        auto it = motor_map.find(master_id);
        if (it != motor_map.end() && it->second != nullptr) {
            damiao::Motor* motor = it->second;
            // Refresh motor status to get latest position
            g_motor_control->refresh_motor_status(*motor);
            float position_rad = motor->motor_fb_param.position;
            float position_deg = position_rad * 180.0f / 3.14159265359f;
            printf("0x%02X      | %12.6f | %12.6f\n", master_id, position_rad, position_deg);
        } else {
            ESP_LOGW(TAG, "Motor with master ID 0x%02X not found", master_id);
        }
    }

    return 0;
}

esp_err_t app_console_init(damiao::Motor_Control* motor_control)
{
    ESP_RETURN_ON_FALSE(motor_control != nullptr, ESP_ERR_INVALID_ARG, TAG, "Motor control is nullptr");

    // Save motor_control as global variable
    g_motor_control = motor_control;

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

    panthera_enable_args.enable = arg_str0(NULL, NULL, "<on|off>", "Enable or disable panthera");
    panthera_enable_args.end = arg_end(1);

    const esp_console_cmd_t panthera_enable = {
        .command = "panthera_enable",
        .help = "Enable panthera",
        .hint = NULL,
        .func = panthera_enable_cmd,
        .argtable = &panthera_enable_args,
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
        .help = "Read current position of all motors",
        .hint = NULL,
        .func = panthera_read_position_cmd,
        .argtable = NULL,
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_enable));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_goto_zero));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_set_zero));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_goto_position));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_set_vision_matrix));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_get_vision_matrix));
    ESP_ERROR_CHECK(esp_console_cmd_register(&panthera_read_position));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    return ESP_OK;
}
#endif
