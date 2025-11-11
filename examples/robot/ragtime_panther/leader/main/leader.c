/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_app_desc.h"
#include "nvs_flash.h"
#include "leader_config.h"
#include "app_espnow.h"
#include "xl330_m077.h"

static const char *TAG = "LEADER";

static void app_espnow_task(void *pvParameter)
{
    xl330_m077_bus_handle_t bus = (xl330_m077_bus_handle_t)pvParameter;

    while (1) {
        float servo_angles[MAX_SERVO_NUM] = {0};
        for (int i = 0; i < MAX_SERVO_NUM; i++) {
            xl330_m077_read_position(bus, i + 1, &servo_angles[i]);
            servo_angles[i] -= CONFIG_XL330_M077_INIT_POSITION * 1.0f;
        }

        app_espnow_send_servo_data(servo_angles, MAX_SERVO_NUM);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

static esp_err_t init_servo_device(xl330_m077_bus_handle_t bus, uint8_t id)
{
    esp_err_t ret;

    // Retry loop: if any operation fails, reboot and retry
    bool init_success = false;
    while (!init_success) {
        // First, disable torque to ensure EEPROM writes can succeed
        // Writing to EEPROM registers fails when torque is enabled
        ret = xl330_m077_control_torque(bus, id, false);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Device %d disable torque failed, attempting reboot...", id);
            ret = xl330_m077_reboot(bus, id);
            if (ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(50));                                                      /*!< Wait after disabling torque */

        ret = xl330_m077_set_operation(bus, id, XL330_M077_OPERATION_POSITION);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Device %d operation failed, attempting reboot...", id);
            ret = xl330_m077_reboot(bus, id);
            if (ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            continue;
        }

        ret = xl330_m077_control_torque(bus, id, true);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Device %d torque control failed, attempting reboot...", id);
            ret = xl330_m077_reboot(bus, id);
            if (ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            continue;
        }

        // Move to position and verify with position reading
        float target_position = CONFIG_XL330_M077_INIT_POSITION * 1.0f;
        bool position_ok = false;
        int position_retry_count = 0;
        const int max_position_retries = 10;

        while (!position_ok && position_retry_count < max_position_retries) {
            ret = xl330_m077_move_to_position(bus, id, target_position);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Device %d move failed, attempting reboot...", id);
                ret = xl330_m077_reboot(bus, id);
                if (ret == ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(300));
                    ret = xl330_m077_set_operation(bus, id, XL330_M077_OPERATION_POSITION);  /*!< After reboot, need to re-initialize: set operation mode and enable torque */
                    if (ret != ESP_OK) {
                        break;
                    }
                    ret = xl330_m077_control_torque(bus, id, true);
                    if (ret != ESP_OK) {
                        break;
                    }
                }
                break;
            }

            // Wait for servo to move
            vTaskDelay(pdMS_TO_TICKS(200));

            // Read current position
            float current_position = 0.0f;
            ret = xl330_m077_read_position(bus, id, &current_position);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Device %d read position failed, attempting reboot...", id);
                ret = xl330_m077_reboot(bus, id);
                if (ret == ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(300));
                    // After reboot, need to re-initialize: set operation mode and enable torque
                    ret = xl330_m077_set_operation(bus, id, XL330_M077_OPERATION_POSITION);
                    if (ret != ESP_OK) {
                        break;
                    }
                    ret = xl330_m077_control_torque(bus, id, true);
                    if (ret != ESP_OK) {
                        break;
                    }
                }
                break;
            }

            // Check position deviation
            float deviation = current_position - target_position;
            if (deviation < 0) {
                deviation = -deviation;
            }

            if (deviation <= CONFIG_XL330_M077_POSITION_TOLERANCE * 1.0f) {
                position_ok = true;
                ESP_LOGI(TAG, "Device %d reached target position: target=%.2f, current=%.2f, deviation=%.2f",
                         id, target_position, current_position, deviation);
            } else {
                position_retry_count++;
                ESP_LOGW(TAG, "Device %d position deviation too large: target=%.2f, current=%.2f, deviation=%.2f, retry %d/%d",
                         id, target_position, current_position, deviation, position_retry_count, max_position_retries);
            }
        }

        if (!position_ok) {
            ESP_LOGW(TAG, "Device %d position not reached after retries, attempting reboot...", id);
            ret = xl330_m077_reboot(bus, id);
            if (ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            continue;
        }

        // Disable torque before completing initialization (except for MAX_SERVO_NUM)
        if (id != MAX_SERVO_NUM) {
            vTaskDelay(pdMS_TO_TICKS(10));
            ret = xl330_m077_control_torque(bus, id, false);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Device %d disable torque failed, attempting reboot...", id);
                ret = xl330_m077_reboot(bus, id);
                if (ret == ESP_OK) {
                    vTaskDelay(pdMS_TO_TICKS(300));
                }
                continue;
            }
        }

        // All operations succeeded
        init_success = true;
        ESP_LOGI(TAG, "Device %d initialized successfully", id);
    }

    return ESP_OK;
}

void app_main(void)
{
    // Print project information
    const esp_app_desc_t* desc = esp_app_get_description();
    ESP_LOGI(TAG, "Project Name: %s, Version: %s, Compile Time: %s-%s, IDF Version: %s", desc->project_name, desc->version, desc->time, desc->date, desc->idf_ver);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize ESP-NOW
    app_espnow_init();

    // Initialize XL330-M077 serial bus
    xl330_m077_bus_config_t bus_config = {
        .uart_num = UART_NUM_1,
        .tx_pin = CONFIG_XL330_M077_TXD,
        .rx_pin = CONFIG_XL330_M077_RXD,
        .baud_rate = CONFIG_XL330_M077_BAUD_RATE
    };

    xl330_m077_bus_handle_t bus = xl330_m077_bus_init(&bus_config);
    if (bus == NULL) {
        ESP_LOGE(TAG, "Failed to initialize bus");
        return;
    }

    // Initialize XL330-M077 motor and set to initial position
    for (int i = 1; i <= MAX_SERVO_NUM; i++) {
        esp_err_t ret = xl330_m077_add_device(bus, i);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add device %d", i);
            continue;
        }

        init_servo_device(bus, i);
    }

    BaseType_t task_ret = xTaskCreate(app_espnow_task, "app_espnow_task", 5 * 1024, bus, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create app_espnow_task");
        return;
    }

    ESP_LOGI(TAG, "Leader initialized successfully");
}
