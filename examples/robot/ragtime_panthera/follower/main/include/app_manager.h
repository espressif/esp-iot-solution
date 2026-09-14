/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include "sdkconfig.h"

#include <atomic>
#include <vector>

#include "dm_motor.h"
#include "driver/jpeg_decode.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "greencube_detect.hpp"
#include "kinematic.h"
#include "lvgl.h"

#define MAX_SERVO_NUM 7

class Manager {
public:
    void set_grasp_z(float m);  /*!< Applies now and persists to NVS */
    float get_grasp_z() const
    {
        return grasp_z_m_;
    }
    static inline const Joint GRASP_IK_SEED{0.0, DEG2RAD(30.0), DEG2RAD(-36.0), DEG2RAD(65.0), 0.0, 0.0};

    static Manager* get_instance(damiao::Motor_Control* motor_control = nullptr);

    Manager(const Manager &) = delete;
    Manager &operator=(const Manager &) = delete;

    ~Manager() = default;

    damiao::Motor_Control* get_motor_control() const
    {
        return motor_control_;
    }

    esp_err_t register_esp_now_receiver(uart_port_t uart_port, int32_t baud_rate);

    /**
     * @brief Enable or disable the arm, exactly as the UI Enable switch does.
     *
     * Drives the motors, the motors_enabled_ gate, the health monitor and every
     * dependent widget, and moves ui_EnableSwitch to match. Safe to call from
     * any task: the LVGL work is done under app_lcd_lock, which is recursive,
     * so calling from the LVGL event handler is fine too.
     *
     * @param on true to enable, false to disable
     * @return ESP_OK, ESP_ERR_INVALID_STATE with no motor control, or the error
     *         from enable_all_motors()
     */
    esp_err_t set_motors_enabled(bool on);

    /**
     * @brief Turn ESP-NOW leader sync on or off, as the UI Sync switch does.
     *
     * @param on true to follow the leader arm
     * @return ESP_OK, or ESP_ERR_INVALID_STATE when motors are off or zero-g is running
     */
    esp_err_t set_sync_enabled(bool on);

    /**
     * @brief Enter or leave zero gravity mode, as the UI Zero Gravity switch does.
     *
     * Support the arm before enabling: the joints go to MIT mode and hold only
     * gravity feedforward.
     *
     * @param on true to enter zero gravity
     * @return ESP_OK, ESP_ERR_INVALID_STATE when motors are off, ESP_FAIL if entry failed
     */
    esp_err_t set_zero_g_enabled(bool on);

    /**
     * @brief Grasp the target the camera currently sees, as the UI Grasp button does.
     *
     * @return ESP_OK when a motion was queued, otherwise the reason it was not
     */
    esp_err_t trigger_grasp();
    /** Re-read the pixel-to-world matrix from NVS so a fresh panthera_set_vision_matrix applies without a reboot. */
    void reload_vision_matrix();
    void set_gripper_close_pos(float rad); /*!< Applies now and persists to NVS */
    float get_gripper_close_pos() const
    {
        return gripper_close_pos_;
    }
    void set_gripper_grip_torque(float nm); /*!< Applies now and persists to NVS; 0 disables the torque trigger */
    float get_gripper_grip_torque() const
    {
        return gripper_grip_torque_;
    }

    /**
     * @brief Drive every joint to zero, as the UI Go Zero button does.
     *
     * @return ESP_OK, or ESP_ERR_INVALID_STATE when motors are off or zero-g is running
     */
    esp_err_t go_zero();

private:
    Manager(damiao::Motor_Control* motor_control);

    /** Move a switch widget without re-entering its own event handler. */
    void set_switch_checked(lv_obj_t* sw, bool checked);

    static Manager* instance_;

    typedef struct {
        uint8_t* image_data;
        size_t image_size;
    } detect_queue_msg_t;

    enum class GraspState {
        IDLE,
        OPEN_GRIPPER,
        WAIT_OPEN,
        MOVE_TO_TARGET,
        WAIT_ARRIVE,
        CLOSE_GRIPPER,
        WAIT_CLOSE,
        MOVE_TO_RELEASE_POSITION,
        WAIT_ARRIVE_RELEASE,
        RELEASE_GRIPPER,
        WAIT_RELEASE,
        RETURN_HOME,
        WAIT_HOME,
        COMPLETED
    };

    damiao::Motor_Control* motor_control_;
    double version_matrix_[3][3];
    portMUX_TYPE vision_matrix_lock_ = portMUX_INITIALIZER_UNLOCKED;

    jpeg_decoder_handle_t jpeg_decoder_handle_;
    jpeg_decode_cfg_t jpeg_decode_cfg_;
    uint8_t* jpeg_decode_buffer_;
    size_t jpeg_decode_buffer_size_;
    uint32_t jpeg_decoded_size_;

    GreenCubeDetect* detector_;
    std::list<dl::detect::result_t> detect_results_;
    dl::detect::result_t selected_target_;

    TaskHandle_t lcd_refresh_task_handle_;
    static void lcd_refresh_task(void* arg);
    TaskHandle_t detect_task_handle_;
    static void detect_task(void* arg);
    TaskHandle_t grasp_task_handle_;
    static void grasp_task(void* arg);
    TaskHandle_t motor_status_task_handle_;
    static void motor_status_task(void* arg);
    void update_motor_status_ui();

    TaskHandle_t zero_g_task_handle_;
    std::atomic_bool zero_g_enabled_;
    static void zero_g_task(void* arg);
    bool enter_zero_g();
    void exit_zero_g();
    bool switch_arm_control_mode(damiao::Control_Mode mode);
    void set_zero_g_ui_enabled(bool enabled);

    static constexpr uint32_t k_motor_master_ids_[MAX_SERVO_NUM] = {
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
    };
    static constexpr const char* k_motor_names_[MAX_SERVO_NUM] = {
        "J1", "J2", "J3", "J4", "J5", "J6", "Grip"
    };
    lv_obj_t* motor_detail_backdrop_;
    lv_obj_t* motor_detail_title_;
    lv_obj_t* motor_detail_info_;
    int motor_detail_index_;
    void open_motor_detail(int index);
    void close_motor_detail();
    void refresh_motor_detail_info();
    static void motor_chip_clicked_event_handler(lv_event_t* event);
    static void motor_detail_event_handler(lv_event_t* event);

    QueueHandle_t detect_queue_;
    QueueHandle_t grasp_command_queue_;
    SemaphoreHandle_t detect_results_mutex_;

    // One buffer can be under inference while the other holds the newest frame.
    // The pending queue entry is replaced instead of accumulating stale frames.
    static constexpr int DETECT_BUFFER_POOL_SIZE = 2;
    uint8_t* detect_buffer_pool_[DETECT_BUFFER_POOL_SIZE];
    QueueHandle_t detect_buffer_pool_queue_;

    std::atomic<GraspState> grasp_state_;
    Joint target_joint_;
    Joint release_joint_;
    bool lifted_ = false; /*!< Grasp carry: straight lift done, next move is to the release spot */
    float target_gripper_pos_;
    float gripper_close_pos_ = 1.44f;                  /*!< Gripper target when grasping, rad; panthera_gripper tunes it, NVS keeps it */
    float grasp_z_m_ = 0.015f;                           /*!< Gripper height at the cube, m above the base plane; panthera_gripper -z */
    float gripper_grip_torque_ = 0.05f;                /*!< measured: free 0.01, on cube 0.06-0.07. |torque| above this = jaws on the cube, hold now. 0 = off; panthera_gripper -t */
    void gripper_cmd(damiao::Motor &motor, float pos); /*!< Every gripper move goes through here */
    static constexpr float POSITION_TOLERANCE = 0.05f;
    static constexpr float GRIPPER_TOLERANCE = 0.1f;
    static constexpr TickType_t GRIPPER_TIMEOUT_MS = 5000;
    static constexpr float GRIPPER_HOLD_SQUEEZE = CONFIG_GRIPPER_HOLD_SQUEEZE / 1000.0f;
    static constexpr TickType_t ARM_MOVE_TIMEOUT_MS = 15000;
    static constexpr uint32_t HEALTH_TIMEOUT_MS = 1500;  /*!< Grace period before declaring a motor OFFLINE */
    static constexpr float LIFT_M = CONFIG_LIFT_M / 1000.0f;
    static constexpr TickType_t RELEASE_SETTLE_MS = 400; /*!< Stand still this long at the release spot before opening */
    std::atomic_bool motors_enabled_;

    static void motor_fault_handler(uint32_t master_id, damiao::Motor_Fault fault, void* user_ctx);
    void abort_motion_and_reset_ui();
    void set_motor_action_buttons_enabled(bool enabled);

    /** Arm joints participating in MIT gravity compensation (J1..J6). Gripper excluded. */
    static constexpr int k_zero_g_joint_count_ = 6;

    static void lvgl_button_clicked_event_handler(lv_event_t* event);
    static void lvgl_switch_event_handler(lv_event_t* event);

    bool esp_now_receiver_registered_;
    uart_port_t esp_now_uart_port_;
    TaskHandle_t esp_now_receiver_task_handle_;
    static void esp_now_receiver_task(void* arg);
    uint8_t esp_now_buffer_[1024];
    float esp_now_servo_angles_[MAX_SERVO_NUM];
    std::atomic_bool esp_now_sync_enabled_;
};
