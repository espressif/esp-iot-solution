/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "dm_motor.h"
#include "esp_log.h"

namespace damiao {

#define P_MIN -12.5f
#define P_MAX 12.5f
#define V_MIN -30.0f
#define V_MAX 30.0f
#define KP_MIN 0.0f
#define KP_MAX 500.0f
#define KD_MIN 0.0f
#define KD_MAX 5.0f
#define T_MIN -10.0f
#define T_MAX 10.0f

static Limit_param_t g_limit_param_table[Num_Of_Motor] = {
    {12.5, 30, 10 }, // DM4310
    {12.5, 50, 10 }, // DM4310_48V
    {12.5, 8, 28 },  // DM4340
    {12.5, 10, 28 }, // DM4340_48V
    {12.5, 45, 20 }, // DM6006
    {12.5, 45, 40 }, // DM8006
    {12.5, 45, 54 }, // DM8009
    {12.5, 25,  200}, // DM10010L
    {12.5, 20, 200}, // DM10010
    {12.5, 280, 1},  // DMH3510
    {12.5, 45, 10},  // DMH6215
    {12.5, 45, 10}   // DMG6220
};

Motor::Motor(DM_Motor_Type Motor_Type, uint32_t Slave_id, uint32_t Master_id)
    : motor_fb_param({0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}), motor_control_mode(POS_VEL_MODE), mode_id(0),
limit_param(g_limit_param_table[Motor_Type]), slave_id(Slave_id), master_id(Master_id), motor_type(Motor_Type)
{
    update_mode_id();
}

void Motor::update_mode_id()
{
    switch (motor_control_mode) {
    case MIT_MODE:
        mode_id = 0x000;
        break;
    case POS_VEL_MODE:
        mode_id = 0x100;
        break;
    case VEL_MODE:
        mode_id = 0x200;
        break;
    case POS_FORCE_MODE:
        mode_id = 0x300;
        break;
    default:
        mode_id = 0x000;
        break;
    }
}

uint32_t Motor::get_slave_id() const
{
    return slave_id;
}

uint32_t Motor::get_master_id() const
{
    return master_id;
}

DM_Motor_Type Motor::get_motor_type() const
{
    return motor_type;
}

Motor_Control* Motor_Control::instance_ = nullptr;

Motor_Control::Motor_Control(gpio_num_t tx, gpio_num_t rx)
    : tx_pin_(tx), rx_pin_(rx), initialized_(false), twai_node_(nullptr), twai_data_queue_(nullptr), mutex_(nullptr)
{
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Failed to create mutex");
    }
}

Motor_Control* Motor_Control::getInstance(gpio_num_t tx, gpio_num_t rx)
{
    if (instance_ == nullptr) {
        instance_ = new Motor_Control(tx, rx);
    }
    return instance_;
}

IRAM_ATTR bool Motor_Control::twai_rx_done_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    Motor_Control *motor = static_cast<Motor_Control *>(user_ctx);
    if (motor == nullptr || motor->twai_data_queue_ == nullptr) {
        return false;
    }

    static uint8_t recv_buff[TWAI_FRAME_MAX_LEN] = {0};
    twai_frame_t rx_frame = {};
    rx_frame.buffer = recv_buff;
    rx_frame.buffer_len = sizeof(recv_buff);

    // Receive data from ISR
    if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
        // Send data to queue (from ISR) - copies the structure and buffer pointer
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(motor->twai_data_queue_, &rx_frame, &xHigherPriorityTaskWoken);

        // Trigger task switch if needed
        if (xHigherPriorityTaskWoken) {
            return true;
        }
    }

    return false;
}

int Motor_Control::float_to_unit(float x_float, float x_min, float x_max, int bits)
{
    float span = x_max - x_min;
    float offset = x_min;
    return (int)((x_float - offset) * ((float)((1 << bits) - 1)) / span);
}

float Motor_Control::unit_to_float(int x_unit, float x_min, float x_max, int bits)
{
    float span = x_max - x_min;
    float offset = x_min;
    return ((float)x_unit) * span / ((float)((1 << bits) - 1)) + offset;
}

void Motor_Control::twai_data_task(void *args)
{
    Motor_Control *motor_control = static_cast<Motor_Control *>(args);
    twai_frame_t rx_frame = {0};

    while (1) {
        if (xQueueReceive(motor_control->twai_data_queue_, &rx_frame, portMAX_DELAY) == pdTRUE) {
            if (rx_frame.buffer_len == TWAI_FRAME_MAX_LEN) {
                uint32_t master_id = rx_frame.header.id;

                // Find motor by master ID (no lock needed for read-only access)
                auto it = motor_control->motor_map_.find(master_id);
                if (it != motor_control->motor_map_.end()) {
                    Motor *motor = it->second;
                    if (motor != nullptr) {
                        ESP_LOGD("dm_motor", "Received data for motor with master ID: 0x%08X", master_id);

                        // Parse CAN data and update motor feedback parameters
                        motor->motor_fb_param.state = rx_frame.buffer[0] >> 4;

                        // Check for error states and log them
                        switch (motor->motor_fb_param.state) {
                        case 8:
                            ESP_LOGE("dm_motor", "Motor 0x%08X: Over voltage error", master_id);
                            break;
                        case 9:
                            ESP_LOGE("dm_motor", "Motor 0x%08X: Under voltage error", master_id);
                            break;
                        case 0xA:
                            ESP_LOGE("dm_motor", "Motor 0x%08X: Over current error", master_id);
                            break;
                        case 0xB:
                            ESP_LOGE("dm_motor", "Motor 0x%08X: MOS over temperature error", master_id);
                            break;
                        case 0xC:
                            ESP_LOGE("dm_motor", "Motor 0x%08X: Motor coil over temperature error", master_id);
                            break;
                        case 0xD:
                            ESP_LOGE("dm_motor", "Motor 0x%08X: Communication lost error", master_id);
                            break;
                        case 0xE:
                            ESP_LOGE("dm_motor", "Motor 0x%08X: Overload error", master_id);
                            break;
                        default:
                            // Normal state, no error
                            break;
                        }

                        int position = (rx_frame.buffer[1] << 8) | (rx_frame.buffer[2]);
                        motor->motor_fb_param.position = motor_control->unit_to_float(position, P_MIN, P_MAX, 16);
                        int velocity = (rx_frame.buffer[3] << 4) | (rx_frame.buffer[4] >> 4);
                        motor->motor_fb_param.velocity = motor_control->unit_to_float(velocity, V_MIN, V_MAX, 12);
                        int torque = ((rx_frame.buffer[4] & 0xF) << 8) | (rx_frame.buffer[5]);
                        motor->motor_fb_param.torque = motor_control->unit_to_float(torque, T_MIN, T_MAX, 12);

                        motor->motor_fb_param.temper_mos = (float)rx_frame.buffer[6];
                        motor->motor_fb_param.temper_rotor = (float)rx_frame.buffer[7];
                    }
                } else {
                    ESP_LOGW("dm_motor", "Unknown motor master ID: 0x%08X", master_id);
                }
            }
        }
    }
}

esp_err_t Motor_Control::init()
{
    if (initialized_) {
        return ESP_OK;
    }

    twai_onchip_node_config_t node_config = {};
    node_config.io_cfg.tx = tx_pin_;
    node_config.io_cfg.rx = rx_pin_;
    node_config.bit_timing.bitrate = 1000000;
    node_config.fail_retry_cnt = 10;
    node_config.tx_queue_depth = 10;

    esp_err_t ret = twai_new_node_onchip(&node_config, &twai_node_);
    if (ret != ESP_OK) {
        ESP_LOGE("dm_motor", "Failed to create TWAI node");
        return ret;
    }

    twai_event_callbacks_t callbacks = {};
    callbacks.on_rx_done = twai_rx_done_cb;

    ret = twai_node_register_event_callbacks(twai_node_, &callbacks, this);
    if (ret != ESP_OK) {
        ESP_LOGE("dm_motor", "Failed to register TWAI event callbacks");
        return ret;
    }

    twai_data_queue_ = xQueueCreate(20, sizeof(twai_frame_t));
    if (twai_data_queue_ == nullptr) {
        ESP_LOGE("dm_motor", "Failed to create twai_data_queue");
        return ESP_ERR_NO_MEM;
    }

    ret = twai_node_enable(twai_node_);
    if (ret != ESP_OK) {
        ESP_LOGE("dm_motor", "Failed to enable TWAI node");
        vQueueDelete(twai_data_queue_);
        twai_data_queue_ = nullptr;
        return ret;
    }

    BaseType_t task_ret = xTaskCreate(twai_data_task, "twai_data_task", 4096, this, 5, nullptr);
    if (task_ret != pdPASS) {
        ESP_LOGE("dm_motor", "Failed to create twai_data_task");
        vQueueDelete(twai_data_queue_);
        twai_data_queue_ = nullptr;
        if (mutex_ != nullptr) {
            vSemaphoreDelete(mutex_);
            mutex_ = nullptr;
        }
        return ESP_FAIL;
    }

    initialized_ = true;
    ESP_LOGI("dm_motor", "Motor control initialized successfully");
    return ESP_OK;
}

void Motor_Control::add_motor(Motor *motor)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (motor->get_master_id() != 0x00) {
            motor_map_.insert({motor->get_master_id(), motor});
        }
        xSemaphoreGive(mutex_);
    }
}

Motor* Motor_Control::get_motor_by_master_id(uint32_t master_id)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return nullptr;
    }

    Motor* motor = nullptr;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        auto it = motor_map_.find(master_id);
        if (it != motor_map_.end()) {
            motor = it->second;
        }
        xSemaphoreGive(mutex_);
    }
    return motor;
}

bool Motor_Control::switch_control_mode(Motor &motor, Control_Mode control_mode)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return false;
    }

    // Update motor mode (requires lock for thread safety)
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        motor.motor_control_mode = control_mode;
        motor.update_mode_id();
        xSemaphoreGive(mutex_);
    } else {
        return false;
    }

    // Call write_motor_param outside the lock to avoid deadlock
    // (write_motor_param will acquire the lock internally)
    uint8_t write_data[4] = {(uint8_t)control_mode, 0x00, 0x00, 0x00};
    esp_err_t ret = write_motor_param(motor, 10, write_data);

    if (ret != ESP_OK) {
        ESP_LOGE("dm_motor", "Failed to write motor parameter");
        return false;
    }
    return true;
}

bool Motor_Control::switch_control_mode(uint32_t master_id, Control_Mode control_mode)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return false;
    }
    return switch_control_mode(*motor, control_mode);
}

esp_err_t Motor_Control::refresh_motor_status(Motor &motor)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        uint32_t id = 0x7FF;
        uint8_t can_low = motor.get_slave_id() & 0xff;
        uint8_t can_high = (motor.get_slave_id() >> 8) & 0xff;
        uint8_t data_buf[4] = {can_low, can_high, 0xcc, 0x00};

        twai_frame_t tx_frame = {};
        tx_frame.header.id = id;
        tx_frame.buffer = (uint8_t *) data_buf;
        tx_frame.buffer_len = sizeof(data_buf);
        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Motor_Control::refresh_motor_status(uint32_t master_id)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return refresh_motor_status(*motor);
}

esp_err_t Motor_Control::enable_motor(Motor &motor)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI("dm_motor", "Enabling motor with ID: 0x%08X", motor.get_slave_id() + motor.mode_id);

        uint8_t data_buf[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc};

        twai_frame_t tx_frame = {};
        tx_frame.header.id = motor.get_slave_id() + motor.mode_id;
        tx_frame.buffer = (uint8_t *) data_buf;
        tx_frame.buffer_len = sizeof(data_buf);

        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Motor_Control::enable_motor(uint32_t master_id)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return enable_motor(*motor);
}

esp_err_t Motor_Control::disable_motor(Motor &motor)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        ESP_LOGD("dm_motor", "Disabling motor with ID: 0x%08X", motor.get_slave_id() + motor.mode_id);

        uint8_t data_buf[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd};
        twai_frame_t tx_frame = {};
        tx_frame.header.id = motor.get_slave_id() + motor.mode_id;
        tx_frame.buffer = (uint8_t *) data_buf;
        tx_frame.buffer_len = sizeof(data_buf);

        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Motor_Control::disable_motor(uint32_t master_id)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return disable_motor(*motor);
}

esp_err_t Motor_Control::enable_all_motors()
{
    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    // No need to lock motor_map_ here - enable_motor will handle its own locking
    esp_err_t ret = ESP_OK;
    for (auto &pair : motor_map_) {
        Motor *motor = pair.second;
        if (motor != nullptr) {
            esp_err_t err = enable_motor(*motor);
            if (err != ESP_OK) {
                ESP_LOGE("dm_motor", "Failed to enable motor with master ID: 0x%08X", pair.first);
                ret = ESP_FAIL;
            }
        }
    }
    return ret;
}

esp_err_t Motor_Control::disable_all_motors()
{
    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    // No need to lock motor_map_ here - disable_motor will handle its own locking
    esp_err_t ret = ESP_OK;
    for (auto &pair : motor_map_) {
        Motor *motor = pair.second;
        if (motor != nullptr) {
            esp_err_t err = disable_motor(*motor);
            if (err != ESP_OK) {
                ESP_LOGE("dm_motor", "Failed to disable motor with master ID: 0x%08X", pair.first);
                ret = ESP_FAIL;
            }
        }
    }
    return ret;
}

esp_err_t Motor_Control::pos_vel_control(Motor &motor, float position, float velocity)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (motor.motor_control_mode != POS_VEL_MODE) {
        ESP_LOGE("dm_motor", "Motor control mode is not POS_VEL_MODE, current mode: %d", motor.motor_control_mode);
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        uint8_t *pbuf, *vbuf;
        uint8_t data[8];

        uint32_t id = motor.get_slave_id() + motor.mode_id;
        pbuf = (uint8_t *)&position;
        vbuf = (uint8_t *)&velocity;

        data[0] = *pbuf;
        data[1] = *(pbuf + 1);
        data[2] = *(pbuf + 2);
        data[3] = *(pbuf + 3);
        data[4] = *vbuf;
        data[5] = *(vbuf + 1);
        data[6] = *(vbuf + 2);
        data[7] = *(vbuf + 3);

        twai_frame_t tx_frame = {};
        tx_frame.header.id = id;
        tx_frame.buffer = (uint8_t *)data;
        tx_frame.buffer_len = sizeof(data);

        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Motor_Control::pos_vel_control(uint32_t master_id, float position, float velocity)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return pos_vel_control(*motor, position, velocity);
}

esp_err_t Motor_Control::vel_control(Motor &motor, float velocity)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (motor.motor_control_mode != VEL_MODE) {
        ESP_LOGE("dm_motor", "Motor control mode is not VEL_MODE, current mode: %d", motor.motor_control_mode);
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        uint32_t id = motor.get_slave_id() + motor.mode_id;
        uint8_t *vbuf;
        uint8_t data[4];

        vbuf = (uint8_t *)&velocity;
        data[0] = *vbuf;
        data[1] = *(vbuf + 1);
        data[2] = *(vbuf + 2);
        data[3] = *(vbuf + 3);

        twai_frame_t tx_frame = {};
        tx_frame.header.id = id;
        tx_frame.buffer = (uint8_t *)data;
        tx_frame.buffer_len = sizeof(data);

        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }

        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Motor_Control::vel_control(uint32_t master_id, float velocity)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return vel_control(*motor, velocity);
}

esp_err_t Motor_Control::mit_control(Motor &motor, float position, float velocity, float kp, float kd, float torque)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (motor.motor_control_mode != MIT_MODE) {
        ESP_LOGE("dm_motor", "Motor control mode is not MIT_MODE, current mode: %d", motor.motor_control_mode);
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        uint8_t data[8];
        int pos_tmp, vel_tmp, kp_tmp, kd_tmp, tor_tmp;
        uint32_t id = motor.get_slave_id() + motor.mode_id;

        pos_tmp = float_to_unit(position, P_MIN, P_MAX, 16);
        vel_tmp = float_to_unit(velocity, V_MIN, V_MAX, 12);
        tor_tmp = float_to_unit(torque, T_MIN, T_MAX, 12);
        kp_tmp = float_to_unit(kp, KP_MIN, KP_MAX, 12);
        kd_tmp = float_to_unit(kd, KD_MIN, KD_MAX, 12);

        data[0] = (pos_tmp >> 8);
        data[1] = pos_tmp;
        data[2] = (vel_tmp >> 4);
        data[3] = ((vel_tmp & 0xF) << 4) | ((kp_tmp >> 8));
        data[4] = kp_tmp;
        data[5] = (kd_tmp >> 4) ;
        data[6] = ((kd_tmp & 0xF) << 4) | ((tor_tmp >> 8));
        data[7] = tor_tmp;

        twai_frame_t tx_frame = {};
        tx_frame.header.id = id;
        tx_frame.buffer = (uint8_t *)data;
        tx_frame.buffer_len = sizeof(data);

        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }

        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Motor_Control::mit_control(uint32_t master_id, float position, float velocity, float kp, float kd, float torque)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return mit_control(*motor, position, velocity, kp, kd, torque);
}

esp_err_t Motor_Control::save_zero_position(Motor &motor)
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        uint32_t id = motor.get_slave_id() + motor.mode_id;
        uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};

        twai_frame_t tx_frame = {};
        tx_frame.header.id = id;
        tx_frame.buffer = (uint8_t *)data;
        tx_frame.buffer_len = sizeof(data);

        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t Motor_Control::save_zero_position(uint32_t master_id)
{
    Motor *motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE("dm_motor", "Motor with master ID 0x%08X not found", master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return save_zero_position(*motor);
}

esp_err_t Motor_Control::write_motor_param(Motor &motor, uint8_t rid, const uint8_t data[4])
{
    if (mutex_ == nullptr) {
        ESP_LOGE("dm_motor", "Mutex not initialized");
        return ESP_FAIL;
    }

    if (twai_node_ == nullptr) {
        ESP_LOGE("dm_motor", "TWAI node is not initialized");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        uint32_t id = motor.get_slave_id();
        uint8_t can_low = id & 0xff;
        uint8_t can_high = (id >> 8) & 0xff;
        uint8_t data_buf[8] = {can_low, can_high, 0x55, rid, 0x00, 0x00, 0x00, 0x00};
        for (int i = 0; i < 4; i++) {
            data_buf[i + 4] = data[i];
        }

        twai_frame_t tx_frame = {};
        tx_frame.header.id = 0x7ff;
        tx_frame.buffer = (uint8_t *) data_buf;
        tx_frame.buffer_len = sizeof(data_buf);

        esp_err_t ret = twai_node_transmit(twai_node_, &tx_frame, pdMS_TO_TICKS(100));
        xSemaphoreGive(mutex_);

        if (ret != ESP_OK) {
            ESP_LOGE("dm_motor", "Failed to transmit TWAI frame");
            return ESP_FAIL;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        return ESP_OK;
    }
    return ESP_FAIL;
}

const std::unordered_map<uint32_t, Motor*> &Motor_Control::get_motor_map() const
{
    return motor_map_;
}

}
