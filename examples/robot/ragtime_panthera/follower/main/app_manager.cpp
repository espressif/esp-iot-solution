/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "string.h"
#include "math.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_crc.h"
#include "esp_heap_caps.h"
#include "ui/ui.h"
#include "ui/screens/ui_RobotGraspScreen.h"
#include "lvgl.h"
#include "usb_camera.h"
#include "kinematic.h"
#include "app_manager.h"

#define FRAME_HEADER_SIZE 2
#define MAX_SERVO_NUM 7
#define PACKET_SIZE (FRAME_HEADER_SIZE + MAX_SERVO_NUM * 2 + 2)  // 2 bytes frame header + 7 servos * 2 bytes + 2 bytes CRC

static const char *TAG = "app_manager";
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

Manager::Manager(damiao::Motor_Control* motor_control)
    : motor_control_(motor_control)
    , jpeg_decoder_handle_(nullptr)
    , jpeg_decode_buffer_(nullptr)
    , jpeg_decode_buffer_size_(0)
    , jpeg_decoded_size_(0)
    , color_detect_(nullptr)
    , lcd_refresh_task_handle_(nullptr)
    , grasp_task_handle_(nullptr)
    , color_detect_buffer_pool_queue_(nullptr)
    , grasp_state_(GraspState::IDLE)
    , motors_enabled_(false)
    , esp_now_receiver_registered_(false)
    , esp_now_sync_enabled_(false)
{
    // Initialize version_matrix_ to zero
    memset(version_matrix_, 0, sizeof(version_matrix_));

    // Initialize ESP-NOW servo angles to zero
    memset(esp_now_servo_angles_, 0, sizeof(esp_now_servo_angles_));

    // Initialize buffer pool to nullptr
    memset(color_detect_buffer_pool_, 0, sizeof(color_detect_buffer_pool_));

    // Read the version matrix from the NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open("vision_matrix", NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS partition: %s", esp_err_to_name(ret));
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
                version_matrix_[row][col] = static_cast<double>(value) / 1000000.0f;
            } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
                ESP_LOGW(TAG, "Matrix element %d not found in NVS, using default 0", i);
            } else {
                ESP_LOGE(TAG, "Error reading matrix element %d: %s", i, esp_err_to_name(ret));
            }
        }
        nvs_close(nvs_handle);
    }

    // Initialize the lvgl callback
    lv_obj_add_event_cb(ui_GraspButton, lvgl_button_clicked_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_GoZeroButton, lvgl_button_clicked_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_SetZeroButton, lvgl_button_clicked_event_handler, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(ui_EnableSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(ui_SyncSwitch, lvgl_switch_event_handler, LV_EVENT_VALUE_CHANGED, this);

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

    // Create the color detect queue
    color_detect_queue_ = xQueueCreate(20, sizeof(color_detect_queue_msg_t));
    if (color_detect_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Color detect queue");
        return;
    }

    // Initialize buffer pool for color detection (pre-allocated buffers to avoid malloc/free overhead)
    color_detect_buffer_pool_queue_ = xQueueCreate(COLOR_DETECT_BUFFER_POOL_SIZE, sizeof(uint8_t*));
    if (color_detect_buffer_pool_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create buffer pool queue");
        return;
    }

    // Pre-allocate buffers in SPIRAM
    size_t buffer_size = 640 * 480 * 2;  // RGB565
    for (int i = 0; i < COLOR_DETECT_BUFFER_POOL_SIZE; i++) {
        color_detect_buffer_pool_[i] = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
        if (color_detect_buffer_pool_[i] == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate buffer %d in pool", i);
            // Free already allocated buffers
            for (int j = 0; j < i; j++) {
                heap_caps_free(color_detect_buffer_pool_[j]);
                color_detect_buffer_pool_[j] = nullptr;
            }
            return;
        }
        // Add buffer to pool queue
        xQueueSend(color_detect_buffer_pool_queue_, &color_detect_buffer_pool_[i], 0);
    }
    ESP_LOGI(TAG, "Color detect buffer pool initialized with %d buffers", COLOR_DETECT_BUFFER_POOL_SIZE);

    // Create the detect results mutex
    detect_results_mutex_ = xSemaphoreCreateMutex();
    if (detect_results_mutex_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Detect results mutex");
        return;
    }

    // Initialize the Color detector
    color_detect_ = new ColorDetect(640, 480);
    if (color_detect_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Color detector");
        return;
    }
    color_detect_->register_color({35, 100, 100}, {85, 255, 255}, "green");

    // Create the LCD refresh task
    if (xTaskCreatePinnedToCore(lcd_refresh_task, "lcd_refresh_task", 10 * 1024, this, 6, &lcd_refresh_task_handle_, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LCD refresh task");
        return;
    }

    // Create the color detect task
    if (xTaskCreatePinnedToCore(color_detect_task, "color_detect_task", 10 * 1024, this, 5, &color_detect_task_handle_, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Color detect task");
        return;
    }

    // Create the grasp command queue
    grasp_command_queue_ = xQueueCreate(1, sizeof(Joint));
    if (grasp_command_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create Grasp command queue");
        return;
    }

    // Create the grasp task
    if (xTaskCreatePinnedToCore(grasp_task, "grasp_task", 8 * 1024, this, 4, &grasp_task_handle_, 1) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Grasp task");
        return;
    }

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
    bsp_display_lock(0);
    lv_obj_t* overlay_container = lv_obj_create(ui_Container2);
    lv_obj_remove_style_all(overlay_container);
    lv_obj_set_size(overlay_container, 640, 480);
    lv_obj_set_pos(overlay_container, 0, 0);
    lv_obj_set_align(overlay_container, LV_ALIGN_TOP_LEFT);
    lv_obj_clear_flag(overlay_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(overlay_container, LV_OPA_TRANSP, 0);
    lv_obj_move_to_index(overlay_container, -1);  // Move to top (above ui_uvcframe)
    bsp_display_unlock();

    // Store rectangle objects for reuse (max 10 detection boxes)
    const int max_boxes = 10;
    lv_obj_t* box_objects[max_boxes] = {nullptr};
    lv_obj_t* label_objects[max_boxes] = {nullptr};
    int box_count = 0;

    // Restart the USB camera stream
    uint32_t failed_get_frame_count = 0;

    while (1) {
        uvc_host_frame_t *frame = usb_camera_get_frame();
        if (frame == NULL) {
            // Add delay to avoid blocking CPU and allow other tasks to run
            failed_get_frame_count++;
            if (failed_get_frame_count > 5) {
                ESP_LOGE(TAG, "Failed to get frame for 5 times, restarting USB camera stream");
                usb_camera_restart_stream();
                failed_get_frame_count = 0;
            }
            ESP_LOGW(TAG, "Frame is NULL, skipping");
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
        bsp_display_lock(0);
        uvc_lv_img_dsc.data = self->jpeg_decode_buffer_;
        lv_image_set_src(ui_uvcframe, &uvc_lv_img_dsc);

        // Draw the detection boxes
        std::list<dl::detect::result_t> current_results{};
        if (self->detect_results_mutex_ != nullptr) {
            if (xSemaphoreTake(self->detect_results_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
                current_results.assign(self->detect_results_.begin(), self->detect_results_.end());
                xSemaphoreGive(self->detect_results_mutex_);
            }
        }

        // Clear previous boxes and labels
        for (int i = 0; i < box_count; i++) {
            if (box_objects[i] != nullptr) {
                lv_obj_del(box_objects[i]);
                box_objects[i] = nullptr;
            }
            if (label_objects[i] != nullptr) {
                lv_obj_del(label_objects[i]);
                label_objects[i] = nullptr;
            }
        }
        box_count = 0;

        // Draw new detection boxes
        const int border_width = 3;
        lv_color_t box_color = lv_color_hex(0xFF0000);  // Red color

        for (const auto &result : current_results) {
            if (box_count >= max_boxes) {
                break;
            }

            int x1 = result.box[0];
            int y1 = result.box[1];
            int x2 = result.box[2];
            int y2 = result.box[3];
            int width = x2 - x1;
            int height = y2 - y1;
            int center_x = (x1 + x2) / 2;
            int center_y = (y1 + y2) / 2;

            // Create rectangle object with border
            lv_obj_t* box = lv_obj_create(overlay_container);
            lv_obj_remove_style_all(box);
            lv_obj_set_size(box, width, height);
            lv_obj_set_pos(box, x1, y1);
            lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);  // Transparent background
            lv_obj_set_style_border_color(box, box_color, 0);
            lv_obj_set_style_border_width(box, border_width, 0);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            box_objects[box_count] = box;

            // Create label to display center coordinates above the box
            lv_obj_t* label = lv_label_create(overlay_container);
            char label_text[32];
            snprintf(label_text, sizeof(label_text), "(%d, %d)", center_x, center_y);
            lv_label_set_text(label, label_text);
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);  // White text
            lv_obj_set_style_bg_color(label, lv_color_hex(0x000000), 0);   // Black background
            lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_all(label, 2, 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
            // Position label above the box (centered horizontally)
            int label_x = center_x - 30;  // Approximate center of text (adjust as needed)
            int label_y = y1 - 20;        // Above the box
            if (label_y < 0) {
                label_y = y2 + 5;  // If too high, place below the box
            }
            lv_obj_set_pos(label, label_x, label_y);
            lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
            label_objects[box_count] = label;

            box_count++;
        }

        bsp_display_unlock();

        // Send the decoded image to the color detect task
        // Get a buffer from the pool (non-blocking, skip if no buffer available)
        uint8_t* image_copy = nullptr;
        if (xQueueReceive(self->color_detect_buffer_pool_queue_, &image_copy, 0) == pdTRUE) {
            // Copy data to the buffer from pool
            memcpy(image_copy, self->jpeg_decode_buffer_, self->jpeg_decoded_size_);
            color_detect_queue_msg_t msg = {
                .image_data = image_copy,
                .image_size = self->jpeg_decoded_size_,
            };
            if (xQueueSend(self->color_detect_queue_, &msg, 0) != pdTRUE) {
                // Queue is full, return buffer to pool
                xQueueSend(self->color_detect_buffer_pool_queue_, &image_copy, 0);
            }
        }

        // Add delay to avoid blocking CPU and allow other tasks to run
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void Manager::color_detect_task(void* arg)
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
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
    };
    color_detect_queue_msg_t msg;

    while (1) {
        if (xQueueReceive(self->color_detect_queue_, &msg, portMAX_DELAY) == pdTRUE) {
            // Set image data from queue message
            img.data = msg.image_data;

            // Run color detection
            if (self->color_detect_ != nullptr) {
                auto &results = self->color_detect_->run(img);

                // Save results with mutex protection
                if (xSemaphoreTake(self->detect_results_mutex_, portMAX_DELAY) == pdTRUE) {
                    self->detect_results_.clear();
                    self->detect_results_.assign(results.begin(), results.end());
                    xSemaphoreGive(self->detect_results_mutex_);
                }
            }

            // Return buffer to pool after processing
            if (msg.image_data != nullptr) {
                xQueueSend(self->color_detect_buffer_pool_queue_, &msg.image_data, 0);
            }
        }
        vTaskDelay(30 / portTICK_PERIOD_MS);
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

    while (1) {
        switch (self->grasp_state_) {
        case GraspState::IDLE: {
            // Wait for grasp command
            if (xQueueReceive(self->grasp_command_queue_, &target_joint, portMAX_DELAY) == pdTRUE) {
                self->target_joint_ = target_joint;
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
            self->motor_control_->pos_vel_control(*gripper_motor, self->target_gripper_pos_, 5.0f);
            self->grasp_state_ = GraspState::WAIT_OPEN;
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
            // Refresh motor status and check if reached target
            self->motor_control_->refresh_motor_status(*gripper_motor);
            float pos_error = fabs(gripper_motor->motor_fb_param.position - self->target_gripper_pos_);
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
            ESP_LOGI(TAG, "Grasp task: Moving to target position");
            break;
        }

        case GraspState::WAIT_ARRIVE: {
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
                    float pos_error = fabs(motor->motor_fb_param.position - target_pos);

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
            self->target_gripper_pos_ = 1.25f;  // Close position
            self->motor_control_->pos_vel_control(*gripper_motor, self->target_gripper_pos_, 5.0f);
            self->grasp_state_ = GraspState::WAIT_CLOSE;
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
            // Refresh motor status and check if reached target
            self->motor_control_->refresh_motor_status(*gripper_motor);
            float pos_error = fabs(gripper_motor->motor_fb_param.position - self->target_gripper_pos_);
            if (pos_error < self->GRIPPER_TOLERANCE) {
                self->grasp_state_ = GraspState::MOVE_TO_RELEASE_POSITION;
                ESP_LOGI(TAG, "Grasp task: Gripper closed, moving to release position");
            } else {
                // Continue sending position command
                self->motor_control_->pos_vel_control(*gripper_motor, self->target_gripper_pos_, 5.0f);
            }
            break;
        }

        case GraspState::MOVE_TO_RELEASE_POSITION: {
            // Calculate inverse kinematics for release position (0.32, 0, 0.397)
            Joint j1 = Joint(0.0, DEG2RAD(30.0), DEG2RAD(-36.0), DEG2RAD(65.0), 0.0, 0.0);
            TransformMatrix t1;
            Kinematic kinematic;
            kinematic.solve_forward_kinematics(j1, t1);

            TransformMatrix t2 = t1;
            t2(0, 3) = 0.32f;   // x position
            t2(1, 3) = 0.0f;    // y position
            t2(2, 3) = 0.045f;  // z position
            Joint j2 = j1;
            kinematic.solve_inverse_kinematics(t2, j2);
            TransformMatrix t3;
            kinematic.solve_forward_kinematics(j2, t3);

            float error_x = t3(0, 3) - t2(0, 3);
            float error_y = t3(1, 3) - t2(1, 3);
            float error_z = t3(2, 3) - t2(2, 3);
            if (fabs(error_x) > 0.01 || fabs(error_y) > 0.01 || fabs(error_z) > 0.01) {
                ESP_LOGW(TAG, "Grasp task: Invalid release position, inverse kinematics error too large");
                self->grasp_state_ = GraspState::RETURN_HOME;  // Fallback to return home
                break;
            }

            self->release_joint_ = j2;

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
            ESP_LOGI(TAG, "Grasp task: Moving to release position (0.32, 0, 0.397)");
            break;
        }

        case GraspState::WAIT_ARRIVE_RELEASE: {
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
                    float pos_error = fabs(motor->motor_fb_param.position - target_pos);

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
            self->motor_control_->pos_vel_control(*gripper_motor, self->target_gripper_pos_, 5.0f);
            self->grasp_state_ = GraspState::WAIT_RELEASE;
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
            // Refresh motor status and check if reached target
            self->motor_control_->refresh_motor_status(*gripper_motor);
            float pos_error = fabs(gripper_motor->motor_fb_param.position - self->target_gripper_pos_);
            if (pos_error < self->GRIPPER_TOLERANCE) {
                self->grasp_state_ = GraspState::RETURN_HOME;
                ESP_LOGI(TAG, "Grasp task: Gripper released");
            } else {
                // Continue sending position command
                self->motor_control_->pos_vel_control(*gripper_motor, self->target_gripper_pos_, 5.0f);
            }
            break;
        }

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
                self->motor_control_->pos_vel_control(*gripper_motor, self->target_gripper_pos_, 5.0f);
            }
            self->grasp_state_ = GraspState::WAIT_HOME;
            ESP_LOGI(TAG, "Grasp task: Returning home (including gripper)");
            break;
        }

        case GraspState::WAIT_HOME: {
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

                    float pos_error = fabs(motor->motor_fb_param.position - 0.0f);
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
                float gripper_pos_error = fabs(gripper_motor->motor_fb_param.position - 0.0f);
                if (gripper_pos_error > self->GRIPPER_TOLERANCE) {
                    all_reached = false;
                    // Continue sending position command
                    self->motor_control_->pos_vel_control(*gripper_motor, 0.0f, 5.0f);
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

void Manager::lvgl_button_clicked_event_handler(lv_event_t *event)
{
    Manager* self = static_cast<Manager*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(event));

    if (obj == ui_GraspButton) {
        ESP_LOGI(TAG, "Grasp button clicked");
        // Check if motors are enabled
        if (!self->motors_enabled_ || self->esp_now_sync_enabled_) {
            ESP_LOGW(TAG, "Grasp button disabled: motors are not enabled or ESP-NOW sync is enabled");
            return;
        }

        // Check if grasp task is in IDLE state
        if (self->grasp_state_ != GraspState::IDLE) {
            ESP_LOGW(TAG, "Grasp button disabled: grasp task is busy (state: %d)", static_cast<int>(self->grasp_state_));
            return;
        }

        if (self->motor_control_ != nullptr && self->detect_results_mutex_ != nullptr) {
            // Only protect the read and copy operation with mutex
            bool has_valid_target = false;
            if (xSemaphoreTake(self->detect_results_mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Immediately copy the first detection result to selected_target_
                // This prevents it from being cleared by subsequent updates
                if (!self->detect_results_.empty()) {
                    self->selected_target_ = self->detect_results_.front();
                    has_valid_target = true;
                }
                xSemaphoreGive(self->detect_results_mutex_);
            }

            // Perform calculations outside of mutex
            if (!has_valid_target) {
                ESP_LOGW(TAG, "No detection results available when button clicked");
                return;
            }

            // Check if selected_target_ has valid box coordinates
            int x1 = self->selected_target_.box[0];
            int y1 = self->selected_target_.box[1];
            int x2 = self->selected_target_.box[2];
            int y2 = self->selected_target_.box[3];
            ESP_LOGI(TAG, "Selected target: box=(%d, %d, %d, %d)", x1, y1, x2, y2);

            if (x2 > x1 && y2 > y1 && x1 >= 0 && y1 >= 0) {
                // Valid target detected
                int center_x = (x1 + x2) / 2;
                int center_y = (y1 + y2) / 2;

                ESP_LOGI(TAG, "Selected target: center=(%d, %d)", center_x, center_y);

                double a[3] = { static_cast<double>(center_x), static_cast<double>(center_y), 1.0 };
                double b[3] = {0};
                for (int i = 0; i < 3; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        b[i] += self->version_matrix_[i][j] * a[j];
                    }
                }

                double world_x = b[0];
                double world_y = b[1];
                double world_z = b[2];
                ESP_LOGI(TAG, "Selected target: world=(%.5f, %.5f, %.5f)", world_x, world_y, world_z);

                // Calculate target joint angles
                Joint j1 = Joint(0.0, DEG2RAD(30.0), DEG2RAD(-36.0), DEG2RAD(65.0), 0.0, 0.0);
                TransformMatrix t1;
                Kinematic kinematic;
                kinematic.solve_forward_kinematics(j1, t1);

                TransformMatrix t2 = t1;
                t2(0, 3) = world_x;
                t2(1, 3) = world_y;
                t2(2, 3) = 0.015f;
                Joint j2 = j1;
                kinematic.solve_inverse_kinematics(t2, j2);
                TransformMatrix t3;
                kinematic.solve_forward_kinematics(j2, t3);

                float error_x = t3(0, 3) - t2(0, 3);
                float error_y = t3(1, 3) - t2(1, 3);
                float error_z = t3(2, 3) - t2(2, 3);
                if (fabs(error_x) > 0.01 || fabs(error_y) > 0.01 || fabs(error_z) > 0.01) {
                    ESP_LOGW(TAG, "Invalid target position, inverse kinematics error too large");
                    return;
                }

                // Send grasp command to queue
                if (self->grasp_command_queue_ != nullptr) {
                    if (xQueueSend(self->grasp_command_queue_, &j2, 0) != pdTRUE) {
                        ESP_LOGW(TAG, "Grasp command queue is full, previous command still executing");
                    } else {
                        ESP_LOGI(TAG, "Grasp command sent to queue");
                    }
                }
            } else {
                ESP_LOGW(TAG, "Invalid target box coordinates");
            }
        }
    } else if (obj == ui_GoZeroButton) {
        ESP_LOGI(TAG, "Go zero button clicked");
        if (self->motor_control_ != nullptr) {
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            for (const auto &pair : motor_map) {
                damiao::Motor* motor = pair.second;
                if (motor != nullptr) {
                    self->motor_control_->pos_vel_control(*motor, 0.0f, 5.0f);
                }
            }
        }
    } else if (obj == ui_SetZeroButton) {
        ESP_LOGI(TAG, "Set zero button clicked");
        if (self->motor_control_ != nullptr) {
            const std::unordered_map<uint32_t, damiao::Motor*> &motor_map = self->motor_control_->get_motor_map();
            for (const auto &pair : motor_map) {
                damiao::Motor* motor = pair.second;
                if (motor != nullptr) {
                    self->motor_control_->save_zero_position(*motor);
                }
            }
        }
    }
}

void Manager::lvgl_switch_event_handler(lv_event_t *event)
{
    Manager* self = static_cast<Manager*>(lv_event_get_user_data(event));
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }
    lv_obj_t *obj = static_cast<lv_obj_t *>(lv_event_get_target(event));
    lv_event_code_t code = lv_event_get_code(event);

    if (obj == ui_EnableSwitch) {
        switch (code) {
        case LV_EVENT_VALUE_CHANGED:
            if (self->motor_control_ != nullptr) {
                if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
                    ESP_LOGI(TAG, "Enable switch: ON");
                    self->motors_enabled_ = true;
                    self->motor_control_->enable_all_motors();
                } else {
                    ESP_LOGI(TAG, "Enable switch: OFF");
                    self->motors_enabled_ = false;
                    self->motor_control_->disable_all_motors();
                }
            }
            break;
        default:
            break;
        }
    } else if (obj == ui_SyncSwitch) {
        switch (code) {
        case LV_EVENT_VALUE_CHANGED:
            if (self->motor_control_ != nullptr) {
                if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
                    ESP_LOGI(TAG, "Sync switch: ON");
                    self->esp_now_sync_enabled_ = true;
                } else {
                    ESP_LOGI(TAG, "Sync switch: OFF");
                    self->esp_now_sync_enabled_ = false;
                }
            }
            break;
        default:
            break;
        }
    }
}

esp_err_t Manager::register_esp_now_receiver(uart_port_t uart_port, int32_t baud_rate)
{
    if (esp_now_receiver_registered_) {
        ESP_LOGW(TAG, "ESP-NOW receiver already registered");
        return ESP_OK;
    }

#if CONFIG_ENABLE_UPDATE_C6_FLASH
    uart_flush_input(uart_port);
    uart_flush(uart_port);
    uart_set_baudrate(uart_port, baud_rate);
#else
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(uart_port, 1024 * 2, 0, 0, NULL, 0), TAG, "Failed to install UART driver");
    ESP_RETURN_ON_ERROR(uart_param_config(uart_port, &uart_config), TAG, "Failed to configure UART parameters");
    ESP_RETURN_ON_ERROR(uart_set_pin(uart_port, CONFIG_SERIAL_FLASH_TX_NUM, CONFIG_SERIAL_FLASH_RX_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "Failed to set UART pins");
#endif
    esp_now_uart_port_ = uart_port;

    if (xTaskCreatePinnedToCore(esp_now_receiver_task, "esp_now_receiver_task", 4 * 1024, this, 7, &esp_now_receiver_task_handle_, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ESP-NOW receiver task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void Manager::esp_now_receiver_task(void* arg)
{
    Manager* self = static_cast<Manager*>(arg);
    if (self == nullptr) {
        ESP_LOGE(TAG, "Manager instance is nullptr");
        return;
    }

    while (1) {
        if (!self->esp_now_sync_enabled_) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        memset(self->esp_now_buffer_, 0, sizeof(self->esp_now_buffer_));
        int len = uart_read_bytes(self->esp_now_uart_port_, self->esp_now_buffer_, sizeof(self->esp_now_buffer_), pdMS_TO_TICKS(10));

        if (len >= PACKET_SIZE) {
            // Search for frame header (0xFF 0xFF)
            int frame_start = -1;
            for (int i = 0; i <= len - PACKET_SIZE; i++) {
                if (self->esp_now_buffer_[i] == 0xFF && self->esp_now_buffer_[i + 1] == 0xFF) {
                    frame_start = i;
                    break;
                }
            }

            if (frame_start >= 0) {
                // Found frame header, parse from this position
                uint8_t* packet_start = &self->esp_now_buffer_[frame_start];

                // Verify CRC
                uint16_t received_crc = ((uint16_t)packet_start[PACKET_SIZE - 1] << 8) |
                                        packet_start[PACKET_SIZE - 2];
                uint16_t calculated_crc = esp_crc16_le(UINT16_MAX, packet_start, PACKET_SIZE - 2);

                if (received_crc == calculated_crc) {
                    // Parse and save servo angles
                    bool invert_table[MAX_SERVO_NUM] = {
                        (bool)CONFIG_SERVO0_JOINT0_INVERT,
                        (bool)CONFIG_SERVO1_JOINT1_INVERT,
                        (bool)CONFIG_SERVO2_JOINT2_INVERT,
                        (bool)CONFIG_SERVO3_JOINT3_INVERT,
                        (bool)CONFIG_SERVO4_JOINT4_INVERT,
                        (bool)CONFIG_SERVO5_JOINT5_INVERT,
                        (bool)CONFIG_SERVO6_JOINT6_INVERT
                    };

                    for (int i = 0; i < MAX_SERVO_NUM; i++) {
                        // Read int16_t (little-endian) from packet
                        int16_t angle_int = (int16_t)((packet_start[FRAME_HEADER_SIZE + i * 2 + 1] << 8) |
                                                      packet_start[FRAME_HEADER_SIZE + i * 2]);
                        // Convert from (radians * 100) to radians
                        float angle = (float)angle_int / 100.0f;
                        // Apply inversion if configured in Kconfig
                        if (invert_table[i]) {
                            angle = -angle;
                        }
                        self->esp_now_servo_angles_[i] = angle;
                    }

                    ESP_LOGD(TAG, "ESP-NOW: Received and saved servo angles: %f, %f, %f, %f, %f, %f, %f",
                             self->esp_now_servo_angles_[0], self->esp_now_servo_angles_[1], self->esp_now_servo_angles_[2],
                             self->esp_now_servo_angles_[3], self->esp_now_servo_angles_[4], self->esp_now_servo_angles_[5],
                             self->esp_now_servo_angles_[6]);

                    // Map Gripper angle
                    float gripper_input = self->esp_now_servo_angles_[6];
                    float input_min = (float)CONFIG_LEADER_INPUT_MIN / 100.0f;
                    float input_max = (float)CONFIG_LEADER_INPUT_MAX / 100.0f;
                    float output_min = (float)CONFIG_FOLLOWER_OUTPUT_MIN / 100.0f;
                    float output_max = (float)CONFIG_FOLLOWER_OUTPUT_MAX / 100.0f;
                    float gripper_output = output_min + (gripper_input - input_min) * (output_max - output_min) / (input_max - input_min);
                    if (gripper_output < output_min) {
                        gripper_output = output_min;
                    } else if (gripper_output > output_max) {
                        gripper_output = output_max;
                    }
                    self->esp_now_servo_angles_[6] = gripper_output;

                    // Update motor positions
                    if (self->motor_control_ != nullptr && self->esp_now_sync_enabled_ && self->motors_enabled_) {
                        const std::unordered_map<uint32_t, damiao::Motor*> motor_map = self->motor_control_->get_motor_map();
                        std::vector<uint32_t> motor_ids;
                        motor_ids.reserve(motor_map.size());
                        for (const auto &pair : motor_map) {
                            motor_ids.push_back(pair.first);
                        }
                        std::sort(motor_ids.begin(), motor_ids.end());

                        for (int i = 0; i < motor_ids.size(); i++) {
                            uint32_t master_id = motor_ids[i];
                            auto it = motor_map.find(master_id);
                            if (it != motor_map.end() && it->second != nullptr) {
                                damiao::Motor* motor = it->second;
                                self->motor_control_->pos_vel_control(*motor, self->esp_now_servo_angles_[i], 5.0f);
                            }
                        }
                    }
                }
            }
        }
    }
}
