/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_console.h"
#include "esp_app_desc.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "dm_motor.h"
#include "kinematic.h"
#include "ui.h"
#include "usb_camera.h"
#include "app_lcd.h"
#include "app_manager.h"
#include "app_serial_flasher.h"
#if CONFIG_CONSOLE_CONTROL
#include "app_console.h"
#endif

static const char *TAG = "main";

damiao::Motor_Control* motor_control = nullptr;

extern "C" void app_main(void)
{
    esp_err_t ret = ESP_OK;

    const esp_app_desc_t* desc = esp_app_get_description();
    ESP_LOGI(TAG, "Project Name: %s, Version: %s, Compile Time: %s-%s, IDF Version: %s", desc->project_name, desc->version, desc->time, desc->date, desc->idf_ver);

#if CONFIG_ENABLE_UPDATE_C6_FLASH
    // Initialize the serial flasher
    const loader_esp32_config_t config = {
        .baud_rate = 115200,
        .uart_port = UART_NUM_1,
        .uart_rx_pin = GPIO_NUM_6,
        .uart_tx_pin = GPIO_NUM_5,
        .reset_trigger_pin = GPIO_NUM_54,
        .gpio0_trigger_pin = GPIO_NUM_53,
    };

    ret = app_serial_flasher_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Serial flasher initialization failed: %s", esp_err_to_name(ret));
        return;
    }
#endif

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize the motor control
    motor_control = damiao::Motor_Control::getInstance(GPIO_NUM_24, GPIO_NUM_25);
    ret = motor_control->init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motor control initialization failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Motor control initialized successfully");

    damiao::Motor* motor1 = new damiao::Motor(damiao::DM4340, 0x01, 0x11);
    damiao::Motor* motor2 = new damiao::Motor(damiao::DM4340, 0x02, 0x12);
    damiao::Motor* motor3 = new damiao::Motor(damiao::DM4340, 0x03, 0x13);
    damiao::Motor* motor4 = new damiao::Motor(damiao::DM4340, 0x04, 0x14);
    damiao::Motor* motor5 = new damiao::Motor(damiao::DM4310, 0x05, 0x15);
    damiao::Motor* motor6 = new damiao::Motor(damiao::DM4310, 0x06, 0x16);
    damiao::Motor* motor7 = new damiao::Motor(damiao::DMH3510, 0x07, 0x17);
    motor_control->add_motor(motor1);
    motor_control->add_motor(motor2);
    motor_control->add_motor(motor3);
    motor_control->add_motor(motor4);
    motor_control->add_motor(motor5);
    motor_control->add_motor(motor6);
    motor_control->add_motor(motor7);
    motor_control->disable_all_motors();

    // Initialize the LCD
    app_lcd_init();
    bsp_display_lock(0);
    ui_panthera_grasp_init();
    bsp_display_unlock();

    // Initialize the USB camera
    usb_camera_init(640, 480);

    // Initialize the console
#if CONFIG_CONSOLE_CONTROL
    app_console_init(motor_control);
#endif

    // Initialize the app manager
    Manager* manager = Manager::get_instance(motor_control);
    if (manager == nullptr) {
        ESP_LOGE(TAG, "Failed to get manager instance");
        return;
    }
    if (manager->register_esp_now_receiver(UART_NUM_1, 115200) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register ESP-NOW receiver");
        return;
    }

    while (1) {
        uint16_t free_sram_size_kb = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
        uint16_t total_sram_size_kb = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024;
        uint16_t free_psram_size_kb = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
        uint16_t total_psram_size_kb = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024;

        ESP_LOGD(TAG, "Free SRAM: %dKB, Total SRAM: %dKB, Free PSRAM: %dKB, Total PSRAM: %dKB", free_sram_size_kb, total_sram_size_kb, free_psram_size_kb, total_psram_size_kb);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
