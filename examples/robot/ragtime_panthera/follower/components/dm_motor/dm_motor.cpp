/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "dm_motor.h"

#include <cstring>

#include "esp_log.h"
#include "esp_rom_sys.h"

namespace damiao {

static const char* TAG = "dm_motor";

// kp/kd ranges in MIT frames. Part of the protocol, same for every motor.
#define KP_MIN 0.0f
#define KP_MAX 500.0f
#define KD_MIN 0.0f
#define KD_MAX 5.0f

// Register 10 is CTRL_MODE; switch_control_mode() writes it.
#define REG_CTRL_MODE 10

static const Limit_param_t g_limit_param_table[Num_Of_Motor] = {
    {12.5, 30, 10},   // DM4310
    {12.5, 50, 10},   // DM4310_48V
    {12.5, 8, 28},    // DM4340
    {12.5, 10, 28},   // DM4340_48V
    {12.5, 45, 20},   // DM6006
    {12.5, 45, 40},   // DM8006
    {12.5, 45, 54},   // DM8009
    {12.5, 25, 200},  // DM10010L
    {12.5, 20, 200},  // DM10010
    {12.5, 280, 1},   // DMH3510
    {12.5, 45, 10},   // DMH6215
    {12.5, 45, 10}    // DMG6220
};

const char* fault_to_string(Motor_Fault fault)
{
    switch (fault) {
    case Motor_Fault::NONE:
        return "NONE";
    case Motor_Fault::OVER_VOLTAGE:
        return "OVER_VOLTAGE";
    case Motor_Fault::UNDER_VOLTAGE:
        return "UNDER_VOLTAGE";
    case Motor_Fault::OVER_CURRENT:
        return "OVER_CURRENT";
    case Motor_Fault::MOS_OVER_TEMP:
        return "MOS_OVER_TEMP";
    case Motor_Fault::COIL_OVER_TEMP:
        return "COIL_OVER_TEMP";
    case Motor_Fault::COMM_LOST:
        return "COMM_LOST";
    case Motor_Fault::OVERLOAD:
        return "OVERLOAD";
    case Motor_Fault::OFFLINE:
        return "OFFLINE";
    default:
        return "UNKNOWN";
    }
}

Motor::Motor(DM_Motor_Type Motor_Type, uint32_t Slave_id, uint32_t Master_id)
    : limit_param(g_limit_param_table[Motor_Type]), slave_id(Slave_id), master_id(Master_id), motor_type(Motor_Type)
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

const Limit_param_t &Motor::get_limit_param() const
{
    return limit_param;
}

Motor_Control* Motor_Control::instance_ = nullptr;

Motor_Control::Motor_Control(gpio_num_t tx, gpio_num_t rx)
    : tx_pin_(tx), rx_pin_(rx)
{
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
    }
    tx_idle_sem_ = xSemaphoreCreateBinary();
    if (tx_idle_sem_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create TX gate semaphore");
    } else {
        xSemaphoreGive(tx_idle_sem_);
    }
}

Motor_Control* Motor_Control::getInstance(gpio_num_t tx, gpio_num_t rx)
{
    if (instance_ == nullptr) {
        instance_ = new Motor_Control(tx, rx);
    }
    return instance_;
}

IRAM_ATTR bool Motor_Control::twai_rx_done_cb(twai_node_handle_t handle, const twai_rx_done_event_data_t* edata, void* user_ctx)
{
    Motor_Control* motor = static_cast<Motor_Control*>(user_ctx);
    if (motor == nullptr || motor->twai_data_queue_ == nullptr) {
        return false;
    }

    uint8_t recv_buff[TWAI_FRAME_MAX_LEN] = {0};
    twai_frame_t rx_frame = {};
    rx_frame.buffer = recv_buff;
    rx_frame.buffer_len = sizeof(recv_buff);

    if (twai_node_receive_from_isr(handle, &rx_frame) == ESP_OK) {
        // Copy it out, recv_buff lives on the ISR stack.
        rx_msg_t msg = {};
        msg.id = rx_frame.header.id;
        msg.len = (rx_frame.buffer_len > sizeof(msg.data)) ? sizeof(msg.data) : rx_frame.buffer_len;
        for (uint8_t i = 0; i < msg.len; i++) {
            msg.data[i] = recv_buff[i];
        }

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(motor->twai_data_queue_, &msg, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            return true;
        }
    }

    return false;
}

IRAM_ATTR bool Motor_Control::twai_tx_done_cb(twai_node_handle_t handle, const twai_tx_done_event_data_t* edata, void* user_ctx)
{
    Motor_Control* self = static_cast<Motor_Control*>(user_ctx);
    if (self == nullptr || edata->done_tx_frame == nullptr) {
        return false;
    }

    tx_slot_t* slot = (tx_slot_t*)edata->done_tx_frame;
    if (slot >= self->tx_pool_ && slot < self->tx_pool_ + TX_POOL_SIZE) {
        portENTER_CRITICAL_ISR(&self->tx_pool_lock_);
        slot->in_use = false;
        portEXIT_CRITICAL_ISR(&self->tx_pool_lock_);
    }

    // Frame is off the wire, let the next transmit() through.
    BaseType_t woken = pdFALSE;
    if (self->tx_idle_sem_ != nullptr) {
        xSemaphoreGiveFromISR(self->tx_idle_sem_, &woken);
    }
    return woken == pdTRUE;
}

IRAM_ATTR bool Motor_Control::twai_state_change_cb(twai_node_handle_t handle, const twai_state_change_event_data_t* edata, void* user_ctx)
{
    Motor_Control* self = static_cast<Motor_Control*>(user_ctx);
    if (self != nullptr) {
        bool bus_off = (edata->new_sta == TWAI_ERROR_BUS_OFF);
        if (bus_off) {
            // On bus-off IDF drops the in-flight frame and never calls TX-done for it.
            tx_slot_t* oldest = nullptr;
            portENTER_CRITICAL_ISR(&self->tx_pool_lock_);
            for (int i = 0; i < TX_POOL_SIZE; i++) {
                tx_slot_t* slot = &self->tx_pool_[i];
                if (slot->in_use && (oldest == nullptr ||
                                     (int32_t)(slot->sequence - oldest->sequence) < 0)) {
                    oldest = slot;
                }
            }
            if (oldest != nullptr) {
                oldest->in_use = false;
            }
            portEXIT_CRITICAL_ISR(&self->tx_pool_lock_);

            // So nobody else will reopen the gate. Do it here.
            BaseType_t woken = pdFALSE;
            if (self->tx_idle_sem_ != nullptr) {
                xSemaphoreGiveFromISR(self->tx_idle_sem_, &woken);
            }
            self->bus_off_.store(bus_off);
            return woken == pdTRUE;
        }
        self->bus_off_.store(bus_off);
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

bool Motor_Control::set_fault_locked(Motor &motor, Motor_Fault fault)
{
    if (motor.fault == fault) {
        return false;
    }
    motor.fault = fault;
    return true;
}

void Motor_Control::notify_fault(uint32_t master_id, Motor_Fault fault)
{
    if (fault == Motor_Fault::NONE) {
        ESP_LOGI(TAG, "Motor 0x%08X: fault recovered", (unsigned int)master_id);
    } else {
        ESP_LOGE(TAG, "Motor 0x%08X: fault %s", (unsigned int)master_id, fault_to_string(fault));
    }
    if (fault_cb_ != nullptr) {
        fault_cb_(master_id, fault, fault_cb_ctx_);
    }
}

void Motor_Control::handle_feedback_frame(const rx_msg_t &msg)
{
    bool fault_changed = false;
    Motor_Fault new_fault = Motor_Fault::NONE;
    uint32_t master_id = msg.id;

    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return;
    }

    auto it = motor_map_.find(master_id);
    if (it == motor_map_.end() || it->second == nullptr) {
        xSemaphoreGive(mutex_);
        ESP_LOGW(TAG, "Unknown motor master ID: 0x%08X", (unsigned int)master_id);
        return;
    }

    Motor* motor = it->second;
    const Limit_param_t &limit = motor->limit_param;

    motor->motor_fb_param.state = msg.data[0] >> 4;

    // Feedback scaling depends on the motor model.
    int position = (msg.data[1] << 8) | (msg.data[2]);
    motor->motor_fb_param.position = unit_to_float(position, -limit.Q_MAX, limit.Q_MAX, 16);
    int velocity = (msg.data[3] << 4) | (msg.data[4] >> 4);
    motor->motor_fb_param.velocity = unit_to_float(velocity, -limit.DQ_MAX, limit.DQ_MAX, 12);
    int torque = ((msg.data[4] & 0xF) << 8) | (msg.data[5]);
    motor->motor_fb_param.torque = unit_to_float(torque, -limit.TAU_MAX, limit.TAU_MAX, 12);
    motor->motor_fb_param.temper_mos = (float)msg.data[6];
    motor->motor_fb_param.temper_rotor = (float)msg.data[7];

    motor->last_feedback_tick = xTaskGetTickCount();
    motor->offline_strikes = 0;
    motor->ever_seen = true;

    if (bus_off_episodes_.exchange(0) > 0) {
        ESP_LOGI(TAG, "CAN communication established");
    }

    // States 0x8..0xE are faults the motor reports itself. Any frame at all clears our OFFLINE.
    int state = motor->motor_fb_param.state;
    new_fault = (state >= 0x8 && state <= 0xE) ? (Motor_Fault)state : Motor_Fault::NONE;
    fault_changed = set_fault_locked(*motor, new_fault);

    xSemaphoreGive(mutex_);

    if (fault_changed) {
        notify_fault(master_id, new_fault);
    }
}

void Motor_Control::twai_data_task(void* args)
{
    Motor_Control* motor_control = static_cast<Motor_Control*>(args);
    rx_msg_t msg;

    while (1) {
        if (xQueueReceive(motor_control->twai_data_queue_, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (msg.len != TWAI_FRAME_MAX_LEN) {
            continue;
        }
        // Replies on 0x7FF are register reads; we don't use them.
        if (msg.id != 0x7FF) {
            motor_control->handle_feedback_frame(msg);
        }
    }
}

void Motor_Control::health_monitor_task(void* args)
{
    Motor_Control* self = static_cast<Motor_Control*>(args);

    while (1) {
        // Bus off: keep trying to recover, backing off, until something answers.
        if (self->bus_off_.load()) {
            if (!self->bus_recover_requested_) {
                uint32_t episodes = self->bus_off_episodes_.fetch_add(1) + 1;
                if (episodes == 1) {
                    ESP_LOGE(TAG, "TWAI bus off (no CAN device answering, check wiring/transceiver), retrying quietly");
                }

                uint32_t offline_ids[16];
                int offline_count = 0;
                if (self->offline_detection_.load() && xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
                    for (auto &pair : self->motor_map_) {
                        if (pair.second != nullptr && pair.second->fault == Motor_Fault::NONE &&
                                offline_count < (int)(sizeof(offline_ids) / sizeof(offline_ids[0])) &&
                                self->set_fault_locked(*pair.second, Motor_Fault::OFFLINE)) {
                            offline_ids[offline_count++] = pair.first;
                        }
                    }
                    xSemaphoreGive(self->mutex_);
                }
                for (int i = 0; i < offline_count; i++) {
                    self->notify_fault(offline_ids[i], Motor_Fault::OFFLINE);
                }

                self->bus_recover_requested_ = true;
                esp_err_t ret = twai_node_recover(self->twai_node_);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "TWAI recovery request failed: %s", esp_err_to_name(ret));
                    self->bus_recover_requested_ = false;
                }
            }
            uint32_t backoff_ms = self->bus_off_episodes_.load() * 1000;
            if (backoff_ms > 30000) {
                backoff_ms = 30000;
            }
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            continue;
        }
        if (self->bus_recover_requested_) {
            self->bus_recover_requested_ = false;
        }

        // Recovery stays on even with offline detection off.
        if (!self->offline_detection_.load()) {
            vTaskDelay(pdMS_TO_TICKS(self->health_period_ms_.load()));
            continue;
        }

        Motor* motors[16];
        int count = 0;
        if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
            for (auto &pair : self->motor_map_) {
                if (pair.second != nullptr && count < (int)(sizeof(motors) / sizeof(motors[0]))) {
                    motors[count++] = pair.second;
                }
            }
            xSemaphoreGive(self->mutex_);
        }

        for (int i = 0; i < count; i++) {
            self->refresh_motor_status(*motors[i]);
        }

        vTaskDelay(pdMS_TO_TICKS(self->health_period_ms_.load()));

        TickType_t now = xTaskGetTickCount();
        for (int i = 0; i < count; i++) {
            Motor* motor = motors[i];
            bool fault_changed = false;
            uint32_t master_id = 0;
            uint8_t strikes = 0;
            if (xSemaphoreTake(self->mutex_, portMAX_DELAY) == pdTRUE) {
                // Don't overwrite a fault the motor told us about.
                if (motor->fault == Motor_Fault::NONE) {
                    if ((now - motor->last_feedback_tick) > pdMS_TO_TICKS(self->health_timeout_ms_.load())) {
                        if (motor->offline_strikes < 255) {
                            motor->offline_strikes++;
                        }
                        strikes = motor->offline_strikes;
                        master_id = motor->get_master_id();
                        if (motor->offline_strikes >= k_offline_confirm_strikes_) {
                            fault_changed = self->set_fault_locked(*motor, Motor_Fault::OFFLINE);
                            motor->offline_strikes = 0;
                        }
                    } else {
                        motor->offline_strikes = 0;
                    }
                }
                xSemaphoreGive(self->mutex_);
            }
            if (fault_changed) {
                self->notify_fault(master_id, Motor_Fault::OFFLINE);
            } else if (strikes > 0 && strikes < k_offline_confirm_strikes_) {
                ESP_LOGW(TAG, "Motor 0x%08X: no feedback, retry %u/%u", (unsigned int)master_id,
                         (unsigned int)strikes, (unsigned int)k_offline_confirm_strikes_);
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
        ESP_LOGE(TAG, "Failed to create TWAI node");
        return ret;
    }

    twai_event_callbacks_t callbacks = {};
    callbacks.on_rx_done = twai_rx_done_cb;
    callbacks.on_tx_done = twai_tx_done_cb;
    callbacks.on_state_change = twai_state_change_cb;

    ret = twai_node_register_event_callbacks(twai_node_, &callbacks, this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register TWAI event callbacks");
        return ret;
    }

    twai_data_queue_ = xQueueCreate(20, sizeof(rx_msg_t));
    if (twai_data_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create twai_data_queue");
        return ESP_ERR_NO_MEM;
    }

    ret = twai_node_enable(twai_node_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable TWAI node");
        vQueueDelete(twai_data_queue_);
        twai_data_queue_ = nullptr;
        return ret;
    }

    BaseType_t task_ret = xTaskCreate(twai_data_task, "twai_data_task", 4096, this, 5, nullptr);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create twai_data_task");
        vQueueDelete(twai_data_queue_);
        twai_data_queue_ = nullptr;
        return ESP_FAIL;
    }

    // The monitor task also does bus recovery, so it runs even with detection off.
    if (xTaskCreate(health_monitor_task, "motor_health", 4096, this, 4, &health_task_) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create health monitor task");
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Motor control initialized successfully");
    return ESP_OK;
}

void Motor_Control::add_motor(Motor* motor)
{
    if (mutex_ == nullptr || motor == nullptr) {
        ESP_LOGE(TAG, "Mutex not initialized or motor is null");
        return;
    }

    if (motor->get_master_id() == 0x00) {
        ESP_LOGE(TAG, "Refusing to add motor with master ID 0x00 (set a non-zero MasterID)");
        return;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        motor->last_feedback_tick = xTaskGetTickCount();
        motor_map_.insert({motor->get_master_id(), motor});
        xSemaphoreGive(mutex_);
    }
}

Motor* Motor_Control::get_motor_by_master_id(uint32_t master_id)
{
    if (mutex_ == nullptr) {
        ESP_LOGE(TAG, "Mutex not initialized");
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

motor_fb_param_t Motor_Control::get_motor_feedback(Motor &motor)
{
    motor_fb_param_t fb = {};
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        fb = motor.motor_fb_param;
        xSemaphoreGive(mutex_);
    }
    return fb;
}

Motor_Fault Motor_Control::get_motor_fault(Motor &motor)
{
    Motor_Fault fault = Motor_Fault::OFFLINE;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        fault = motor.fault;
        xSemaphoreGive(mutex_);
    }
    return fault;
}

Motor_Fault Motor_Control::get_motor_fault(uint32_t master_id)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        return Motor_Fault::OFFLINE;
    }
    return get_motor_fault(*motor);
}

bool Motor_Control::is_motor_online(Motor &motor)
{
    bool online = false;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        online = motor.ever_seen && motor.fault != Motor_Fault::OFFLINE;
        xSemaphoreGive(mutex_);
    }
    return online;
}

int64_t Motor_Control::get_ms_since_last_feedback(Motor &motor)
{
    int64_t ms = -1;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (motor.ever_seen) {
            ms = (int64_t)pdTICKS_TO_MS(xTaskGetTickCount() - motor.last_feedback_tick);
        }
        xSemaphoreGive(mutex_);
    }
    return ms;
}

void Motor_Control::register_fault_callback(motor_fault_cb_t cb, void* user_ctx)
{
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        fault_cb_ = cb;
        fault_cb_ctx_ = user_ctx;
        xSemaphoreGive(mutex_);
    }
}

esp_err_t Motor_Control::start_health_monitor(uint32_t period_ms, uint32_t timeout_ms)
{
    if (!initialized_) {
        ESP_LOGE(TAG, "Motor control not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (period_ms == 0 || timeout_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    health_period_ms_ = period_ms;
    health_timeout_ms_ = timeout_ms;

    if (health_task_ == nullptr) {
        if (xTaskCreate(health_monitor_task, "motor_health", 4096, this, 4, &health_task_) != pdPASS) {
            ESP_LOGE(TAG, "Failed to create health monitor task");
            return ESP_FAIL;
        }
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        TickType_t now = xTaskGetTickCount();
        for (auto &pair : motor_map_) {
            if (pair.second != nullptr) {
                pair.second->ever_seen = false;
                pair.second->last_feedback_tick = now;
                if (pair.second->fault == Motor_Fault::OFFLINE) {
                    pair.second->fault = Motor_Fault::NONE;
                }
            }
        }
        xSemaphoreGive(mutex_);
    }

    offline_detection_.store(true);
    ESP_LOGI(TAG, "Motor offline detection enabled (period %ums, timeout %ums)",
             (unsigned int)period_ms, (unsigned int)timeout_ms);
    return ESP_OK;
}

void Motor_Control::stop_health_monitor()
{
    if (offline_detection_.exchange(false)) {
        ESP_LOGI(TAG, "Motor offline detection disabled");
    }
}

esp_err_t Motor_Control::transmit(uint32_t id, const uint8_t* data, size_t len)
{
    if (tx_idle_sem_ == nullptr) {
        ESP_LOGE(TAG, "TX gate not initialized");
        return ESP_FAIL;
    }
    if (twai_node_ == nullptr) {
        ESP_LOGE(TAG, "TWAI node is not initialized");
        return ESP_FAIL;
    }
    if (bus_off_.load()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (len > sizeof(tx_pool_[0].data)) {
        return ESP_ERR_INVALID_ARG;
    }

    // One frame in flight at a time. The gate is a binary semaphore that only
    // the TX-done (or bus-off) ISR gives back, so twai_node_transmit() always
    // finds the hardware idle and takes its fast path.
    //
    // Tried twai_node_transmit_wait_all_done() here. It waits on
    // an event group bit that the ISR hands to the timer daemon task, and that
    // task barely gets scheduled in this app, so the wait times out even though
    // the bus went idle long ago.
    if (xSemaphoreTake(tx_idle_sem_, pdMS_TO_TICKS(100)) != pdTRUE) {
        // 100 ms without a TX-done. Either a frame nobody ACKs, or we lost the
        // wakeup. A stuck gate would take every motor OFFLINE, so log what the
        // controller thinks and carry on; the next TX-done reopens the gate.
        if (!bus_off_.load()) {
            twai_node_status_t st = {};
            twai_node_get_info(twai_node_, &st, nullptr);
            ESP_LOGE(TAG, "TWAI TX still busy after 100ms (state %d, tec %u, rec %u), forcing frame (id 0x%08X)",
                     (int)st.state, (unsigned)st.tx_error_count, (unsigned)st.rx_error_count, (unsigned int)id);
        }
    }

    // The driver keeps the frame pointer until TX-done, so it can't be a stack buffer.
    tx_slot_t* slot = nullptr;
    portENTER_CRITICAL(&tx_pool_lock_);
    for (int i = 0; i < TX_POOL_SIZE; i++) {
        if (!tx_pool_[i].in_use) {
            tx_pool_[i].in_use = true;
            tx_pool_[i].sequence = tx_next_sequence_++;
            slot = &tx_pool_[i];
            break;
        }
    }
    portEXIT_CRITICAL(&tx_pool_lock_);

    if (slot == nullptr) {
        xSemaphoreGive(tx_idle_sem_);
        if (!tx_pool_warned_) {
            tx_pool_warned_ = true;
            ESP_LOGW(TAG, "TX frame pool exhausted, dropping frames (id 0x%08X)", (unsigned int)id);
        }
        return ESP_ERR_NO_MEM;
    }
    tx_pool_warned_ = false;

    memcpy(slot->data, data, len);
    slot->frame = {};
    slot->frame.header.id = id;
    slot->frame.buffer = slot->data;
    slot->frame.buffer_len = len;

    // Small race in the IDF 6.1 driver. The ISR gives our gate from on_tx_done
    // but clears its own hw_busy flag a few hundred ns later. If we wake on the
    // other core inside that gap we hit the driver's busy path, and there is an
    // ordering where our frame lands in its queue after the ISR has already
    // checked it, with nothing left to start it. It then sits there until the
    // next transmit, which is how the "TX still busy" log above used to fire on
    // an otherwise healthy bus. So wait for the driver to say idle first.
    // wait_all_done with timeout 0 is just a flag check, no event group involved.
    // If the hardware really is busy we give up after 200 us and queue as before.
    int spin_us = 0;
    static constexpr int TX_IDLE_SPIN_LIMIT_US = 200;
    while (twai_node_transmit_wait_all_done(twai_node_, 0) != ESP_OK && spin_us < TX_IDLE_SPIN_LIMIT_US) {
        esp_rom_delay_us(1);
        spin_us++;
    }
    esp_err_t ret = twai_node_transmit(twai_node_, &slot->frame, pdMS_TO_TICKS(10));
    if (ret != ESP_OK) {
        // Never queued, so no TX-done is coming. Give the slot and gate back ourselves.
        portENTER_CRITICAL(&tx_pool_lock_);
        slot->in_use = false;
        portEXIT_CRITICAL(&tx_pool_lock_);
        xSemaphoreGive(tx_idle_sem_);
    }

    if (ret != ESP_OK) {
        if (!bus_off_.load()) {
            ESP_LOGE(TAG, "Failed to transmit TWAI frame (id 0x%08X): %s", (unsigned int)id, esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t Motor_Control::send_control_cmd(Motor &motor, uint8_t cmd)
{
    // Only motion commands get the mode offset added to the ID.
    uint8_t data_buf[8] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, cmd};
    return transmit(motor.get_slave_id(), data_buf, sizeof(data_buf));
}

bool Motor_Control::switch_control_mode(Motor &motor, Control_Mode control_mode)
{
    uint8_t write_data[4] = {(uint8_t)control_mode, 0x00, 0x00, 0x00};
    if (write_motor_param(motor, REG_CTRL_MODE, write_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write control mode");
        return false;
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        motor.motor_control_mode = control_mode;
        motor.update_mode_id();
        // Feedback pauses for a moment after a mode change. Don't count that as a strike.
        motor.last_feedback_tick = xTaskGetTickCount();
        motor.offline_strikes = 0;
        xSemaphoreGive(mutex_);
    }
    return true;
}

bool Motor_Control::switch_control_mode(uint32_t master_id, Control_Mode control_mode)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return false;
    }
    return switch_control_mode(*motor, control_mode);
}

esp_err_t Motor_Control::refresh_motor_status(Motor &motor)
{
    uint8_t can_low = motor.get_slave_id() & 0xff;
    uint8_t can_high = (motor.get_slave_id() >> 8) & 0xff;
    uint8_t data_buf[4] = {can_low, can_high, 0xcc, 0x00};
    return transmit(0x7FF, data_buf, sizeof(data_buf));
}

esp_err_t Motor_Control::refresh_motor_status(uint32_t master_id)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return refresh_motor_status(*motor);
}

esp_err_t Motor_Control::enable_motor(Motor &motor)
{
    ESP_LOGI(TAG, "Enabling motor with ID: 0x%08X", (unsigned int)motor.get_slave_id());
    esp_err_t ret = send_control_cmd(motor, 0xfc);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t Motor_Control::enable_motor(uint32_t master_id)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return enable_motor(*motor);
}

esp_err_t Motor_Control::disable_motor(Motor &motor)
{
    ESP_LOGD(TAG, "Disabling motor with ID: 0x%08X", (unsigned int)motor.get_slave_id());
    esp_err_t ret = send_control_cmd(motor, 0xfd);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t Motor_Control::disable_motor(uint32_t master_id)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return disable_motor(*motor);
}

esp_err_t Motor_Control::clear_error(Motor &motor)
{
    ESP_LOGI(TAG, "Clearing error of motor 0x%08X", (unsigned int)motor.get_master_id());
    esp_err_t ret = send_control_cmd(motor, 0xfb);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

esp_err_t Motor_Control::clear_error(uint32_t master_id)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return clear_error(*motor);
}

static void sort_motors_by_master_id(Motor* motors[], int count)
{
    // Enable/disable in ID order so it's the same every time.
    for (int i = 1; i < count; i++) {
        Motor* key = motors[i];
        int j = i - 1;
        while (j >= 0 && motors[j]->get_master_id() > key->get_master_id()) {
            motors[j + 1] = motors[j];
            j--;
        }
        motors[j + 1] = key;
    }
}

esp_err_t Motor_Control::enable_all_motors()
{
    Motor* motors[16];
    int count = 0;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        for (auto &pair : motor_map_) {
            if (pair.second != nullptr && count < (int)(sizeof(motors) / sizeof(motors[0]))) {
                motors[count++] = pair.second;
            }
        }
        xSemaphoreGive(mutex_);
    }
    sort_motors_by_master_id(motors, count);

    esp_err_t ret = ESP_OK;
    for (int i = 0; i < count; i++) {
        if (enable_motor(*motors[i]) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable motor with master ID: 0x%08X", (unsigned int)motors[i]->get_master_id());
            ret = ESP_FAIL;
        }
    }
    return ret;
}

esp_err_t Motor_Control::disable_all_motors()
{
    Motor* motors[16];
    int count = 0;
    if (mutex_ != nullptr && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        for (auto &pair : motor_map_) {
            if (pair.second != nullptr && count < (int)(sizeof(motors) / sizeof(motors[0]))) {
                motors[count++] = pair.second;
            }
        }
        xSemaphoreGive(mutex_);
    }
    sort_motors_by_master_id(motors, count);

    esp_err_t ret = ESP_OK;
    for (int i = 0; i < count; i++) {
        if (disable_motor(*motors[i]) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disable motor with master ID: 0x%08X", (unsigned int)motors[i]->get_master_id());
            ret = ESP_FAIL;
        }
    }
    return ret;
}

esp_err_t Motor_Control::pos_vel_control(Motor &motor, float position, float velocity)
{
    if (motor.motor_control_mode != POS_VEL_MODE) {
        ESP_LOGE(TAG, "Motor control mode is not POS_VEL_MODE, current mode: %d", motor.motor_control_mode);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[8];
    memcpy(data, &position, sizeof(float));
    memcpy(data + 4, &velocity, sizeof(float));
    return transmit(motor.get_slave_id() + motor.mode_id, data, sizeof(data));
}

esp_err_t Motor_Control::pos_vel_control(uint32_t master_id, float position, float velocity)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return pos_vel_control(*motor, position, velocity);
}

esp_err_t Motor_Control::vel_control(Motor &motor, float velocity)
{
    if (motor.motor_control_mode != VEL_MODE) {
        ESP_LOGE(TAG, "Motor control mode is not VEL_MODE, current mode: %d", motor.motor_control_mode);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t data[4];
    memcpy(data, &velocity, sizeof(float));
    return transmit(motor.get_slave_id() + motor.mode_id, data, sizeof(data));
}

esp_err_t Motor_Control::vel_control(uint32_t master_id, float velocity)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return vel_control(*motor, velocity);
}

esp_err_t Motor_Control::mit_control(Motor &motor, float position, float velocity, float kp, float kd, float torque)
{
    if (motor.motor_control_mode != MIT_MODE) {
        ESP_LOGE(TAG, "Motor control mode is not MIT_MODE, current mode: %d", motor.motor_control_mode);
        return ESP_ERR_INVALID_STATE;
    }

    const Limit_param_t &limit = motor.get_limit_param();
    int pos_tmp = float_to_unit(position, -limit.Q_MAX, limit.Q_MAX, 16);
    int vel_tmp = float_to_unit(velocity, -limit.DQ_MAX, limit.DQ_MAX, 12);
    int tor_tmp = float_to_unit(torque, -limit.TAU_MAX, limit.TAU_MAX, 12);
    int kp_tmp = float_to_unit(kp, KP_MIN, KP_MAX, 12);
    int kd_tmp = float_to_unit(kd, KD_MIN, KD_MAX, 12);

    uint8_t data[8];
    data[0] = (pos_tmp >> 8);
    data[1] = pos_tmp;
    data[2] = (vel_tmp >> 4);
    data[3] = ((vel_tmp & 0xF) << 4) | ((kp_tmp >> 8));
    data[4] = kp_tmp;
    data[5] = (kd_tmp >> 4);
    data[6] = ((kd_tmp & 0xF) << 4) | ((tor_tmp >> 8));
    data[7] = tor_tmp;

    return transmit(motor.get_slave_id() + motor.mode_id, data, sizeof(data));
}

esp_err_t Motor_Control::mit_control(uint32_t master_id, float position, float velocity, float kp, float kd, float torque)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return mit_control(*motor, position, velocity, kp, kd, torque);
}

esp_err_t Motor_Control::save_zero_position(Motor &motor)
{
    esp_err_t ret = send_control_cmd(motor, 0xfe);
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

esp_err_t Motor_Control::save_zero_position(uint32_t master_id)
{
    Motor* motor = get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        ESP_LOGE(TAG, "Motor with master ID 0x%08X not found", (unsigned int)master_id);
        return ESP_ERR_NOT_FOUND;
    }
    return save_zero_position(*motor);
}

esp_err_t Motor_Control::write_motor_param(Motor &motor, uint8_t rid, const uint8_t data[4])
{
    uint32_t id = motor.get_slave_id();
    uint8_t can_low = id & 0xff;
    uint8_t can_high = (id >> 8) & 0xff;
    uint8_t data_buf[8] = {can_low, can_high, 0x55, rid, data[0], data[1], data[2], data[3]};

    esp_err_t ret = transmit(0x7ff, data_buf, sizeof(data_buf));
    if (ret != ESP_OK) {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

const std::unordered_map<uint32_t, Motor*> &Motor_Control::get_motor_map() const
{
    return motor_map_;
}

}  // namespace damiao
