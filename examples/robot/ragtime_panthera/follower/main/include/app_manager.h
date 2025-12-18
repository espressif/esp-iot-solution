/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "color_detect.hpp"
#include "driver/jpeg_decode.h"
#include "driver/uart.h"
#include "dm_motor.h"

#define MAX_SERVO_NUM 7

class Manager {
public:
    static Manager* get_instance(damiao::Motor_Control* motor_control = nullptr);

    // Delete copy constructor and assignment operator
    Manager(const Manager &) = delete;
    Manager &operator=(const Manager &) = delete;

    ~Manager() = default;

    damiao::Motor_Control* get_motor_control() const
    {
        return motor_control_;
    }

    esp_err_t register_esp_now_receiver(uart_port_t uart_port, int32_t baud_rate);

private:
    Manager(damiao::Motor_Control* motor_control);

    static Manager* instance_;

    typedef struct {
        uint8_t* image_data;
        size_t image_size;
    } color_detect_queue_msg_t;

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

    // Motor control
    damiao::Motor_Control* motor_control_;
    double version_matrix_[3][3];

    // JPEG decoder handle
    jpeg_decoder_handle_t jpeg_decoder_handle_;
    jpeg_decode_cfg_t jpeg_decode_cfg_;
    uint8_t* jpeg_decode_buffer_;
    size_t jpeg_decode_buffer_size_;
    uint32_t jpeg_decoded_size_;

    // AI related
    ColorDetect* color_detect_;
    std::list<dl::detect::result_t> detect_results_;
    dl::detect::result_t selected_target_;               /*!< Selected target when button is clicked */

    // Task handles
    TaskHandle_t lcd_refresh_task_handle_;
    static void lcd_refresh_task(void* arg);
    TaskHandle_t color_detect_task_handle_;
    static void color_detect_task(void* arg);
    TaskHandle_t grasp_task_handle_;
    static void grasp_task(void* arg);
    QueueHandle_t color_detect_queue_;
    QueueHandle_t grasp_command_queue_;
    SemaphoreHandle_t detect_results_mutex_;

    // Buffer pool for color detection (avoid frequent malloc/free)
    static constexpr int COLOR_DETECT_BUFFER_POOL_SIZE = 3;
    uint8_t* color_detect_buffer_pool_[COLOR_DETECT_BUFFER_POOL_SIZE];
    QueueHandle_t color_detect_buffer_pool_queue_;  // Queue of available buffers

    // Grasp task state
    GraspState grasp_state_;
    Joint target_joint_;                                 /*!< Target joint angles for grasping */
    Joint release_joint_;                                /*!< Joint angles for release position */
    float target_gripper_pos_;                           /*!< Target gripper position */
    static constexpr float POSITION_TOLERANCE = 0.05f;   /*!< Position tolerance in radians */
    static constexpr float GRIPPER_TOLERANCE = 0.1f;     /*!< Gripper position tolerance */
    bool motors_enabled_;                                 /*!< Whether motors are enabled via switch */

    // LVGL related
    static void lvgl_button_clicked_event_handler(lv_event_t *event);
    static void lvgl_switch_event_handler(lv_event_t *event);

    // ESP-NOW related
    bool esp_now_receiver_registered_;
    uart_port_t esp_now_uart_port_;
    TaskHandle_t esp_now_receiver_task_handle_;
    static void esp_now_receiver_task(void* arg);
    uint8_t esp_now_buffer_[1024];
    float esp_now_servo_angles_[MAX_SERVO_NUM];  /*!< Received servo angles from ESP-NOW (in radians) */
    bool esp_now_sync_enabled_;
};
