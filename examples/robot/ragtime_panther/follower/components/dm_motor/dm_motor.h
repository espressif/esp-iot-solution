/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <unordered_map>
#include "driver/gpio.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

namespace damiao {

enum DM_Motor_Type {
    DM4310,
    DM4310_48V,
    DM4340,
    DM4340_48V,
    DM6006,
    DM8006,
    DM8009,
    DM10010L,
    DM10010,
    DMH3510,
    DMH6215,
    DMG6220,
    Num_Of_Motor
};

enum Control_Mode {
    MIT_MODE = 1,
    POS_VEL_MODE = 2,
    VEL_MODE = 3,
    POS_FORCE_MODE = 4,
};

typedef struct {
    float Q_MAX;
    float DQ_MAX;
    float TAU_MAX;
} Limit_param_t;

typedef struct {
    int state;
    float position;
    float velocity;
    float torque;
    float temper_mos;
    float temper_rotor;
} motor_fb_param_t;

class Motor {
public:
    Motor(DM_Motor_Type Motor_Type, uint32_t Slave_id, uint32_t Master_id);
    uint32_t get_slave_id() const;
    uint32_t get_master_id() const;
    DM_Motor_Type get_motor_type() const;
    void update_mode_id();
    motor_fb_param_t motor_fb_param;
    Control_Mode motor_control_mode;
    uint32_t mode_id;
private:
    Limit_param_t limit_param;
    uint32_t slave_id;
    uint32_t master_id;
    DM_Motor_Type motor_type;
};

class Motor_Control {
public:
    static Motor_Control* getInstance(gpio_num_t tx, gpio_num_t rx);
    esp_err_t init();

    Motor_Control(const Motor_Control &) = delete;
    Motor_Control &operator=(const Motor_Control &) = delete;

    void add_motor(Motor *motor);
    Motor* get_motor_by_master_id(uint32_t master_id);
    bool switch_control_mode(Motor &motor, Control_Mode control_mode);
    bool switch_control_mode(uint32_t master_id, Control_Mode control_mode);
    esp_err_t refresh_motor_status(Motor &motor);
    esp_err_t refresh_motor_status(uint32_t master_id);
    esp_err_t enable_motor(Motor &motor);
    esp_err_t enable_motor(uint32_t master_id);
    esp_err_t disable_motor(Motor &motor);
    esp_err_t disable_motor(uint32_t master_id);
    esp_err_t enable_all_motors();
    esp_err_t disable_all_motors();
    esp_err_t pos_vel_control(Motor &motor, float position, float velocity);
    esp_err_t pos_vel_control(uint32_t master_id, float position, float velocity);
    esp_err_t vel_control(Motor &motor, float velocity);
    esp_err_t vel_control(uint32_t master_id, float velocity);
    esp_err_t mit_control(Motor &motor, float position, float velocity, float kp, float kd, float torque);
    esp_err_t mit_control(uint32_t master_id, float position, float velocity, float kp, float kd, float torque);
    esp_err_t save_zero_position(Motor &motor);
    esp_err_t save_zero_position(uint32_t master_id);
    esp_err_t write_motor_param(Motor &motor, uint8_t rid, const uint8_t data[4]);
    const std::unordered_map<uint32_t, Motor*> &get_motor_map() const;

    gpio_num_t getTxPin() const
    {
        return tx_pin_;
    }
    gpio_num_t getRxPin() const
    {
        return rx_pin_;
    }

private:
    Motor_Control(gpio_num_t tx, gpio_num_t rx);
    static IRAM_ATTR bool twai_rx_done_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx);
    static void twai_data_task(void *args);
    static int float_to_unit(float x_float, float x_min, float x_max, int bits);
    static float unit_to_float(int x_unit, float x_min, float x_max, int bits);
    static Motor_Control* instance_;
    gpio_num_t tx_pin_;
    gpio_num_t rx_pin_;
    bool initialized_;
    twai_node_handle_t twai_node_;
    QueueHandle_t twai_data_queue_;
    SemaphoreHandle_t mutex_;
    std::unordered_map<uint32_t, Motor*> motor_map_;
};

};
