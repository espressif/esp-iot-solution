/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "app_manager.h"

#include "app_lcd.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "gravity_comp.h"
#include "kinematic.h"
#include "lvgl.h"
#include "math.h"
#include "nvs_flash.h"
#include "string.h"
#include "ui/screens/ui_RobotGraspScreen.h"
#include "ui/ui.h"
#include "usb_camera.h"

#define MAX_SERVO_NUM 7

static const char* TAG = "app_manager";
Manager* Manager::instance_ = nullptr;

Manager* Manager::get_instance(damiao::Motor_Control* motor_control)
{
    if (instance_ == nullptr) {
        if (motor_control == nullptr) {
            return nullptr;
        }
        instance_ = new Manager(motor_control);
    }
    return instance_;
}

static void grasp_nvs_save(const char* key, float value)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open("grasp", NVS_READWRITE, &h);
    if (ret == ESP_OK) {
        ret = nvs_set_i32(h, key, (int32_t)(value * 1000000.0f));
        if (ret == ESP_OK) {
            ret = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "%s not saved: %s", key, esp_err_to_name(ret));
    }
}

void Manager::set_grasp_z(float m)
{
    grasp_z_m_ = m;
    grasp_nvs_save("grasp_z", m);
}

void Manager::set_gripper_grip_torque(float nm)
{
    gripper_grip_torque_ = nm;
    grasp_nvs_save("grip_torque", nm);
}

void Manager::set_gripper_close_pos(float rad)
{
    gripper_close_pos_ = rad;
    grasp_nvs_save("grip_close", rad);
}

void Manager::gripper_cmd(damiao::Motor &motor, float pos)
{
    motor_control_->pos_vel_control(motor, pos, 5.0f);
}

void Manager::reload_vision_matrix()
{
    portENTER_CRITICAL(&vision_matrix_lock_);
    memset(version_matrix_, 0, sizeof(version_matrix_));
    portEXIT_CRITICAL(&vision_matrix_lock_);
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("vision_matrix", NVS_READONLY, &nvs_handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Vision matrix not calibrated yet (namespace missing), using zeros; run panthera_set_vision_matrix");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        // Continue with zero matrix if NVS open fails
    } else {
        // Read matrix values from NVS
        for (int i = 0; i < 9; i++) {
            char key[8];
            snprintf(key, sizeof(key), "m%d", i);
            int32_t value = 0;
            ret = nvs_get_i32(nvs_handle, key, &value);
            if (ret == ESP_OK) {
                // Convert int back to double by dividing by 1000000
                int row = i / 3;
                int col = i % 3;
                portENTER_CRITICAL(&vision_matrix_lock_);
                version_matrix_[row][col] = static_cast<double>(value) / 1000000.0f;
                portEXIT_CRITICAL(&vision_matrix_lock_);
            } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Matrix element %d not found in NVS, using default 0", i);
            } else {
                ESP_LOGE(TAG, "Error reading matrix element %d: %s", i, esp_err_to_name(ret));
            }
        }
        nvs_close(nvs_handle);
    }
}

Manager::Manager(damiao::Motor_Control* motor_control)
    : motor_control_(motor_control), jpeg_decoder_handle_(nullptr), jpeg_decode_buffer_(nullptr), jpeg_decode_buffer_size_(0), jpeg_decoded_size_(0), detector_(nullptr), lcd_refresh_task_handle_(nullptr), detect_task_handle_(nullptr), grasp_task_handle_(nullptr), motor_status_task_handle_(nullptr), zero_g_task_handle_(nullptr), zero_g_enabled_(false), motor_detail_backdrop_(nullptr), motor_detail_title_(nullptr), motor_detail_info_(nullptr), motor_detail_index_(-1), detect_queue_(nullptr), grasp_command_queue_(nullptr), detect_results_mutex_(nullptr), detect_buffer_pool_queue_(nullptr), grasp_state_(GraspState::IDLE), motors_enabled_(false)
{
    static_assert(MAX_SERVO_NUM == UI_MOTOR_STATUS_COUNT, "motor status count mismatch");
    // Initialize version_matrix_ to zero
    memset(version_matrix_, 0, sizeof(version_matrix_));

    // Initialize buffer pool to nullptr
    memset(detect_buffer_pool_, 0, sizeof(detect_buffer_pool_));

    reload_vision_matrix();

    // Gripper close position survives reboots; same int32 x1e6 scheme as the matrix.
    nvs_handle_t grasp_nvs;
    if (nvs_open("grasp", NVS_READONLY, &grasp_nvs) == ESP_OK) {
        int32_t v = 0;
        if (nvs_get_i32(grasp_nvs, "grip_close", &v) == ESP_OK) {
            gripper_close_pos_ = v / 1000000.0f;
        }
        if (nvs_get_i32(grasp_nvs, "grip_torque", &v) == ESP_OK) {
            gripper_grip_torque_ = v / 1000000.0f;
        }
        if (nvs_get_i32(grasp_nvs, "grasp_z", &v) == ESP_OK) {
            grasp_z_m_ = v / 1000000.0f;
        }
        nvs_close(grasp_nvs);
    }

    // The LVGL task is already running, so register callbacks under its lock.
    app_lcd_lock(0);
    lv_obj_add_event_cb(ui_GraspButton, lvgl_button_clicked_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_GoZeroButton, lvgl_button_clicked_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_EnableSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(ui_ZeroGSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);
    for (int i = 0; i < UI_MOTOR_STATUS_COUNT; i++) {
        if (ui_MotorStatusChips[i] != nullptr) {
            lv_obj_add_event_cb(ui_MotorStatusChips[i], motor_chip_clicked_event_handler, LV_EVENT_CLICKED, this);
            lv_obj_set_user_data(ui_MotorStatusChips[i], reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        }
    }
    // Disable Zero-G / action buttons initially since motors are disabled by default
    set_zero_g_ui_enabled(false);
    set_motor_action_buttons_enabled(false);
    app_lcd_unlock();

    // Initialize the JPEG decoder
    memset(&jpeg_decode_cfg_, 0, sizeof(jpeg_decode_cfg_t));
    jpeg_decode_cfg_.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    jpeg_decode_cfg_.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;
    jpeg_decode_cfg_.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
    jpeg_decode_engine_cfg_t decode_eng_cfg = {
        .intr_priority = 0,
        .timeout_ms = 5000,
    };
    ESP_ERROR_CHECK(jpeg_new_decoder_engine(&decode_eng_cfg, &jpeg_decoder_handle_));

    jpeg_decode_memory_alloc_cfg_t jpeg_dec_output_mem_cfg = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };

    jpeg_decode_buffer_ = (uint8_t*)jpeg_alloc_decoder_mem(640 * 480 * 2, &jpeg_dec_output_mem_cfg, &jpeg_decode_buffer_size_);
    if (jpeg_decode_buffer_ == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate JPEG decode buffer");
        return;
    }

    // Only keep one pending frame. A deeper FIFO increases latency without
    // increasing inference throughput.
    detect_queue_ = xQueueCreate(1, sizeof(detect_queue_msg_t));
    if (detect_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create detect queue");
        return;
    }

    detect_buffer_pool_queue_ = xQueueCreate(DETECT_BUFFER_POOL_SIZE, sizeof(uint8_t*));
    if (detect_buffer_pool_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create detect buffer pool queue");
        return;
    }

    size_t buffer_size = 640 * 480 * 2;  // RGB565
    for (int i = 0; i < DETECT_BUFFER_POOL_SIZE; i++) {
        detect_buffer_pool_[i] = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
        if (detect_buffer_pool_[i] == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate detect buffer %d", i);
            for (int j = 0; j < i; j++) {
                heap_caps_free(detect_buffer_pool_[j]);
                detect_buffer_pool_[j] = nullptr;
            }
            return;
        }
        xQueueSend(detect_buffer_pool_queue_, &detect_buffer_pool_[i], 0);
    }
    ESP_LOGI(TAG, "Detect buffer pool initialized with %d buffers", DETECT_BUFFER_POOL_SIZE);

    detect_results_mutex_ = xSemaphoreCreateMutex();
    if (detect_results_mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create detect results mutex");
        return;
    }

    // Keep model loading lazy: USB/UVC transfers require scarce contiguous
    // internal DMA memory and must be allocated before ESP-DL claims memory.
    detector_ = new GreenCubeDetect();
    if (detector_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create GreenCubeDetect");
        return;
    }

    // Camera decode/display uses cycles left by control, USB and AI workers.
    if (xTaskCreatePinnedToCore(lcd_refresh_task, "lcd_refresh_task", 10 * 1024, this, 4, &lcd_refresh_task_handle_, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LCD refresh task");
        return;
    }

    if (xTaskCreatePinnedToCore(detect_task, "detect_task", 40 * 1024, this, 5, &detect_task_handle_, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create detect task");
        return;
    }

    // Create the grasp command queue
    grasp_command_queue_ = xQueueCreate(1, sizeof(Joint));
    if (grasp_command_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Grasp command queue");
        return;
    }

    // Create the grasp task
    // Motion control must preempt AI work; USB/UVC remains the highest-priority
    // application workload on this core.
    if (xTaskCreatePinnedToCore(grasp_task, "grasp_task", 8 * 1024, this, 6, &grasp_task_handle_, 1) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Grasp task");
        return;
    }

    if (xTaskCreatePinnedToCore(motor_status_task, "motor_status", 4 * 1024, this, 6, &motor_status_task_handle_, 1) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motor status task");
        return;
    }

    motor_control_->register_fault_callback(motor_fault_handler, this);
}

void Manager::update_motor_status_ui()
{
    struct {
        bool online;
        damiao::Motor_Fault fault;
    } snapshot[MAX_SERVO_NUM];

    for (int i = 0; i < MAX_SERVO_NUM; i++) {
        snapshot[i].online = false;
        snapshot[i].fault = damiao::Motor_Fault::OFFLINE;
        damiao::Motor* motor = motor_control_->get_motor_by_master_id(k_motor_master_ids_[i]);
        if (motor == nullptr) {
            continue;
        }
        // Keep feedback fresh even when the health monitor is not running.
        motor_control_->refresh_motor_status(*motor);
    }

    // Give the async feedback path a moment to land.
    vTaskDelay(pdMS_TO_TICKS(20));

    for (int i = 0; i < MAX_SERVO_NUM; i++) {
        damiao::Motor* motor = motor_control_->get_motor_by_master_id(k_motor_master_ids_[i]);
        if (motor == nullptr) {
            continue;
        }
        snapshot[i].online = motor_control_->is_motor_online(*motor);
        snapshot[i].fault = motor_control_->get_motor_fault(*motor);
    }

    app_lcd_lock(0);
    for (int i = 0; i < MAX_SERVO_NUM; i++) {
        if (ui_MotorStatusLabels[i] == nullptr || ui_MotorStatusChips[i] == nullptr) {
            continue;
        }

        uint32_t chip_color;
        uint32_t text_color;
        const char* state_text;
        if (!snapshot[i].online || snapshot[i].fault == damiao::Motor_Fault::OFFLINE) {
            chip_color = 0x252B34;
            text_color = 0x8B949E;
            state_text = "OFF";
        } else if (snapshot[i].fault != damiao::Motor_Fault::NONE) {
            chip_color = 0x5A2A2A;
            text_color = 0xF85149;
            state_text = "FLT";
        } else {
            chip_color = 0x1F3D2A;
            text_color = 0x3FB950;
            state_text = "ON";
        }

        lv_obj_set_style_bg_color(ui_MotorStatusChips[i], lv_color_hex(chip_color), 0);
        lv_obj_set_style_text_color(ui_MotorStatusLabels[i], lv_color_hex(text_color), 0);
        lv_label_set_text_fmt(ui_MotorStatusLabels[i], "%s\n%s", k_motor_names_[i], state_text);
    }
    if (motor_detail_index_ >= 0) {
        refresh_motor_detail_info();
    }
    app_lcd_unlock();
}

void Manager::motor_status_task(void* arg)
{
    Manager* self = static_cast<Manager*>(arg);
    if (self == nullptr || self->motor_control_ == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    while (1) {
        self->update_motor_status_ui();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void Manager::motor_chip_clicked_event_handler(lv_event_t* event)
{
    Manager* self = static_cast<Manager*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || target == nullptr) {
        return;
    }
    const int index = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    if (index < 0 || index >= MAX_SERVO_NUM) {
        return;
    }
    self->open_motor_detail(index);
}

void Manager::close_motor_detail()
{
    if (motor_detail_backdrop_ != nullptr) {
        lv_obj_del(motor_detail_backdrop_);
    }
    motor_detail_backdrop_ = nullptr;
    motor_detail_title_ = nullptr;
    motor_detail_info_ = nullptr;
    motor_detail_index_ = -1;
}

void Manager::refresh_motor_detail_info()
{
    if (motor_detail_info_ == nullptr || motor_detail_index_ < 0 || motor_control_ == nullptr) {
        return;
    }

    const int index = motor_detail_index_;
    const uint32_t master_id = k_motor_master_ids_[index];
    damiao::Motor* motor = motor_control_->get_motor_by_master_id(master_id);
    if (motor == nullptr) {
        lv_label_set_text(motor_detail_info_, "Motor not registered");
        return;
    }

    damiao::motor_fb_param_t fb = motor_control_->get_motor_feedback(*motor);
    damiao::Motor_Fault fault = motor_control_->get_motor_fault(*motor);
    const bool online = motor_control_->is_motor_online(*motor);
    const int64_t age_ms = motor_control_->get_ms_since_last_feedback(*motor);
    const char* enabled_text = (fb.state == 1) ? "enabled" : (fb.state == 0) ? "disabled"
                               : "unknown";

    char age_buf[32];
    if (age_ms < 0) {
        snprintf(age_buf, sizeof(age_buf), "never");
    } else {
        snprintf(age_buf, sizeof(age_buf), "%lld ms ago", (long long)age_ms);
    }

    lv_label_set_text_fmt(
        motor_detail_info_,
        "Master 0x%02X   Slave 0x%02X\n"
        "Online: %s\n"
        "Drive: %s (state 0x%X)\n"
        "Fault: %s\n"
        "Pos: %.3f rad (%.1f deg)\n"
        "Vel: %.3f rad/s\n"
        "Torque: %.3f Nm\n"
        "Temp: MOS %.0f C / Rotor %.0f C\n"
        "Feedback: %s",
        (unsigned int)master_id,
        (unsigned int)motor->get_slave_id(),
        online ? "yes" : "no",
        enabled_text,
        fb.state,
        damiao::fault_to_string(fault),
        fb.position,
        fb.position * 180.0f / (float)M_PI,
        fb.velocity,
        fb.torque,
        fb.temper_mos,
        fb.temper_rotor,
        age_buf);
}

void Manager::open_motor_detail(int index)
{
    if (index < 0 || index >= MAX_SERVO_NUM || ui_RobotGraspScreen == nullptr) {
        return;
    }
    if (motor_detail_backdrop_ != nullptr) {
        close_motor_detail();
    }

    motor_detail_index_ = index;

    motor_detail_backdrop_ = lv_obj_create(ui_RobotGraspScreen);
    lv_obj_remove_flag(motor_detail_backdrop_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(motor_detail_backdrop_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(motor_detail_backdrop_, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(motor_detail_backdrop_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(motor_detail_backdrop_, LV_OPA_50, 0);
    lv_obj_set_style_border_width(motor_detail_backdrop_, 0, 0);
    lv_obj_set_style_pad_all(motor_detail_backdrop_, 0, 0);
    lv_obj_add_event_cb(motor_detail_backdrop_, motor_detail_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(motor_detail_backdrop_, reinterpret_cast<void*>(static_cast<intptr_t>(0)));  // close

    lv_obj_t* card = lv_obj_create(motor_detail_backdrop_);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(card, 420, 360);
    lv_obj_center(card);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1C2129), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x343B46), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 10, 0);
    // Keep clicks on the card from closing via backdrop.
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, motor_detail_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(card, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));  // ignore

    motor_detail_title_ = lv_label_create(card);
    lv_obj_set_width(motor_detail_title_, LV_PCT(100));
    lv_obj_set_style_text_color(motor_detail_title_, lv_color_hex(0xE8ECF1), 0);
    lv_obj_set_style_text_font(motor_detail_title_, &lv_font_montserrat_20, 0);
    lv_label_set_text_fmt(motor_detail_title_, "%s  detail", k_motor_names_[index]);

    motor_detail_info_ = lv_label_create(card);
    lv_obj_set_width(motor_detail_info_, LV_PCT(100));
    lv_obj_set_style_text_color(motor_detail_info_, lv_color_hex(0xE8ECF1), 0);
    lv_obj_set_style_text_font(motor_detail_info_, &lv_font_montserrat_14, 0);
    lv_label_set_text(motor_detail_info_, "Loading...");

    lv_obj_t* btn_row = lv_obj_create(card);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, LV_PCT(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_style_pad_column(btn_row, 8, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    auto make_btn = [&](const char* text, intptr_t action, uint32_t color) {
        lv_obj_t* btn = lv_button_create(btn_row);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, 42);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_event_cb(btn, motor_detail_event_handler, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, reinterpret_cast<void*>(action));
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_center(label);
        return btn;
    };

    make_btn("Enable", 1, 0x1F3D2A);
    make_btn("Disable", 2, 0x5A2A2A);
    make_btn("Clear", 3, 0x3D9CF0);

    lv_obj_t* close_btn = lv_button_create(card);
    lv_obj_set_width(close_btn, LV_PCT(100));
    lv_obj_set_height(close_btn, 42);
    lv_obj_set_style_radius(close_btn, 8, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x252B34), 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, motor_detail_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(close_btn, reinterpret_cast<void*>(static_cast<intptr_t>(0)));
    lv_obj_t* close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, "Close");
    lv_obj_set_style_text_font(close_label, &lv_font_montserrat_16, 0);
    lv_obj_center(close_label);

    // Refresh once after creating UI; status task will keep it updated.
    if (motor_control_ != nullptr) {
        damiao::Motor* motor = motor_control_->get_motor_by_master_id(k_motor_master_ids_[index]);
        if (motor != nullptr) {
            motor_control_->refresh_motor_status(*motor);
        }
    }
    refresh_motor_detail_info();
}

void Manager::motor_detail_event_handler(lv_event_t* event)
{
    Manager* self = static_cast<Manager*>(lv_event_get_user_data(event));
    lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    if (self == nullptr || target == nullptr) {
        return;
    }

    const intptr_t action = reinterpret_cast<intptr_t>(lv_obj_get_user_data(target));
    if (action < 0) {
        // Card body: swallow click so backdrop does not close.
        return;
    }
    if (action == 0) {
        self->close_motor_detail();
        return;
    }

    if (self->motor_detail_index_ < 0 || self->motor_control_ == nullptr) {
        return;
    }
    damiao::Motor* motor =
        self->motor_control_->get_motor_by_master_id(self->k_motor_master_ids_[self->motor_detail_index_]);
    if (motor == nullptr) {
        ESP_LOGW(TAG, "Motor detail action ignored: motor not found");
        return;
    }

    if (action == 1) {
        if (self->zero_g_enabled_) {
            ESP_LOGW(TAG, "Per-motor enable ignored while Zero-G is active");
            return;
        }
        ESP_LOGI(TAG, "UI enable motor 0x%02X", (unsigned int)motor->get_master_id());
        if (!self->motor_control_->switch_control_mode(*motor, damiao::POS_VEL_MODE)) {
            ESP_LOGW(TAG, "Failed to select POS_VEL mode for 0x%02X", (unsigned int)motor->get_master_id());
        }
        if (self->motor_control_->enable_motor(*motor) != ESP_OK) {
            ESP_LOGW(TAG, "Enable failed for motor 0x%02X", (unsigned int)motor->get_master_id());
        }
    } else if (action == 2) {
        ESP_LOGI(TAG, "UI disable motor 0x%02X", (unsigned int)motor->get_master_id());
        if (self->motor_control_->disable_motor(*motor) != ESP_OK) {
            ESP_LOGW(TAG, "Disable failed for motor 0x%02X", (unsigned int)motor->get_master_id());
        }
    } else if (action == 3) {
        ESP_LOGI(TAG, "UI clear error motor 0x%02X", (unsigned int)motor->get_master_id());
        if (self->motor_control_->clear_error(*motor) != ESP_OK) {
            ESP_LOGW(TAG, "Clear error failed for motor 0x%02X", (unsigned int)motor->get_master_id());
        }
    }

    self->motor_control_->refresh_motor_status(*motor);
    self->refresh_motor_detail_info();
}

void Manager::set_motor_action_buttons_enabled(bool enabled)
{
    lv_obj_t* buttons[] = {ui_GraspButton, ui_GoZeroButton};
    for (lv_obj_t * btn : buttons) {
        if (btn == nullptr) {
            continue;
        }
        if (enabled) {
            lv_obj_clear_state(btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
    }
}

void Manager::set_zero_g_ui_enabled(bool enabled)
{
    if (ui_ZeroGSwitch == nullptr) {
        return;
    }
    if (enabled) {
        lv_obj_clear_state(ui_ZeroGSwitch, LV_STATE_DISABLED);
        if (ui_ZeroGLabel != nullptr) {
            lv_obj_set_style_text_color(ui_ZeroGLabel, lv_color_hex(0xE6EDF3), 0);
        }
    } else {
        lv_obj_add_state(ui_ZeroGSwitch, LV_STATE_DISABLED);
        if (ui_ZeroGLabel != nullptr) {
            lv_obj_set_style_text_color(ui_ZeroGLabel, lv_color_hex(0x8B949E), 0);
        }
    }
}

bool Manager::switch_arm_control_mode(damiao::Control_Mode mode)
{
    if (motor_control_ == nullptr) {
        return false;
    }
    bool ok = true;
    for (int i = 0; i < k_zero_g_joint_count_; i++) {
        damiao::Motor* motor = motor_control_->get_motor_by_master_id(k_motor_master_ids_[i]);
        if (motor == nullptr) {
            continue;  // Unregistered joint (partial-arm bring-up only).
        }
        if (!motor_control_->switch_control_mode(*motor, mode)) {
            ESP_LOGE(TAG, "Failed to switch motor 0x%02X to mode %d", (unsigned int)k_motor_master_ids_[i],
                     static_cast<int>(mode));
            ok = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return ok;
}

bool Manager::enter_zero_g()
{
    if (motor_control_ == nullptr || !motors_enabled_) {
        return false;
    }
    if (zero_g_enabled_) {
        return true;
    }
    if (grasp_state_ != GraspState::IDLE) {
        ESP_LOGW(TAG, "Zero-G rejected: grasp task busy");
        return false;
    }

    // Mutually exclusive with position actions.
    set_motor_action_buttons_enabled(false);

    if (!switch_arm_control_mode(damiao::MIT_MODE)) {
        switch_arm_control_mode(damiao::POS_VEL_MODE);
        set_motor_action_buttons_enabled(true);
        return false;
    }

    zero_g_enabled_ = true;
    if (zero_g_task_handle_ == nullptr) {
        BaseType_t created = xTaskCreatePinnedToCore(zero_g_task, "zero_g", 4096, this, 7, &zero_g_task_handle_, 1);
        if (created != pdPASS) {
            ESP_LOGE(TAG, "Failed to create zero-g task");
            zero_g_enabled_ = false;
            zero_g_task_handle_ = nullptr;
            switch_arm_control_mode(damiao::POS_VEL_MODE);
            set_motor_action_buttons_enabled(true);
            return false;
        }
    }

    if (ui_StatusLabel != nullptr) {
        lv_label_set_text(ui_StatusLabel, "Status: Zero Gravity");
        lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0x58A6FF), 0);
    }
    ESP_LOGI(TAG, "Zero gravity mode ON (MIT + G(q))");
    return true;
}

void Manager::exit_zero_g()
{
    const bool was_on = zero_g_enabled_.exchange(false);
    if (!was_on && zero_g_task_handle_ == nullptr) {
        return;
    }

    // Prefer letting zero_g_task restore POS_VEL itself; only force-delete as fallback.
    for (int i = 0; i < 40 && zero_g_task_handle_ != nullptr; i++) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    if (zero_g_task_handle_ != nullptr) {
        vTaskDelete(zero_g_task_handle_);
        zero_g_task_handle_ = nullptr;
        if (motor_control_ != nullptr) {
            switch_arm_control_mode(damiao::POS_VEL_MODE);
        }
    }

    ESP_LOGI(TAG, "Zero gravity mode OFF");
}

void Manager::zero_g_task(void* arg)
{
    Manager* self = static_cast<Manager*>(arg);
    if (self == nullptr || self->motor_control_ == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    constexpr TickType_t k_period = pdMS_TO_TICKS(5);  // ~200 Hz (requires CONFIG_FREERTOS_HZ=1000)
    TickType_t last_wake = xTaskGetTickCount();
    float q[6] = {};
    float tau_g[6] = {};

    while (self->zero_g_enabled_ && self->motors_enabled_) {
        // Read arm joints J1..J6; an unregistered joint keeps q=0. Gripper stays in POS_VEL.
        for (int i = 0; i < k_zero_g_joint_count_; i++) {
            damiao::Motor* motor = self->motor_control_->get_motor_by_master_id(self->k_motor_master_ids_[i]);
            if (motor == nullptr) {
                q[i] = 0.0f;
                continue;
            }
            self->motor_control_->refresh_motor_status(*motor);
        }
        vTaskDelay(pdMS_TO_TICKS(1));

        for (int i = 0; i < k_zero_g_joint_count_; i++) {
            damiao::Motor* motor = self->motor_control_->get_motor_by_master_id(self->k_motor_master_ids_[i]);
            if (motor == nullptr) {
                continue;
            }
            // Bus-frame angles (same convention as ROS ZeroGraMode; no J3 flip here).
            const damiao::motor_fb_param_t fb = self->motor_control_->get_motor_feedback(*motor);
            q[i] = fb.position;
        }

        compute_gravity_compensation(q, tau_g);

        // kp=0, small kd: pure gravity + light damping so imperfect G(q) does not free-fall.
        constexpr float k_zero_g_kd = 0.5f;
        for (int i = 0; i < k_zero_g_joint_count_; i++) {
            damiao::Motor* motor = self->motor_control_->get_motor_by_master_id(self->k_motor_master_ids_[i]);
            if (motor == nullptr) {
                continue;
            }
            self->motor_control_->mit_control(*motor, 0.0f, 0.0f, 0.0f, k_zero_g_kd, tau_g[i]);
        }

        vTaskDelayUntil(&last_wake, k_period);
    }

    // Leave MIT before the task exits so the LVGL/Enable path need not block on mode I/O.
    self->switch_arm_control_mode(damiao::POS_VEL_MODE);
    self->zero_g_task_handle_ = nullptr;
    vTaskDelete(nullptr);
}

void Manager::set_switch_checked(lv_obj_t* sw, bool checked)
{
    if (sw == nullptr) {
        return;
    }
    app_lcd_lock(0);
    // Detached first, so a programmatic change cannot re-enter the handler that
    // asked for it.
    lv_obj_remove_event_cb(sw, lvgl_switch_event_handler);
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);
    app_lcd_unlock();
}

esp_err_t Manager::set_zero_g_enabled(bool on)
{
    if (motor_control_ == nullptr) {
        ESP_LOGE(TAG, "Motor control is nullptr");
        return ESP_ERR_INVALID_STATE;
    }

    if (on && !motors_enabled_) {
        ESP_LOGW(TAG, "Zero-G ignored: motors are not enabled");
        set_switch_checked(ui_ZeroGSwitch, false);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Zero-G request: %s", on ? "ON" : "OFF");

    if (on) {
        // enter_zero_g touches widgets directly, so it must run under the lock
        // now that a console task can reach it.
        app_lcd_lock(0);
        const bool entered = enter_zero_g();
        app_lcd_unlock();

        if (!entered) {
            set_switch_checked(ui_ZeroGSwitch, false);
            app_lcd_lock(0);
            if (ui_StatusLabel != nullptr) {
                lv_label_set_text(ui_StatusLabel, "Status: Zero-G failed");
                lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0xF85149), 0);
            }
            app_lcd_unlock();
            return ESP_FAIL;
        }
        set_switch_checked(ui_ZeroGSwitch, true);
        return ESP_OK;
    }

    exit_zero_g();
    set_switch_checked(ui_ZeroGSwitch, false);
    app_lcd_lock(0);
    set_motor_action_buttons_enabled(true);
    if (ui_StatusLabel != nullptr) {
        lv_label_set_text(ui_StatusLabel, "Status: Enabled");
        lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0x3FB950), 0);
    }
    app_lcd_unlock();
    return ESP_OK;
}

esp_err_t Manager::trigger_grasp()
{
    if (!motors_enabled_ || zero_g_enabled_) {
        ESP_LOGW(TAG, "Grasp disabled: motors off or zero-g on");
        return ESP_ERR_INVALID_STATE;
    }

    // Check if grasp task is in IDLE state
    if (grasp_state_ != GraspState::IDLE) {
        ESP_LOGW(TAG, "Grasp disabled: grasp task is busy (state: %d)", static_cast<int>(grasp_state_.load()));
        return ESP_ERR_INVALID_STATE;
    }

    if (motor_control_ == nullptr || detect_results_mutex_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    // Only protect the read and copy operation with mutex
    bool has_valid_target = false;
    if (xSemaphoreTake(detect_results_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Immediately copy the first detection result to selected_target_
        // This prevents it from being cleared by subsequent updates
        if (!detect_results_.empty()) {
            selected_target_ = detect_results_.front();
            has_valid_target = true;
        }
        xSemaphoreGive(detect_results_mutex_);
    }

    // Perform calculations outside of mutex
    if (!has_valid_target) {
        ESP_LOGW(TAG, "No detection results available");
        return ESP_ERR_NOT_FOUND;
    }

    // Check if selected_target_ has valid box coordinates
    int x1 = selected_target_.box[0];
    int y1 = selected_target_.box[1];
    int x2 = selected_target_.box[2];
    int y2 = selected_target_.box[3];
    ESP_LOGI(TAG, "Selected target: box=(%d, %d, %d, %d)", x1, y1, x2, y2);

    if (!(x2 > x1 && y2 > y1 && x1 >= 0 && y1 >= 0)) {
        ESP_LOGW(TAG, "Invalid target box coordinates");
        return ESP_ERR_INVALID_ARG;
    }

    // Valid target detected
    int center_x = (x1 + x2) / 2;
    int center_y = (y1 + y2) / 2;

    ESP_LOGI(TAG, "Selected target: center=(%d, %d)", center_x, center_y);

    double a[3] = {static_cast<double>(center_x), static_cast<double>(center_y), 1.0};
    double b[3] = {0};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            b[i] += version_matrix_[i][j] * a[j];
        }
    }

    double world_x = b[0];
    double world_y = b[1];
    double world_z = b[2];
    ESP_LOGI(TAG, "Selected target: world=(%.5f, %.5f, %.5f)", world_x, world_y, world_z);

    // Calculate target joint angles
    Joint seed = Manager::GRASP_IK_SEED;
    Kinematic kinematic;
    TransformMatrix target = kinematic.forward(seed);
    target(0, 3) = world_x;
    target(1, 3) = world_y;
    target(2, 3) = grasp_z_m_;

    IKResult ik = kinematic.solve_ik(target, seed);
    if (!ik) {
        ESP_LOGW(TAG, "Invalid target position, IK failed: %s (pos_err=%.4f m, rot_err=%.4f rad)",
                 ik_status_to_string(ik.status), ik.pos_error, ik.rot_error);
        return ESP_FAIL;
    }

    // Send grasp command to queue
    if (grasp_command_queue_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(grasp_command_queue_, &ik.joints, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Grasp command queue is full, previous command still executing");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Grasp command sent to queue");
    return ESP_OK;
}

esp_err_t Manager::go_zero()
{
    if (motor_control_ == nullptr) {
        ESP_LOGE(TAG, "Motor control is nullptr");
        return ESP_ERR_INVALID_STATE;
    }
    if (!motors_enabled_ || zero_g_enabled_) {
        ESP_LOGW(TAG, "Go zero ignored: motors not enabled or zero-g active");
        return ESP_ERR_INVALID_STATE;
    }

    const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = motor_control_->get_motor_map();
    for (const auto &pair : motor_map) {
        damiao::Motor* motor = pair.second;
        if (motor != nullptr) {
            motor_control_->pos_vel_control(*motor, 0.0f, 5.0f);
        }
    }
    return ESP_OK;
}

esp_err_t Manager::set_motors_enabled(bool on)
{
    if (motor_control_ == nullptr) {
        ESP_LOGE(TAG, "Motor control is nullptr");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Enable request: %s", on ? "ON" : "OFF");

    // Motor and task work first, outside the LVGL lock: enable_all_motors and
    // exit_zero_g can both block for hundreds of milliseconds, and holding the
    // lock across them stalls the display.
    esp_err_t ret = ESP_OK;
    if (on) {
        ret = motor_control_->enable_all_motors();
        if (ret == ESP_OK) {
            // enable_all_motors() only confirms CAN transmit, not motor presence.
            // Check that at least one motor actually sent feedback back.
            bool any_responded = false;
            for (auto& [id, m] : motor_control_->get_motor_map()) {
                int64_t ms = motor_control_->get_ms_since_last_feedback(*m);
                if (ms >= 0 && ms < 1500) {
                    any_responded = true;
                    break;
                }
            }
            if (!any_responded) {
                ESP_LOGW(TAG, "No motor responded after enable:  motors may be powered off");
                ret = ESP_ERR_TIMEOUT;
            }
        }
        if (ret == ESP_OK) {
            motors_enabled_ = true;
            // 1.5 s of silence before OFFLINE: single motors drop feedback for ~1 s at a
            // time on this bus and the abort below disables the whole arm.
            motor_control_->start_health_monitor(100, HEALTH_TIMEOUT_MS);
        } else {
            ESP_LOGW(TAG, "Enable motors failed, cancel enable");
            motors_enabled_ = false;
        }
    } else {
        motors_enabled_ = false;
        exit_zero_g();
        motor_control_->stop_health_monitor();
        motor_control_->disable_all_motors();
        // Reset grasp state to IDLE when motors are disabled
        grasp_state_ = GraspState::IDLE;
        ESP_LOGI(TAG, "Grasp state reset to IDLE due to motors disabled");
    }

    const bool active = (on && ret == ESP_OK);

    app_lcd_lock(0);

    // Move the switch to match, whichever way the request arrived. A no-op when
    // the user just moved it themselves; the point of the method when the
    // request came from the console. The callback is detached first so a
    // programmatic change cannot re-enter the handler.
    lv_obj_remove_event_cb(ui_EnableSwitch, lvgl_switch_event_handler);
    if (active) {
        lv_obj_add_state(ui_EnableSwitch, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_EnableSwitch, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui_EnableSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);

    if (active) {
        // Enable Zero-G / action buttons when motors are enabled
        set_zero_g_ui_enabled(true);
        set_motor_action_buttons_enabled(true);
        if (ui_StatusLabel != nullptr) {
            lv_label_set_text(ui_StatusLabel, "Status: Enabled");
            lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0x3FB950), 0);
        }
    } else {
        // Disable Zero-G / action buttons when motors are not enabled
        set_zero_g_ui_enabled(false);
        set_motor_action_buttons_enabled(false);
        if (ui_ZeroGSwitch != nullptr && lv_obj_has_state(ui_ZeroGSwitch, LV_STATE_CHECKED)) {
            lv_obj_remove_event_cb(ui_ZeroGSwitch, lvgl_switch_event_handler);
            lv_obj_clear_state(ui_ZeroGSwitch, LV_STATE_CHECKED);
            lv_obj_add_event_cb(ui_ZeroGSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);
        }
        if (ui_StatusLabel != nullptr) {
            if (ret == ESP_ERR_TIMEOUT) {
                lv_label_set_text(ui_StatusLabel, "No response: check motor power");
                lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0xF85149), 0);
            } else if (ret != ESP_OK) {
                lv_label_set_text(ui_StatusLabel, "Status: Enable failed");
                lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0xF85149), 0);
            } else {
                lv_label_set_text(ui_StatusLabel, "Status: Standby");
                lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0x3FB950), 0);
            }
        }
    }

    app_lcd_unlock();
    return ret;
}

void Manager::abort_motion_and_reset_ui()
{
    // Stop issuing new commands and reset UI, but keep motors enabled so the arm
    // retains torque. Hard disable can drop a held load and is unsafe here.
    motors_enabled_ = false;
    exit_zero_g();
    motor_control_->stop_health_monitor();
    grasp_state_ = GraspState::IDLE;
    app_lcd_lock(0);
    lv_obj_remove_event_cb(ui_EnableSwitch, lvgl_switch_event_handler);
    lv_obj_clear_state(ui_EnableSwitch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(ui_EnableSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);
    if (ui_ZeroGSwitch != nullptr && lv_obj_has_state(ui_ZeroGSwitch, LV_STATE_CHECKED)) {
        lv_obj_remove_event_cb(ui_ZeroGSwitch, lvgl_switch_event_handler);
        lv_obj_clear_state(ui_ZeroGSwitch, LV_STATE_CHECKED);
        lv_obj_add_event_cb(ui_ZeroGSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);
    }
    set_zero_g_ui_enabled(false);
    set_motor_action_buttons_enabled(false);
    if (ui_StatusLabel != nullptr) {
        lv_label_set_text(ui_StatusLabel, "Status: Fault / Aborted");
        lv_obj_set_style_text_color(ui_StatusLabel, lv_color_hex(0xF85149), 0);
    }
    app_lcd_unlock();
}

void Manager::motor_fault_handler(uint32_t master_id, damiao::Motor_Fault fault, void* user_ctx)
{
    Manager* self = static_cast<Manager*>(user_ctx);
    if (self == nullptr) {
        return;
    }

    if (fault == damiao::Motor_Fault::NONE) {
        ESP_LOGI(TAG, "Motor 0x%08X recovered; re-enable motors via the switch to resume", (unsigned int)master_id);
        return;
    }

    if (!self->motors_enabled_ || self->grasp_task_handle_ == nullptr) {
        return;
    }

    xTaskNotify(self->grasp_task_handle_, master_id, eSetValueWithOverwrite);
}

void Manager::lcd_refresh_task(void* arg)
{
    Manager* self = (Manager*)arg;
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }

    // create lvgl image descriptor
    lv_image_dsc_t uvc_lv_img_dsc = {};
    uvc_lv_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    uvc_lv_img_dsc.header.w = 640;
    uvc_lv_img_dsc.header.h = 480;
    uvc_lv_img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    uvc_lv_img_dsc.header.flags = 0;
    uvc_lv_img_dsc.data_size = 640 * 480 * 2;
    uvc_lv_img_dsc.data = NULL;

    // Create overlay container for drawing detection boxes
    app_lcd_lock(0);
    lv_obj_t* overlay_container = lv_obj_create(ui_Container2);
    lv_obj_remove_style_all(overlay_container);
    lv_obj_set_size(overlay_container, 640, 480);
    lv_obj_set_pos(overlay_container, 0, 0);
    lv_obj_set_align(overlay_container, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(overlay_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(overlay_container, LV_OPA_TRANSP, 0);
    lv_obj_move_to_index(overlay_container, -1);  // Move to top (above ui_uvcframe)
    app_lcd_unlock();

    // Store rectangle objects for reuse (max 10 detection boxes)
    const int max_boxes = 10;
    lv_obj_t* box_objects[max_boxes] = {nullptr};
    lv_obj_t* label_objects[max_boxes] = {nullptr};
    int box_count = 0;

    // Restart the USB camera stream
    uint32_t failed_get_frame_count = 0;

    while (1) {
        // Wait quietly until a camera starts streaming.
        if (!usb_camera_is_streaming()) {
            failed_get_frame_count = 0;
            vTaskDelay(200 / portTICK_PERIOD_MS);
            continue;
        }

        uvc_host_frame_t* frame = usb_camera_get_frame();
        if (frame == NULL) {
            failed_get_frame_count++;
            if (failed_get_frame_count > 5) {
                ESP_LOGE(TAG, "Failed to get frame for 5 times, restarting USB camera stream");
                usb_camera_restart_stream();
                failed_get_frame_count = 0;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
            continue;
        }

        // Decode the JPEG
        esp_err_t ret = jpeg_decoder_process(self->jpeg_decoder_handle_, &self->jpeg_decode_cfg_, frame->data, frame->data_len, self->jpeg_decode_buffer_, self->jpeg_decode_buffer_size_, &self->jpeg_decoded_size_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "JPEG decoder process failed: %s, skipping frame", esp_err_to_name(ret));
            esp_err_t ret_return = usb_camera_return_frame(frame);
            if (ret_return != ESP_OK) {
                ESP_LOGE(TAG, "Failed to return frame: %s", esp_err_to_name(ret_return));
            }
            continue;
        }

        // Return the frame to the USB camera
        esp_err_t ret_return = usb_camera_return_frame(frame);
        if (ret_return != ESP_OK) {
            ESP_LOGE(TAG, "Failed to return frame: %s", esp_err_to_name(ret_return));
        }

        failed_get_frame_count = 0;

        // Refresh the LCD with the decoded image
        app_lcd_lock(0);
        uvc_lv_img_dsc.data = self->jpeg_decode_buffer_;
        lv_image_set_src(ui_uvcframe, &uvc_lv_img_dsc);

        // Draw detection boxes (reuse LVGL objects — create/delete every frame is too slow)
        std::list<dl::detect::result_t> current_results{};
        if (self->detect_results_mutex_ != nullptr) {
            if (xSemaphoreTake(self->detect_results_mutex_, pdMS_TO_TICKS(2)) == pdTRUE) {
                current_results.assign(self->detect_results_.begin(), self->detect_results_.end());
                xSemaphoreGive(self->detect_results_mutex_);
            }
        }

        const int border_width = 3;
        const lv_color_t box_color = lv_color_hex(0xFF0000);
        int new_count = 0;
        for (const auto &result : current_results) {
            if (new_count >= max_boxes) {
                break;
            }

            const int x1 = result.box[0];
            const int y1 = result.box[1];
            const int x2 = result.box[2];
            const int y2 = result.box[3];
            const int width = x2 - x1;
            const int height = y2 - y1;
            const int center_x = (x1 + x2) / 2;
            const int center_y = (y1 + y2) / 2;

            if (box_objects[new_count] == nullptr) {
                lv_obj_t* box = lv_obj_create(overlay_container);
                lv_obj_remove_style_all(box);
                lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_color(box, box_color, 0);
                lv_obj_set_style_border_width(box, border_width, 0);
                lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
                box_objects[new_count] = box;
            }
            lv_obj_set_size(box_objects[new_count], width, height);
            lv_obj_set_pos(box_objects[new_count], x1, y1);
            lv_obj_clear_flag(box_objects[new_count], LV_OBJ_FLAG_HIDDEN);

            if (label_objects[new_count] == nullptr) {
                lv_obj_t* label = lv_label_create(overlay_container);
                lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
                lv_obj_set_style_bg_color(label, lv_color_hex(0x000000), 0);
                lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
                lv_obj_set_style_pad_all(label, 2, 0);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
                lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
                label_objects[new_count] = label;
            }
            char label_text[32];
            snprintf(label_text, sizeof(label_text), "(%d, %d)", center_x, center_y);
            lv_label_set_text(label_objects[new_count], label_text);
            int label_x = center_x - 30;
            int label_y = y1 - 20;
            if (label_y < 0) {
                label_y = y2 + 5;
            }
            lv_obj_set_pos(label_objects[new_count], label_x, label_y);
            lv_obj_clear_flag(label_objects[new_count], LV_OBJ_FLAG_HIDDEN);

            new_count++;
        }
        for (int i = new_count; i < box_count; i++) {
            if (box_objects[i] != nullptr) {
                lv_obj_add_flag(box_objects[i], LV_OBJ_FLAG_HIDDEN);
            }
            if (label_objects[i] != nullptr) {
                lv_obj_add_flag(label_objects[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
        box_count = new_count;

        app_lcd_unlock();

        // Keep inference busy without building a FIFO of old frames. If both
        // buffers are occupied, reclaim and replace the pending (not currently
        // running) frame with this newer one.
        uint8_t* image_copy = nullptr;
        if (xQueueReceive(self->detect_buffer_pool_queue_, &image_copy, 0) != pdTRUE) {
            detect_queue_msg_t stale_msg = {};
            if (xQueueReceive(self->detect_queue_, &stale_msg, 0) == pdTRUE) {
                image_copy = stale_msg.image_data;
            }
        }

        if (image_copy == nullptr) {
            continue;
        }

        memcpy(image_copy, self->jpeg_decode_buffer_, self->jpeg_decoded_size_);
        detect_queue_msg_t msg = {
            .image_data = image_copy,
            .image_size = self->jpeg_decoded_size_,
        };

        if (xQueueSend(self->detect_queue_, &msg, 0) != pdTRUE) {
            // The consumer can race with buffer return: a free buffer may be
            // available while an older message is still pending. Replace that
            // message, or retry once if the consumer drained it meanwhile.
            detect_queue_msg_t stale_msg = {};
            if (xQueueReceive(self->detect_queue_, &stale_msg, 0) == pdTRUE) {
                if (xQueueSend(self->detect_buffer_pool_queue_, &stale_msg.image_data, 0) != pdTRUE) {
                    ESP_LOGE(TAG, "Detect buffer pool full while replacing frame");
                }
            }

            if (xQueueSend(self->detect_queue_, &msg, 0) != pdTRUE) {
                if (xQueueSend(self->detect_buffer_pool_queue_, &image_copy, 0) != pdTRUE) {
                    ESP_LOGE(TAG, "Detect buffer pool full while dropping frame");
                }
                continue;
            }
        }
    }
}

void Manager::detect_task(void* arg)
{
    Manager* self = (Manager*)arg;
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }

    dl::image::img_t img = {
        .data = NULL,
        .width = 640,
        .height = 480,
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE,
    };
    detect_queue_msg_t msg;
    int64_t last_idle_window_us = esp_timer_get_time();

    while (1) {
        if (xQueueReceive(self->detect_queue_, &msg, portMAX_DELAY) == pdTRUE) {
            img.data = msg.image_data;

            if (self->detector_ != nullptr) {
                auto &results = self->detector_->run(img);

                if (xSemaphoreTake(self->detect_results_mutex_, portMAX_DELAY) == pdTRUE) {
                    self->detect_results_.clear();
                    self->detect_results_.assign(results.begin(), results.end());
                    xSemaphoreGive(self->detect_results_mutex_);
                }
            }

            if (msg.image_data != nullptr) {
                xQueueSend(self->detect_buffer_pool_queue_, &msg.image_data, 0);
            }

            // Let IDLE1 pet the task WDT periodically instead of paying a full
            // 10 ms tick after every inference.
            const int64_t now_us = esp_timer_get_time();
            if (now_us - last_idle_window_us >= 500000) {
                vTaskDelay(1);
                last_idle_window_us = esp_timer_get_time();
            }
        }
    }
}

void Manager::grasp_task(void* arg)
{
    Manager* self = (Manager*)arg;
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }

    Joint target_joint;
    TickType_t state_start_time = 0;
    TickType_t gripper_still_since = 0;

    while (1) {
        uint32_t faulted_motor_id;
        if (xTaskNotifyWait(0, UINT32_MAX, &faulted_motor_id, 0) == pdTRUE) {
            damiao::Motor_Fault fault = self->motor_control_->get_motor_fault(faulted_motor_id);
            ESP_LOGE(TAG, "Grasp task: motor 0x%08X fault (%s), aborting motion (motors left enabled)",
                     (unsigned int)faulted_motor_id, damiao::fault_to_string(fault));
            self->abort_motion_and_reset_ui();
        }

        switch (self->grasp_state_) {
        case GraspState::IDLE: {
            // Wait for grasp command (bounded wait so fault handling above stays responsive)
            if (xQueueReceive(self->grasp_command_queue_, &target_joint, pdMS_TO_TICKS(100)) == pdTRUE) {
                self->target_joint_ = target_joint;
                self->lifted_ = false;
                self->grasp_state_ = GraspState::OPEN_GRIPPER;
                ESP_LOGI(TAG, "Grasp task: Received command, starting grasp sequence");
            }
            break;
        }

        case GraspState::OPEN_GRIPPER: {
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor == nullptr) {
                ESP_LOGE(TAG, "Grasp task: Gripper motor not found");
                self->grasp_state_ = GraspState::IDLE;
                break;
            }
            self->target_gripper_pos_ = 5.0f;  // Open position
            self->gripper_cmd(*gripper_motor, self->target_gripper_pos_);
            self->grasp_state_ = GraspState::WAIT_OPEN;
            state_start_time = xTaskGetTickCount();
            ESP_LOGI(TAG, "Grasp task: Opening gripper");
            break;
        }

        case GraspState::WAIT_OPEN: {
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor == nullptr) {
                ESP_LOGE(TAG, "Grasp task: Gripper motor not found");
                self->grasp_state_ = GraspState::IDLE;
                break;
            }
            // Check timeout
            if (xTaskGetTickCount() - state_start_time > pdMS_TO_TICKS(self->GRIPPER_TIMEOUT_MS)) {
                ESP_LOGE(TAG, "Grasp task: Timeout waiting for gripper to open");
                self->grasp_state_ = GraspState::IDLE;
                break;
            }
            // Refresh motor status and check if reached target
            self->motor_control_->refresh_motor_status(*gripper_motor);
            float pos_error = fabs(self->motor_control_->get_motor_feedback(*gripper_motor).position - self->target_gripper_pos_);
            if (pos_error < self->GRIPPER_TOLERANCE) {
                self->grasp_state_ = GraspState::MOVE_TO_TARGET;
                ESP_LOGI(TAG, "Grasp task: Gripper opened");
            }
            break;
        }

        case GraspState::MOVE_TO_TARGET: {
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            std::vector<uint32_t> motor_ids;
            motor_ids.reserve(motor_map.size());
            for (const auto &pair : motor_map) {
                motor_ids.push_back(pair.first);
            }
            std::sort(motor_ids.begin(), motor_ids.end());

            int joint_count = (motor_ids.size() > 6) ? 6 : motor_ids.size();
            for (int i = 0; i < joint_count; i++) {
                uint32_t master_id = motor_ids[i];
                auto it = motor_map.find(master_id);
                if (it != motor_map.end() && it->second != nullptr) {
                    damiao::Motor* motor = it->second;
                    if (master_id == 0x13) {
                        self->motor_control_->pos_vel_control(*motor, self->target_joint_[i] * -1.0f, 5.0f);
                    } else {
                        self->motor_control_->pos_vel_control(*motor, self->target_joint_[i], 2.0f);
                    }
                }
            }
            self->grasp_state_ = GraspState::WAIT_ARRIVE;
            state_start_time = xTaskGetTickCount();
            ESP_LOGI(TAG, "Grasp task: Moving to target position");
            break;
        }

        case GraspState::WAIT_ARRIVE: {
            if (xTaskGetTickCount() - state_start_time > pdMS_TO_TICKS(self->ARM_MOVE_TIMEOUT_MS)) {
                ESP_LOGE(TAG, "Grasp task: Timeout moving to target position, aborting motion");
                self->abort_motion_and_reset_ui();
                break;
            }
            // Refresh motor status and check if all motors reached target position
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            std::vector<uint32_t> motor_ids;
            motor_ids.reserve(motor_map.size());
            for (const auto &pair : motor_map) {
                motor_ids.push_back(pair.first);
            }
            std::sort(motor_ids.begin(), motor_ids.end());

            int joint_count = (motor_ids.size() > 6) ? 6 : motor_ids.size();
            bool all_reached = true;

            for (int i = 0; i < joint_count; i++) {
                uint32_t master_id = motor_ids[i];
                auto it = motor_map.find(master_id);
                if (it != motor_map.end() && it->second != nullptr) {
                    damiao::Motor* motor = it->second;
                    self->motor_control_->refresh_motor_status(*motor);

                    float target_pos = self->target_joint_[i];
                    if (master_id == 0x13) {
                        target_pos = target_pos * -1.0f;
                    }
                    float pos_error = fabs(self->motor_control_->get_motor_feedback(*motor).position - target_pos);

                    if (pos_error > self->POSITION_TOLERANCE) {
                        all_reached = false;
                        // Continue sending position command
                        if (master_id == 0x13) {
                            self->motor_control_->pos_vel_control(*motor, self->target_joint_[i] * -1.0f, 5.0f);
                        } else {
                            self->motor_control_->pos_vel_control(*motor, self->target_joint_[i], 2.0f);
                        }
                    }
                }
            }

            if (all_reached) {
                self->grasp_state_ = GraspState::CLOSE_GRIPPER;
                ESP_LOGI(TAG, "Grasp task: Reached target position");
            }
            break;
        }

        case GraspState::CLOSE_GRIPPER: {
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor == nullptr) {
                ESP_LOGE(TAG, "Grasp task: Gripper motor not found");
                self->grasp_state_ = GraspState::IDLE;
                break;
            }
            self->target_gripper_pos_ = self->gripper_close_pos_;
            self->gripper_cmd(*gripper_motor, self->target_gripper_pos_);
            self->grasp_state_ = GraspState::WAIT_CLOSE;
            state_start_time = xTaskGetTickCount();
            gripper_still_since = 0;
            ESP_LOGI(TAG, "Grasp task: Closing gripper");
            break;
        }

        case GraspState::WAIT_CLOSE: {
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor == nullptr) {
                ESP_LOGE(TAG, "Grasp task: Gripper motor not found");
                self->grasp_state_ = GraspState::IDLE;
                break;
            }
            // A stalled gripper indicates that it is holding the object.
            if (xTaskGetTickCount() - state_start_time > pdMS_TO_TICKS(self->GRIPPER_TIMEOUT_MS)) {
                ESP_LOGW(TAG, "Grasp task: Gripper close timeout, assuming object is held");
                self->grasp_state_ = GraspState::MOVE_TO_RELEASE_POSITION;
                break;
            }
            // Refresh motor status and check if reached target
            self->motor_control_->refresh_motor_status(*gripper_motor);
            damiao::motor_fb_param_t fb = self->motor_control_->get_motor_feedback(*gripper_motor);
            float pos_error = fabs(fb.position - self->target_gripper_pos_);
            // Contact = torque over the threshold (immediate), or jaws still for a while
            // short of the target (fallback when the threshold is 0 / mis-tuned).
            const bool torque_hit = self->gripper_grip_torque_ > 0.0f && fabs(fb.torque) > self->gripper_grip_torque_;
            if (pos_error < self->GRIPPER_TOLERANCE) {
                self->grasp_state_ = GraspState::MOVE_TO_RELEASE_POSITION;
                ESP_LOGI(TAG, "Grasp task: Gripper closed, moving to release position");
            } else if (torque_hit || fabs(fb.velocity) < 0.05f /* stall fallback: rad/s */) {
                // Jaws are on the cube. Hold just past where they are instead of pushing
                // at the current limit until the motor's protection trips and lets go.
                TickType_t now = xTaskGetTickCount();
                if (gripper_still_since == 0) {
                    gripper_still_since = now;
                }
                if (torque_hit || now - gripper_still_since > pdMS_TO_TICKS(300) /* stall hold-off ms */) {
                    self->target_gripper_pos_ = fb.position - self->GRIPPER_HOLD_SQUEEZE;  // smaller = tighter
                    self->gripper_cmd(*gripper_motor, self->target_gripper_pos_);
                    self->grasp_state_ = GraspState::MOVE_TO_RELEASE_POSITION;
                    ESP_LOGI(TAG, "Grasp task: Gripper %s at %.3f rad (%.3f Nm), holding at %.3f",
                             torque_hit ? "hit torque" : "stopped", fb.position, fb.torque, self->target_gripper_pos_);
                }
            } else {
                gripper_still_since = 0;
                // Continue sending position command
                self->gripper_cmd(*gripper_motor, self->target_gripper_pos_);
            }
            break;
        }

        case GraspState::MOVE_TO_RELEASE_POSITION: {
            // Two legs: first straight up from the grasp pose (carry, don't drag),
            // then across to the release spot.
            Joint seed = Manager::GRASP_IK_SEED;
            Kinematic kinematic;
            TransformMatrix target = kinematic.forward(self->lifted_ ? seed : self->target_joint_);
            if (self->lifted_) {
                target(0, 3) = 0.32f;   // x position
                target(1, 3) = 0.0f;    // y position
                target(2, 3) = 0.045f;  // z position
            } else {
                target(2, 3) += self->LIFT_M;
                seed = self->target_joint_;
            }

            IKResult ik = kinematic.solve_ik(target, seed);
            // A lift that lands within 5 cm is still a lift; only the release spot needs precision.
            const bool lift_close_enough = !self->lifted_ && ik.pos_error < 0.05f;
            if (!ik && !lift_close_enough) {
                ESP_LOGW(TAG, "Grasp task: %s IK failed: %s (pos_err=%.4f m, rot_err=%.4f rad)",
                         self->lifted_ ? "Release position" : "Lift", ik_status_to_string(ik.status), ik.pos_error, ik.rot_error);
                if (!self->lifted_) {
                    self->lifted_ = true;  // skip the lift, carry straight to the release spot
                    break;
                }
                // Never go home holding the cube: open where we are, then home.
                self->grasp_state_ = GraspState::RELEASE_GRIPPER;
                break;
            }

            self->release_joint_ = ik.joints;

            // Move to release position
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            std::vector<uint32_t> motor_ids;
            motor_ids.reserve(motor_map.size());
            for (const auto &pair : motor_map) {
                motor_ids.push_back(pair.first);
            }
            std::sort(motor_ids.begin(), motor_ids.end());

            int joint_count = (motor_ids.size() > 6) ? 6 : motor_ids.size();
            for (int i = 0; i < joint_count; i++) {
                uint32_t master_id = motor_ids[i];
                auto it = motor_map.find(master_id);
                if (it != motor_map.end() && it->second != nullptr) {
                    damiao::Motor* motor = it->second;
                    if (master_id == 0x13) {
                        self->motor_control_->pos_vel_control(*motor, self->release_joint_[i] * -1.0f, 5.0f);
                    } else {
                        self->motor_control_->pos_vel_control(*motor, self->release_joint_[i], 2.0f);
                    }
                }
            }
            self->grasp_state_ = GraspState::WAIT_ARRIVE_RELEASE;
            state_start_time = xTaskGetTickCount();
            ESP_LOGI(TAG, "Grasp task: %s", self->lifted_ ? "Moving to release position (0.32, 0, 0.045)" : "Lifting");
            break;
        }

        case GraspState::WAIT_ARRIVE_RELEASE: {
            if (xTaskGetTickCount() - state_start_time > pdMS_TO_TICKS(self->ARM_MOVE_TIMEOUT_MS)) {
                ESP_LOGE(TAG, "Grasp task: Timeout moving to release position, releasing here");
                self->grasp_state_ = GraspState::RELEASE_GRIPPER;  // never go home holding the cube
                break;
            }
            // Refresh motor status and check if all motors reached release position
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            std::vector<uint32_t> motor_ids;
            motor_ids.reserve(motor_map.size());
            for (const auto &pair : motor_map) {
                motor_ids.push_back(pair.first);
            }
            std::sort(motor_ids.begin(), motor_ids.end());

            int joint_count = (motor_ids.size() > 6) ? 6 : motor_ids.size();
            bool all_reached = true;

            for (int i = 0; i < joint_count; i++) {
                uint32_t master_id = motor_ids[i];
                auto it = motor_map.find(master_id);
                if (it != motor_map.end() && it->second != nullptr) {
                    damiao::Motor* motor = it->second;
                    self->motor_control_->refresh_motor_status(*motor);

                    float target_pos = self->release_joint_[i];
                    if (master_id == 0x13) {
                        target_pos = target_pos * -1.0f;
                    }
                    float pos_error = fabs(self->motor_control_->get_motor_feedback(*motor).position - target_pos);

                    if (pos_error > self->POSITION_TOLERANCE) {
                        all_reached = false;
                        // Continue sending position command
                        if (master_id == 0x13) {
                            self->motor_control_->pos_vel_control(*motor, self->release_joint_[i] * -1.0f, 5.0f);
                        } else {
                            self->motor_control_->pos_vel_control(*motor, self->release_joint_[i], 2.0f);
                        }
                    }
                }
            }

            if (all_reached) {
                if (!self->lifted_) {
                    self->lifted_ = true;
                    self->grasp_state_ = GraspState::MOVE_TO_RELEASE_POSITION;
                    ESP_LOGI(TAG, "Grasp task: Lifted");
                    break;
                }
                // Inside tolerance is not stopped. Never open while still moving.
                vTaskDelay(pdMS_TO_TICKS(self->RELEASE_SETTLE_MS));
                self->grasp_state_ = GraspState::RELEASE_GRIPPER;
                ESP_LOGI(TAG, "Grasp task: Reached release position");
            }
            break;
        }

        case GraspState::RELEASE_GRIPPER: {
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor == nullptr) {
                ESP_LOGE(TAG, "Grasp task: Gripper motor not found");
                self->grasp_state_ = GraspState::IDLE;
                break;
            }
            self->target_gripper_pos_ = 5.0f;  // Open position (release)
            self->gripper_cmd(*gripper_motor, self->target_gripper_pos_);
            self->grasp_state_ = GraspState::WAIT_RELEASE;
            state_start_time = xTaskGetTickCount();
            ESP_LOGI(TAG, "Grasp task: Releasing gripper");
            break;
        }

        case GraspState::WAIT_RELEASE: {
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor == nullptr) {
                ESP_LOGE(TAG, "Grasp task: Gripper motor not found");
                self->grasp_state_ = GraspState::IDLE;
                break;
            }
            if (xTaskGetTickCount() - state_start_time > pdMS_TO_TICKS(self->GRIPPER_TIMEOUT_MS)) {
                ESP_LOGW(TAG, "Grasp task: Gripper release timeout, returning home anyway");
                self->grasp_state_ = GraspState::RETURN_HOME;
                break;
            }
            // Refresh motor status and check if reached target
            self->motor_control_->refresh_motor_status(*gripper_motor);
            float pos_error = fabs(self->motor_control_->get_motor_feedback(*gripper_motor).position - self->target_gripper_pos_);
            if (pos_error < self->GRIPPER_TOLERANCE) {
                self->grasp_state_ = GraspState::RETURN_HOME;
                ESP_LOGI(TAG, "Grasp task: Gripper released");
            } else {
                // Continue sending position command
                self->gripper_cmd(*gripper_motor, self->target_gripper_pos_);
            }
            break;
        }

        // Invariant: home is reached only through RELEASE_GRIPPER, so the cube is
        // already out of the jaws here and the claw goes home CLOSED (0.0).
        // Never add a path into RETURN_HOME that skips the release.
        case GraspState::RETURN_HOME: {
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            std::vector<uint32_t> motor_ids;
            motor_ids.reserve(motor_map.size());
            for (const auto &pair : motor_map) {
                motor_ids.push_back(pair.first);
            }
            std::sort(motor_ids.begin(), motor_ids.end());

            int joint_count = (motor_ids.size() > 6) ? 6 : motor_ids.size();
            for (int i = 0; i < joint_count; i++) {
                uint32_t master_id = motor_ids[i];
                auto it = motor_map.find(master_id);
                if (it != motor_map.end() && it->second != nullptr) {
                    damiao::Motor* motor = it->second;
                    self->motor_control_->pos_vel_control(*motor, 0.0f, 1.0f);
                }
            }
            // Also return gripper to zero position
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor != nullptr) {
                self->target_gripper_pos_ = 0.0f;  // Zero position
                self->gripper_cmd(*gripper_motor, self->target_gripper_pos_);
            }
            self->grasp_state_ = GraspState::WAIT_HOME;
            state_start_time = xTaskGetTickCount();
            ESP_LOGI(TAG, "Grasp task: Returning home (including gripper)");
            break;
        }

        case GraspState::WAIT_HOME: {
            if (xTaskGetTickCount() - state_start_time > pdMS_TO_TICKS(self->ARM_MOVE_TIMEOUT_MS)) {
                ESP_LOGE(TAG, "Grasp task: Timeout returning home, aborting motion");
                self->abort_motion_and_reset_ui();
                break;
            }
            // Refresh motor status and check if all motors reached home position
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            std::vector<uint32_t> motor_ids;
            motor_ids.reserve(motor_map.size());
            for (const auto &pair : motor_map) {
                motor_ids.push_back(pair.first);
            }
            std::sort(motor_ids.begin(), motor_ids.end());

            int joint_count = (motor_ids.size() > 6) ? 6 : motor_ids.size();
            bool all_reached = true;

            for (int i = 0; i < joint_count; i++) {
                uint32_t master_id = motor_ids[i];
                auto it = motor_map.find(master_id);
                if (it != motor_map.end() && it->second != nullptr) {
                    damiao::Motor* motor = it->second;
                    self->motor_control_->refresh_motor_status(*motor);

                    float pos_error = fabs(self->motor_control_->get_motor_feedback(*motor).position - 0.0f);
                    if (pos_error > self->POSITION_TOLERANCE) {
                        all_reached = false;
                        // Continue sending position command
                        self->motor_control_->pos_vel_control(*motor, 0.0f, 1.0f);
                    }
                }
            }

            // Also check if gripper reached zero position
            damiao::Motor* gripper_motor = self->motor_control_->get_motor_by_master_id(0x17);
            if (gripper_motor != nullptr) {
                self->motor_control_->refresh_motor_status(*gripper_motor);
                float gripper_pos_error = fabs(self->motor_control_->get_motor_feedback(*gripper_motor).position - 0.0f);
                if (gripper_pos_error > self->GRIPPER_TOLERANCE) {
                    all_reached = false;
                    // Continue sending position command
                    self->gripper_cmd(*gripper_motor, 0.0f);
                }
            }

            if (all_reached) {
                self->grasp_state_ = GraspState::COMPLETED;
                state_start_time = xTaskGetTickCount();
                ESP_LOGI(TAG, "Grasp task: Returned home (including gripper)");
            }
            break;
        }

        case GraspState::COMPLETED: {
            // Wait a bit then return to idle
            if (xTaskGetTickCount() - state_start_time >= pdMS_TO_TICKS(1000)) {
                self->grasp_state_ = GraspState::IDLE;
                ESP_LOGI(TAG, "Grasp task: Completed, returning to idle");
            }
            break;
        }
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void Manager::lvgl_button_clicked_event_handler(lv_event_t* event)
{
    Manager* self = static_cast<Manager*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(event));

    if (obj == ui_GraspButton) {
        ESP_LOGI(TAG, "Grasp button clicked");
        // Both paths run trigger_grasp so the console command behaves identically.
        self->trigger_grasp();
    } else if (obj == ui_GoZeroButton) {
        ESP_LOGI(TAG, "Go zero button clicked");
        self->go_zero();
    }
}

void Manager::lvgl_switch_event_handler(lv_event_t* event)
{
    Manager* self = static_cast<Manager*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }
    lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_target(event));
    lv_event_code_t code = lv_event_get_code(event);

    if (obj == ui_EnableSwitch) {
        switch (code) {
        case LV_EVENT_VALUE_CHANGED:
            // The whole sequence lives in set_motors_enabled so the console
            // command runs exactly the same path.
            self->set_motors_enabled(lv_obj_has_state(obj, LV_STATE_CHECKED));
            break;
        default:
            break;
        }
    } else if (obj == ui_ZeroGSwitch) {
        if (code == LV_EVENT_VALUE_CHANGED) {
            self->set_zero_g_enabled(lv_obj_has_state(obj, LV_STATE_CHECKED));
        }
    }
}
